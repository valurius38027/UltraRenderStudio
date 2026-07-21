#include "ur/gfx/render_device.hpp"

#include "ur/platform/detail/qt_window_access.hpp"
#include "ur/platform/window.hpp"

#include <QFile>
#include <QMatrix4x4>
#include <QVulkanInstance>
#include <QWindow>
#include <private/qrhivulkan_p.h>
#include <rhi/qrhi.h>

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

static void initShaderResourcesGlobal() {
    Q_INIT_RESOURCE(shaders);
}

namespace ur::gfx {
namespace {

constexpr int kFloatsPerRectVertex = 6;
constexpr std::size_t kVerticesPerRect = 6U;
constexpr quint32 kUniformBufferSize = 64U;

QShader loadShader(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("ur_gfx: failed to open baked shader: " + path.toStdString());
    }
    return QShader::fromSerialized(file.readAll());
}

[[nodiscard]] quint32 checkedByteSize(std::size_t elementCount, std::size_t elementSize) {
    constexpr auto kMax = static_cast<std::size_t>(std::numeric_limits<quint32>::max());
    if (elementSize != 0U && elementCount > kMax / elementSize) {
        throw std::overflow_error("ur_gfx: buffer size exceeds QRhi range");
    }
    return static_cast<quint32>(elementCount * elementSize);
}

[[nodiscard]] quint32 checkedVertexCount(std::size_t rectCount) {
    constexpr auto kMax = static_cast<std::size_t>(std::numeric_limits<quint32>::max());
    if (rectCount > kMax / kVerticesPerRect) {
        throw std::overflow_error("ur_gfx: rectangle vertex count exceeds QRhi range");
    }
    return static_cast<quint32>(rectCount * kVerticesPerRect);
}

[[nodiscard]] int checkedQtDimension(std::uint32_t value) {
    constexpr auto kMax = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    if (value > kMax) {
        throw std::overflow_error("ur_gfx: extent exceeds Qt int range");
    }
    return static_cast<int>(value);
}

[[nodiscard]] std::uint32_t checkedExtentDimension(int value) {
    if (value < 0) {
        throw std::runtime_error("ur_gfx: QRhi returned a negative extent");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] QSize checkedQtSize(Extent2D size) {
    return {checkedQtDimension(size.width), checkedQtDimension(size.height)};
}

[[nodiscard]] std::vector<float> buildRectVertices(const std::vector<RectPrimitive>& rects) {
    std::vector<float> vertexData;
    const std::size_t vertexCount = rects.size() * kVerticesPerRect;
    vertexData.reserve(vertexCount * static_cast<std::size_t>(kFloatsPerRectVertex));

    for (const RectPrimitive& rect : rects) {
        const float x0 = rect.x;
        const float y0 = rect.y;
        const float x1 = rect.x + rect.width;
        const float y1 = rect.y + rect.height;
        const std::array<std::array<float, 2>, kVerticesPerRect> corners{{
            {x0, y0}, {x1, y0}, {x1, y1},
            {x0, y0}, {x1, y1}, {x0, y1},
        }};
        for (const auto& corner : corners) {
            vertexData.insert(vertexData.end(),
                              {corner[0], corner[1], rect.r, rect.g, rect.b, rect.a});
        }
    }
    return vertexData;
}

[[nodiscard]] QMatrix4x4 rectProjection(QRhi& rhi, float width, float height) {
    QMatrix4x4 projection;
    projection.ortho(0.0F, width, height, 0.0F, -1.0F, 1.0F);
    return rhi.clipSpaceCorrMatrix() * projection;
}

void configureRectPipeline(QRhiGraphicsPipeline& pipeline, QRhiShaderResourceBindings& srb,
                           QRhiRenderPassDescriptor& renderPassDescriptor) {
    const QShader vertexShader = loadShader(QStringLiteral(":/ur_gfx/shaders/rect.vert.qsb"));
    const QShader fragmentShader = loadShader(QStringLiteral(":/ur_gfx/shaders/rect.frag.qsb"));

    pipeline.setShaderStages({
        {QRhiShaderStage::Vertex, vertexShader},
        {QRhiShaderStage::Fragment, fragmentShader},
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{kFloatsPerRectVertex * static_cast<int>(sizeof(float))}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2U * sizeof(float)},
    });
    pipeline.setVertexInputLayout(inputLayout);
    pipeline.setShaderResourceBindings(&srb);
    pipeline.setRenderPassDescriptor(&renderPassDescriptor);
    if (!pipeline.create()) {
        throw std::runtime_error("ur_gfx: failed to create rectangle pipeline");
    }
}

}  // namespace

class VulkanRenderDevice final : public RenderDevice {
public:
    explicit VulkanRenderDevice(ur::platform::Window* window)
        : platformWindow_(window), qtWindow_(window == nullptr
                                                ? nullptr
                                                : ur::platform::detail::WindowAccess::qtWindow(*window)) {
        initShaderResourcesGlobal();

        instance_.setLayers({});
        if (qtWindow_ != nullptr) {
            instance_.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        }
        if (!instance_.create()) {
            throw std::runtime_error("ur_gfx: failed to create QVulkanInstance");
        }

        if (qtWindow_ != nullptr) {
            qtWindow_->setVulkanInstance(&instance_);
        } else {
            initializeRhi();
        }
    }

    ~VulkanRenderDevice() override {
        windowPipeline_.reset();
        windowSrb_.reset();
        windowUniformBuffer_.reset();
        windowVertexBuffer_.reset();
        swapChainRenderPass_.reset();
        swapChainDepthStencil_.reset();
        swapChain_.reset();
        rhi_.reset();
        if (qtWindow_ != nullptr) {
            qtWindow_->setVulkanInstance(nullptr);
        }
    }

    [[nodiscard]] Backend backend() const override { return Backend::Vulkan; }

    [[nodiscard]] std::string driverName() const override {
        if (rhi_ == nullptr) {
            throw std::logic_error(
                "ur_gfx: windowed driver information is unavailable before first presentation");
        }
        return rhi_->driverInfo().deviceName.toStdString();
    }

    FrameReadback renderRects(const std::vector<RectPrimitive>& rects, Extent2D targetSize) override {
        initShaderResourcesGlobal();
        const QSize textureSize = checkedQtSize(targetSize);
        if (textureSize.isEmpty()) {
            throw std::invalid_argument("ur_gfx: offscreen target size must be non-zero");
        }

        QScopedPointer<QRhiTexture> texture(rhi_->newTexture(
            QRhiTexture::RGBA8, textureSize, 1,
            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        if (!texture->create()) {
            throw std::runtime_error("ur_gfx: failed to create render target texture");
        }

        QScopedPointer<QRhiTextureRenderTarget> renderTarget(
            rhi_->newTextureRenderTarget({QRhiColorAttachment(texture.data())}));
        QScopedPointer<QRhiRenderPassDescriptor> renderPass(
            renderTarget->newCompatibleRenderPassDescriptor());
        renderTarget->setRenderPassDescriptor(renderPass.data());
        if (!renderTarget->create()) {
            throw std::runtime_error("ur_gfx: failed to create render target");
        }

        const std::vector<float> vertexData = buildRectVertices(rects);
        const bool hasGeometry = !rects.empty();

        QScopedPointer<QRhiBuffer> vertexBuffer;
        if (hasGeometry) {
            vertexBuffer.reset(rhi_->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                                checkedByteSize(vertexData.size(), sizeof(float))));
            if (!vertexBuffer->create()) {
                throw std::runtime_error("ur_gfx: failed to create vertex buffer");
            }
        }

        QScopedPointer<QRhiBuffer> uniformBuffer(
            rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize));
        if (!uniformBuffer->create()) {
            throw std::runtime_error("ur_gfx: failed to create uniform buffer");
        }

        QScopedPointer<QRhiShaderResourceBindings> srb(rhi_->newShaderResourceBindings());
        srb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            uniformBuffer.data())});
        if (!srb->create()) {
            throw std::runtime_error("ur_gfx: failed to create shader resource bindings");
        }

        QScopedPointer<QRhiGraphicsPipeline> pipeline(rhi_->newGraphicsPipeline());
        configureRectPipeline(*pipeline, *srb, *renderPass);

        const QMatrix4x4 mvp = rectProjection(*rhi_, static_cast<float>(targetSize.width),
                                              static_cast<float>(targetSize.height));

        QRhiCommandBuffer* commandBuffer = nullptr;
        if (rhi_->beginOffscreenFrame(&commandBuffer) != QRhi::FrameOpSuccess) {
            throw std::runtime_error("ur_gfx: beginOffscreenFrame failed");
        }

        QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();
        if (hasGeometry) {
            updates->uploadStaticBuffer(vertexBuffer.data(), vertexData.data());
        }
        updates->updateDynamicBuffer(uniformBuffer.data(), 0, kUniformBufferSize, mvp.constData());

        commandBuffer->beginPass(renderTarget.data(), QColor(20, 20, 30), {1.0F, 0}, updates);
        if (hasGeometry) {
            recordRectDraw(*commandBuffer, *pipeline, *vertexBuffer,
                           static_cast<float>(textureSize.width()),
                           static_cast<float>(textureSize.height()), checkedVertexCount(rects.size()));
        }

        QRhiReadbackResult readback;
        bool completed = false;
        readback.completed = [&completed] { completed = true; };
        QRhiResourceUpdateBatch* readbackBatch = rhi_->nextResourceUpdateBatch();
        readbackBatch->readBackTexture({texture.data()}, &readback);
        commandBuffer->endPass(readbackBatch);
        rhi_->endOffscreenFrame();

        if (!completed) {
            throw std::runtime_error("ur_gfx: synchronous readback did not complete");
        }

        FrameReadback result;
        result.size = {checkedExtentDimension(readback.pixelSize.width()),
                       checkedExtentDimension(readback.pixelSize.height())};
        result.pixels.assign(readback.data.begin(), readback.data.end());
        return result;
    }

    FrameReadback renderDebugTriangleToTexture(Extent2D size) override {
        const QSize textureSize = checkedQtSize(size);
        if (textureSize.isEmpty()) {
            throw std::invalid_argument("ur_gfx: debug target size must be non-zero");
        }

        QScopedPointer<QRhiTexture> texture(rhi_->newTexture(
            QRhiTexture::RGBA8, textureSize, 1,
            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        if (!texture->create()) {
            throw std::runtime_error("ur_gfx: failed to create render target texture");
        }

        QScopedPointer<QRhiTextureRenderTarget> renderTarget(
            rhi_->newTextureRenderTarget({QRhiColorAttachment(texture.data())}));
        QScopedPointer<QRhiRenderPassDescriptor> renderPass(
            renderTarget->newCompatibleRenderPassDescriptor());
        renderTarget->setRenderPassDescriptor(renderPass.data());
        if (!renderTarget->create()) {
            throw std::runtime_error("ur_gfx: failed to create render target");
        }

        const std::array<float, 15> vertices = {
            0.0F, 0.6F, 1.0F, 0.2F, 0.2F,
            -0.6F, -0.6F, 0.2F, 1.0F, 0.2F,
            0.6F, -0.6F, 0.2F, 0.2F, 1.0F,
        };

        QScopedPointer<QRhiBuffer> vertexBuffer(rhi_->newBuffer(
            QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
            checkedByteSize(vertices.size(), sizeof(float))));
        if (!vertexBuffer->create()) {
            throw std::runtime_error("ur_gfx: failed to create vertex buffer");
        }

        const QShader vertexShader = loadShader(QStringLiteral(":/ur_gfx/shaders/tri.vert.qsb"));
        const QShader fragmentShader = loadShader(QStringLiteral(":/ur_gfx/shaders/tri.frag.qsb"));

        QScopedPointer<QRhiShaderResourceBindings> srb(rhi_->newShaderResourceBindings());
        if (!srb->create()) {
            throw std::runtime_error("ur_gfx: failed to create shader resource bindings");
        }

        QScopedPointer<QRhiGraphicsPipeline> pipeline(rhi_->newGraphicsPipeline());
        pipeline->setShaderStages({
            {QRhiShaderStage::Vertex, vertexShader},
            {QRhiShaderStage::Fragment, fragmentShader},
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{5 * static_cast<int>(sizeof(float))}});
        inputLayout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {0, 1, QRhiVertexInputAttribute::Float3, 2U * sizeof(float)},
        });
        pipeline->setVertexInputLayout(inputLayout);
        pipeline->setShaderResourceBindings(srb.data());
        pipeline->setRenderPassDescriptor(renderPass.data());
        if (!pipeline->create()) {
            throw std::runtime_error("ur_gfx: failed to create pipeline");
        }

        QRhiCommandBuffer* commandBuffer = nullptr;
        if (rhi_->beginOffscreenFrame(&commandBuffer) != QRhi::FrameOpSuccess) {
            throw std::runtime_error("ur_gfx: beginOffscreenFrame failed");
        }

        QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();
        updates->uploadStaticBuffer(vertexBuffer.data(), vertices.data());
        commandBuffer->beginPass(renderTarget.data(), QColor(20, 20, 30), {1.0F, 0}, updates);
        commandBuffer->setGraphicsPipeline(pipeline.data());
        commandBuffer->setViewport(QRhiViewport(0, 0, static_cast<float>(textureSize.width()),
                                                 static_cast<float>(textureSize.height())));
        const QRhiCommandBuffer::VertexInput vertexInput(vertexBuffer.data(), 0);
        commandBuffer->setVertexInput(0, 1, &vertexInput);
        commandBuffer->draw(3);

        QRhiReadbackResult readback;
        bool completed = false;
        readback.completed = [&completed] { completed = true; };
        QRhiResourceUpdateBatch* readbackBatch = rhi_->nextResourceUpdateBatch();
        readbackBatch->readBackTexture({texture.data()}, &readback);
        commandBuffer->endPass(readbackBatch);
        rhi_->endOffscreenFrame();

        if (!completed) {
            throw std::runtime_error("ur_gfx: synchronous readback did not complete");
        }

        FrameReadback result;
        result.size = {checkedExtentDimension(readback.pixelSize.width()),
                       checkedExtentDimension(readback.pixelSize.height())};
        result.pixels.assign(readback.data.begin(), readback.data.end());
        return result;
    }

    void resizeSwapChain() override {
        requireWindowed();
        if (!platformWindow_->isExposed()) {
            swapChainReady_ = false;
            return;
        }
        initializeRhi();
        const QSize surfaceSize = swapChain_->surfacePixelSize();
        if (surfaceSize.isEmpty()) {
            swapChainReady_ = false;
            return;
        }
        if (!swapChain_->createOrResize()) {
            throw std::runtime_error("ur_gfx: failed to create or resize swapchain");
        }
        swapChainReady_ = true;
        ensureWindowPipeline();
    }

    PresentResult presentRects(const std::vector<RectPrimitive>& rects) override {
        requireWindowed();
        if (!platformWindow_->isExposed()) {
            return PresentResult::SkippedNotExposed;
        }
        if (!swapChainReady_) {
            resizeSwapChain();
            if (!swapChainReady_) {
                return PresentResult::SkippedNotExposed;
            }
        }

        for (int attempt = 0; attempt < 2; ++attempt) {
            const QRhi::FrameOpResult beginResult = rhi_->beginFrame(swapChain_.data());
            if (beginResult == QRhi::FrameOpSwapChainOutOfDate) {
                resizeSwapChain();
                continue;
            }
            if (beginResult == QRhi::FrameOpDeviceLost) {
                return PresentResult::DeviceLost;
            }
            if (beginResult != QRhi::FrameOpSuccess) {
                throw std::runtime_error("ur_gfx: beginFrame failed");
            }

            const QSize pixelSize = swapChain_->currentPixelSize();
            const ur::platform::WindowSize logicalSize = platformWindow_->size();
            if (pixelSize.isEmpty() || logicalSize.width == 0U || logicalSize.height == 0U) {
                static_cast<void>(rhi_->endFrame(swapChain_.data(), QRhi::SkipPresent));
                return PresentResult::SkippedNotExposed;
            }

            const std::vector<float> vertexData = buildRectVertices(rects);
            const bool hasGeometry = !rects.empty();
            if (hasGeometry) {
                ensureWindowVertexBuffer(checkedByteSize(vertexData.size(), sizeof(float)));
            }

            const QMatrix4x4 mvp = rectProjection(*rhi_, static_cast<float>(logicalSize.width),
                                                  static_cast<float>(logicalSize.height));
            QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();
            if (hasGeometry) {
                updates->updateDynamicBuffer(windowVertexBuffer_.data(), 0,
                                             checkedByteSize(vertexData.size(), sizeof(float)),
                                             vertexData.data());
            }
            updates->updateDynamicBuffer(windowUniformBuffer_.data(), 0, kUniformBufferSize,
                                         mvp.constData());

            QRhiCommandBuffer* commandBuffer = swapChain_->currentFrameCommandBuffer();
            commandBuffer->beginPass(swapChain_->currentFrameRenderTarget(), QColor(20, 20, 30),
                                     {1.0F, 0}, updates);
            if (hasGeometry) {
                recordRectDraw(*commandBuffer, *windowPipeline_, *windowVertexBuffer_,
                               static_cast<float>(pixelSize.width()),
                               static_cast<float>(pixelSize.height()),
                               checkedVertexCount(rects.size()));
            }
            commandBuffer->endPass();

            const QRhi::FrameOpResult endResult = rhi_->endFrame(swapChain_.data());
            if (endResult == QRhi::FrameOpSwapChainOutOfDate) {
                resizeSwapChain();
                return PresentResult::Resized;
            }
            if (endResult == QRhi::FrameOpDeviceLost) {
                return PresentResult::DeviceLost;
            }
            if (endResult != QRhi::FrameOpSuccess) {
                throw std::runtime_error("ur_gfx: endFrame failed");
            }
            return PresentResult::Presented;
        }

        return PresentResult::Resized;
    }

private:
    static void recordRectDraw(QRhiCommandBuffer& commandBuffer,
                               QRhiGraphicsPipeline& pipeline, QRhiBuffer& vertexBuffer,
                               float viewportWidth, float viewportHeight, quint32 vertexCount) {
        commandBuffer.setGraphicsPipeline(&pipeline);
        commandBuffer.setShaderResources();
        commandBuffer.setViewport(QRhiViewport(0, 0, viewportWidth, viewportHeight));
        const QRhiCommandBuffer::VertexInput vertexInput(&vertexBuffer, 0);
        commandBuffer.setVertexInput(0, 1, &vertexInput);
        commandBuffer.draw(vertexCount);
    }

    void requireWindowed() const {
        if (platformWindow_ == nullptr || qtWindow_ == nullptr) {
            throw std::logic_error("ur_gfx: operation requires a windowed render device");
        }
    }

    void initializeRhi() {
        if (rhi_ != nullptr) {
            return;
        }

        QRhiVulkanInitParams params;
        params.inst = &instance_;
        params.window = qtWindow_;
        rhi_.reset(QRhi::create(QRhi::Vulkan, &params));
        if (rhi_ == nullptr) {
            throw std::runtime_error("ur_gfx: failed to create QRhi Vulkan backend");
        }

        if (qtWindow_ == nullptr) {
            return;
        }

        swapChain_.reset(rhi_->newSwapChain());
        swapChain_->setWindow(qtWindow_);
        swapChainDepthStencil_.reset(rhi_->newRenderBuffer(
            QRhiRenderBuffer::DepthStencil, QSize(), 1,
            QRhiRenderBuffer::UsedWithSwapChainOnly));
        swapChain_->setDepthStencil(swapChainDepthStencil_.data());
        swapChainRenderPass_.reset(swapChain_->newCompatibleRenderPassDescriptor());
        if (swapChainRenderPass_ == nullptr) {
            throw std::runtime_error("ur_gfx: failed to create swapchain render pass descriptor");
        }
        swapChain_->setRenderPassDescriptor(swapChainRenderPass_.data());
    }

    void ensureWindowPipeline() {
        if (windowPipeline_ != nullptr) {
            return;
        }

        windowUniformBuffer_.reset(
            rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize));
        if (!windowUniformBuffer_->create()) {
            throw std::runtime_error("ur_gfx: failed to create window uniform buffer");
        }

        windowSrb_.reset(rhi_->newShaderResourceBindings());
        windowSrb_->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            windowUniformBuffer_.data())});
        if (!windowSrb_->create()) {
            throw std::runtime_error("ur_gfx: failed to create window shader bindings");
        }

        windowPipeline_.reset(rhi_->newGraphicsPipeline());
        configureRectPipeline(*windowPipeline_, *windowSrb_, *swapChainRenderPass_);
    }

    void ensureWindowVertexBuffer(quint32 requiredBytes) {
        if (windowVertexBuffer_ != nullptr && windowVertexCapacityBytes_ >= requiredBytes) {
            return;
        }
        windowVertexBuffer_.reset();
        windowVertexBuffer_.reset(
            rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, requiredBytes));
        if (!windowVertexBuffer_->create()) {
            throw std::runtime_error("ur_gfx: failed to create window vertex buffer");
        }
        windowVertexCapacityBytes_ = requiredBytes;
    }

    QVulkanInstance instance_;
    QScopedPointer<QRhi> rhi_;
    ur::platform::Window* platformWindow_ = nullptr;
    QWindow* qtWindow_ = nullptr;
    QScopedPointer<QRhiSwapChain> swapChain_;
    QScopedPointer<QRhiRenderBuffer> swapChainDepthStencil_;
    QScopedPointer<QRhiRenderPassDescriptor> swapChainRenderPass_;
    QScopedPointer<QRhiBuffer> windowVertexBuffer_;
    quint32 windowVertexCapacityBytes_ = 0U;
    QScopedPointer<QRhiBuffer> windowUniformBuffer_;
    QScopedPointer<QRhiShaderResourceBindings> windowSrb_;
    QScopedPointer<QRhiGraphicsPipeline> windowPipeline_;
    bool swapChainReady_ = false;
};

std::unique_ptr<RenderDevice> RenderDevice::createOffscreen(Backend backend) {
    switch (backend) {
        case Backend::Vulkan:
            return std::make_unique<VulkanRenderDevice>(nullptr);
        case Backend::D3D11:
        case Backend::D3D12:
        case Backend::Metal:
            throw std::runtime_error("ur_gfx: backend not implemented yet");
    }
    throw std::runtime_error("ur_gfx: unknown backend");
}

std::unique_ptr<RenderDevice> RenderDevice::createForWindow(Backend backend,
                                                             ur::platform::Window& window) {
    switch (backend) {
        case Backend::Vulkan:
            return std::make_unique<VulkanRenderDevice>(&window);
        case Backend::D3D11:
        case Backend::D3D12:
        case Backend::Metal:
            throw std::runtime_error("ur_gfx: backend not implemented yet");
    }
    throw std::runtime_error("ur_gfx: unknown backend");
}

}  // namespace ur::gfx
