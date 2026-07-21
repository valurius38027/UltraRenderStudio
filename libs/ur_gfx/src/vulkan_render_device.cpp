#include "ur/gfx/render_device.hpp"

#include "ur/platform/detail/qt_window_access.hpp"
#include "ur/platform/window.hpp"

#include <QFile>
#include <QMatrix4x4>
#include <QVulkanInstance>
#include <QWindow>
#include <private/qrhivulkan_p.h>
#include <rhi/qrhi.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

static void initShaderResourcesGlobal() {
    Q_INIT_RESOURCE(shaders);
}

namespace ur::gfx {
namespace {

constexpr int kFloatsPerRectVertex = 6;
constexpr int kFloatsPerGlyphVertex = 8;
constexpr std::size_t kVerticesPerQuad = 6U;
constexpr quint32 kUniformBufferSize = 64U;

enum class DrawKind { Rect, Masked };

struct DrawBatch {
    DrawKind kind = DrawKind::Rect;
    quint32 firstVertex = 0U;
    quint32 vertexCount = 0U;
};

struct UiGeometry {
    std::vector<float> rectVertices;
    std::vector<float> glyphVertices;
    std::vector<DrawBatch> batches;
};

QShader loadShader(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("ur_gfx: failed to open baked shader: " + path.toStdString());
    }
    const QShader shader = QShader::fromSerialized(file.readAll());
    if (!shader.isValid()) {
        throw std::runtime_error("ur_gfx: invalid baked shader: " + path.toStdString());
    }
    return shader;
}

[[nodiscard]] quint32 checkedByteSize(std::size_t elementCount, std::size_t elementSize) {
    constexpr auto kMax = static_cast<std::size_t>(std::numeric_limits<quint32>::max());
    if (elementSize != 0U && elementCount > kMax / elementSize) {
        throw std::overflow_error("ur_gfx: buffer size exceeds QRhi range");
    }
    return static_cast<quint32>(elementCount * elementSize);
}

[[nodiscard]] quint32 checkedVertexOffset(std::size_t floatCount, std::size_t floatsPerVertex) {
    if (floatsPerVertex == 0U || floatCount % floatsPerVertex != 0U) {
        throw std::logic_error("ur_gfx: invalid vertex layout accounting");
    }
    const std::size_t vertices = floatCount / floatsPerVertex;
    if (vertices > static_cast<std::size_t>(std::numeric_limits<quint32>::max())) {
        throw std::overflow_error("ur_gfx: vertex offset exceeds QRhi range");
    }
    return static_cast<quint32>(vertices);
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

[[nodiscard]] std::size_t checkedAtlasArea(Extent2D size) {
    if (size.width == 0U || size.height == 0U) {
        throw std::invalid_argument("ur_gfx: alpha atlas dimensions must be non-zero");
    }
    if (static_cast<std::size_t>(size.width) >
        std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(size.height)) {
        throw std::overflow_error("ur_gfx: alpha atlas dimensions overflow addressable memory");
    }
    return static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height);
}

[[nodiscard]] const AlphaAtlasView* validateFrame(const UiFrame& frame) {
    const AlphaAtlasView* atlas = nullptr;
    if (frame.alphaAtlas.has_value()) {
        atlas = &*frame.alphaAtlas;
        if (atlas->pixels.size() != checkedAtlasArea(atlas->size)) {
            throw std::invalid_argument("ur_gfx: alpha atlas pixel count does not match its extent");
        }
    }

    for (const UiPrimitive& primitive : frame.primitives) {
        const auto* quad = std::get_if<MaskedQuadPrimitive>(&primitive);
        if (quad == nullptr) {
            continue;
        }
        if (atlas == nullptr) {
            throw std::invalid_argument("ur_gfx: masked quad requires an alpha atlas");
        }
        if (quad->width < 0.0F || quad->height < 0.0F ||
            quad->atlasWidth == 0U || quad->atlasHeight == 0U) {
            throw std::invalid_argument("ur_gfx: masked quad dimensions are invalid");
        }
        const std::uint64_t right = static_cast<std::uint64_t>(quad->atlasX) + quad->atlasWidth;
        const std::uint64_t bottom = static_cast<std::uint64_t>(quad->atlasY) + quad->atlasHeight;
        if (right > atlas->size.width || bottom > atlas->size.height) {
            throw std::invalid_argument("ur_gfx: masked quad exceeds alpha atlas bounds");
        }
    }
    return atlas;
}

void appendBatch(std::vector<DrawBatch>& batches, DrawKind kind, quint32 firstVertex) {
    if (!batches.empty() && batches.back().kind == kind &&
        batches.back().firstVertex + batches.back().vertexCount == firstVertex) {
        batches.back().vertexCount += static_cast<quint32>(kVerticesPerQuad);
        return;
    }
    batches.push_back(DrawBatch{kind, firstVertex, static_cast<quint32>(kVerticesPerQuad)});
}

void appendRect(UiGeometry& geometry, const RectPrimitive& rect) {
    if (rect.width <= 0.0F || rect.height <= 0.0F) {
        return;
    }
    const quint32 first = checkedVertexOffset(
        geometry.rectVertices.size(), static_cast<std::size_t>(kFloatsPerRectVertex));
    const float x0 = rect.x;
    const float y0 = rect.y;
    const float x1 = rect.x + rect.width;
    const float y1 = rect.y + rect.height;
    const std::array<std::array<float, 2>, kVerticesPerQuad> corners{{
        {x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1},
    }};
    for (const auto& corner : corners) {
        geometry.rectVertices.insert(geometry.rectVertices.end(),
                                     {corner[0], corner[1], rect.r, rect.g, rect.b, rect.a});
    }
    appendBatch(geometry.batches, DrawKind::Rect, first);
}

void appendMasked(UiGeometry& geometry, const MaskedQuadPrimitive& quad,
                  const AlphaAtlasView& atlas) {
    if (quad.width == 0.0F || quad.height == 0.0F) {
        return;
    }
    const quint32 first = checkedVertexOffset(
        geometry.glyphVertices.size(), static_cast<std::size_t>(kFloatsPerGlyphVertex));
    const float x0 = quad.x;
    const float y0 = quad.y;
    const float x1 = quad.x + quad.width;
    const float y1 = quad.y + quad.height;
    const float atlasWidth = static_cast<float>(atlas.size.width);
    const float atlasHeight = static_cast<float>(atlas.size.height);
    const float u0 = static_cast<float>(quad.atlasX) / atlasWidth;
    const float v0 = static_cast<float>(quad.atlasY) / atlasHeight;
    const float u1 = static_cast<float>(quad.atlasX + quad.atlasWidth) / atlasWidth;
    const float v1 = static_cast<float>(quad.atlasY + quad.atlasHeight) / atlasHeight;
    const std::array<std::array<float, 4>, kVerticesPerQuad> vertices{{
        {x0, y0, u0, v0}, {x1, y0, u1, v0}, {x1, y1, u1, v1},
        {x0, y0, u0, v0}, {x1, y1, u1, v1}, {x0, y1, u0, v1},
    }};
    for (const auto& vertex : vertices) {
        geometry.glyphVertices.insert(geometry.glyphVertices.end(),
                                      {vertex[0], vertex[1], vertex[2], vertex[3],
                                       quad.r, quad.g, quad.b, quad.a});
    }
    appendBatch(geometry.batches, DrawKind::Masked, first);
}

[[nodiscard]] UiGeometry buildGeometry(const UiFrame& frame, const AlphaAtlasView* atlas) {
    UiGeometry geometry;
    geometry.batches.reserve(frame.primitives.size());
    for (const UiPrimitive& primitive : frame.primitives) {
        if (const auto* rect = std::get_if<RectPrimitive>(&primitive)) {
            appendRect(geometry, *rect);
        } else {
            if (atlas == nullptr) {
                throw std::logic_error("ur_gfx: validated masked frame lost its atlas");
            }
            appendMasked(geometry, std::get<MaskedQuadPrimitive>(primitive), *atlas);
        }
    }
    return geometry;
}

[[nodiscard]] QMatrix4x4 uiProjection(QRhi& rhi, float width, float height) {
    QMatrix4x4 projection;
    projection.ortho(0.0F, width, height, 0.0F, -1.0F, 1.0F);
    return rhi.clipSpaceCorrMatrix() * projection;
}

void configureRectPipeline(QRhiGraphicsPipeline& pipeline, QRhiShaderResourceBindings& srb,
                           QRhiRenderPassDescriptor& renderPassDescriptor) {
    pipeline.setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/ur_gfx/shaders/rect.vert.qsb"))},
        {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/ur_gfx/shaders/rect.frag.qsb"))},
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

void configureGlyphPipeline(QRhiGraphicsPipeline& pipeline, QRhiShaderResourceBindings& srb,
                            QRhiRenderPassDescriptor& renderPassDescriptor) {
    pipeline.setShaderStages({
        {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/ur_gfx/shaders/glyph.vert.qsb"))},
        {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/ur_gfx/shaders/glyph.frag.qsb"))},
    });
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{kFloatsPerGlyphVertex * static_cast<int>(sizeof(float))}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 2U * sizeof(float)},
        {0, 2, QRhiVertexInputAttribute::Float4, 4U * sizeof(float)},
    });
    pipeline.setVertexInputLayout(inputLayout);
    pipeline.setShaderResourceBindings(&srb);
    pipeline.setRenderPassDescriptor(&renderPassDescriptor);
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    pipeline.setTargetBlends({blend});
    if (!pipeline.create()) {
        throw std::runtime_error("ur_gfx: failed to create masked-quad pipeline");
    }
}

[[nodiscard]] QRhiTextureUploadDescription atlasUpload(const AlphaAtlasView& atlas) {
    QRhiTextureSubresourceUploadDescription subresource(
        atlas.pixels.data(), checkedByteSize(atlas.pixels.size(), sizeof(std::uint8_t)));
    subresource.setDataStride(checkedByteSize(atlas.size.width, sizeof(std::uint8_t)));
    return QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, subresource));
}

void recordBatches(QRhiCommandBuffer& commandBuffer, const UiGeometry& geometry,
                   QRhiGraphicsPipeline* rectPipeline, QRhiShaderResourceBindings* rectSrb,
                   QRhiBuffer* rectBuffer, QRhiGraphicsPipeline* glyphPipeline,
                   QRhiShaderResourceBindings* glyphSrb, QRhiBuffer* glyphBuffer,
                   float viewportWidth, float viewportHeight) {
    for (const DrawBatch& batch : geometry.batches) {
        if (batch.kind == DrawKind::Rect) {
            if (rectPipeline == nullptr || rectSrb == nullptr || rectBuffer == nullptr) {
                throw std::logic_error("ur_gfx: rectangle batch has no GPU resources");
            }
            commandBuffer.setGraphicsPipeline(rectPipeline);
            commandBuffer.setShaderResources(rectSrb);
            commandBuffer.setViewport(QRhiViewport(0, 0, viewportWidth, viewportHeight));
            const QRhiCommandBuffer::VertexInput input(rectBuffer, 0U);
            commandBuffer.setVertexInput(0, 1, &input);
        } else {
            if (glyphPipeline == nullptr || glyphSrb == nullptr || glyphBuffer == nullptr) {
                throw std::logic_error("ur_gfx: masked batch has no GPU resources");
            }
            commandBuffer.setGraphicsPipeline(glyphPipeline);
            commandBuffer.setShaderResources(glyphSrb);
            commandBuffer.setViewport(QRhiViewport(0, 0, viewportWidth, viewportHeight));
            const QRhiCommandBuffer::VertexInput input(glyphBuffer, 0U);
            commandBuffer.setVertexInput(0, 1, &input);
        }
        commandBuffer.draw(batch.vertexCount, 1U, batch.firstVertex);
    }
}

}  // namespace

class VulkanRenderDevice final : public RenderDevice {
public:
    explicit VulkanRenderDevice(ur::platform::Window* window)
        : platformWindow_(window),
          qtWindow_(window == nullptr ? nullptr
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
        resetWindowUiResources();
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

    [[nodiscard]] RenderStatistics statistics() const override { return statistics_; }

    FrameReadback renderUiFrame(const UiFrame& frame, Extent2D targetSize) override {
        const AlphaAtlasView* atlas = validateFrame(frame);
        const UiGeometry geometry = buildGeometry(frame, atlas);
        const QSize textureSize = checkedQtSize(targetSize);
        if (textureSize.isEmpty()) {
            throw std::invalid_argument("ur_gfx: offscreen target size must be non-zero");
        }

        QScopedPointer<QRhiTexture> target(rhi_->newTexture(
            QRhiTexture::RGBA8, textureSize, 1,
            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
        if (!target->create()) {
            throw std::runtime_error("ur_gfx: failed to create render target texture");
        }
        QScopedPointer<QRhiTextureRenderTarget> renderTarget(
            rhi_->newTextureRenderTarget({QRhiColorAttachment(target.data())}));
        QScopedPointer<QRhiRenderPassDescriptor> renderPass(
            renderTarget->newCompatibleRenderPassDescriptor());
        renderTarget->setRenderPassDescriptor(renderPass.data());
        if (!renderTarget->create()) {
            throw std::runtime_error("ur_gfx: failed to create render target");
        }

        QScopedPointer<QRhiBuffer> uniform(
            rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize));
        if (!uniform->create()) {
            throw std::runtime_error("ur_gfx: failed to create UI uniform buffer");
        }
        QScopedPointer<QRhiShaderResourceBindings> rectSrb(rhi_->newShaderResourceBindings());
        rectSrb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            uniform.data())});
        if (!rectSrb->create()) {
            throw std::runtime_error("ur_gfx: failed to create rectangle bindings");
        }
        QScopedPointer<QRhiGraphicsPipeline> rectPipeline(rhi_->newGraphicsPipeline());
        configureRectPipeline(*rectPipeline, *rectSrb, *renderPass);

        QScopedPointer<QRhiBuffer> rectBuffer;
        if (!geometry.rectVertices.empty()) {
            rectBuffer.reset(rhi_->newBuffer(
                QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                checkedByteSize(geometry.rectVertices.size(), sizeof(float))));
            if (!rectBuffer->create()) {
                throw std::runtime_error("ur_gfx: failed to create rectangle vertex buffer");
            }
        }

        QScopedPointer<QRhiTexture> atlasTexture;
        QScopedPointer<QRhiSampler> atlasSampler;
        QScopedPointer<QRhiShaderResourceBindings> glyphSrb;
        QScopedPointer<QRhiGraphicsPipeline> glyphPipeline;
        QScopedPointer<QRhiBuffer> glyphBuffer;
        if (!geometry.glyphVertices.empty()) {
            if (atlas == nullptr) {
                throw std::logic_error("ur_gfx: glyph geometry has no atlas");
            }
            atlasTexture.reset(rhi_->newTexture(QRhiTexture::R8, checkedQtSize(atlas->size), 1));
            if (!atlasTexture->create()) {
                throw std::runtime_error("ur_gfx: failed to create alpha atlas texture");
            }
            atlasSampler.reset(rhi_->newSampler(
                QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
            if (!atlasSampler->create()) {
                throw std::runtime_error("ur_gfx: failed to create alpha atlas sampler");
            }
            glyphSrb.reset(rhi_->newShaderResourceBindings());
            glyphSrb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage, uniform.data()),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage,
                    atlasTexture.data(), atlasSampler.data()),
            });
            if (!glyphSrb->create()) {
                throw std::runtime_error("ur_gfx: failed to create glyph bindings");
            }
            glyphPipeline.reset(rhi_->newGraphicsPipeline());
            configureGlyphPipeline(*glyphPipeline, *glyphSrb, *renderPass);
            glyphBuffer.reset(rhi_->newBuffer(
                QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                checkedByteSize(geometry.glyphVertices.size(), sizeof(float))));
            if (!glyphBuffer->create()) {
                throw std::runtime_error("ur_gfx: failed to create glyph vertex buffer");
            }
        }

        const QMatrix4x4 mvp = uiProjection(
            *rhi_, static_cast<float>(targetSize.width), static_cast<float>(targetSize.height));
        QRhiCommandBuffer* commandBuffer = nullptr;
        if (rhi_->beginOffscreenFrame(&commandBuffer) != QRhi::FrameOpSuccess) {
            throw std::runtime_error("ur_gfx: beginOffscreenFrame failed");
        }
        QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();
        if (rectBuffer != nullptr) {
            updates->uploadStaticBuffer(rectBuffer.data(), geometry.rectVertices.data());
        }
        if (glyphBuffer != nullptr) {
            updates->uploadStaticBuffer(glyphBuffer.data(), geometry.glyphVertices.data());
            updates->uploadTexture(atlasTexture.data(), atlasUpload(*atlas));
            ++statistics_.alphaAtlasUploadCount;
        }
        updates->updateDynamicBuffer(uniform.data(), 0, kUniformBufferSize, mvp.constData());

        commandBuffer->beginPass(renderTarget.data(), QColor(20, 20, 30), {1.0F, 0}, updates);
        recordBatches(*commandBuffer, geometry, rectPipeline.data(), rectSrb.data(),
                      rectBuffer.data(), glyphPipeline.data(), glyphSrb.data(), glyphBuffer.data(),
                      static_cast<float>(textureSize.width()),
                      static_cast<float>(textureSize.height()));

        QRhiReadbackResult readback;
        bool completed = false;
        readback.completed = [&completed] { completed = true; };
        QRhiResourceUpdateBatch* readbackBatch = rhi_->nextResourceUpdateBatch();
        readbackBatch->readBackTexture({target.data()}, &readback);
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
        QScopedPointer<QRhiShaderResourceBindings> srb(rhi_->newShaderResourceBindings());
        if (!srb->create()) {
            throw std::runtime_error("ur_gfx: failed to create shader resource bindings");
        }
        QScopedPointer<QRhiGraphicsPipeline> pipeline(rhi_->newGraphicsPipeline());
        pipeline->setShaderStages({
            {QRhiShaderStage::Vertex, loadShader(QStringLiteral(":/ur_gfx/shaders/tri.vert.qsb"))},
            {QRhiShaderStage::Fragment, loadShader(QStringLiteral(":/ur_gfx/shaders/tri.frag.qsb"))},
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
        const QRhiCommandBuffer::VertexInput input(vertexBuffer.data(), 0U);
        commandBuffer->setVertexInput(0, 1, &input);
        commandBuffer->draw(3U);
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
        ensureWindowBaseResources();
    }

    PresentResult presentUiFrame(const UiFrame& frame) override {
        requireWindowed();
        const AlphaAtlasView* atlas = validateFrame(frame);
        const UiGeometry geometry = buildGeometry(frame, atlas);
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

            ensureWindowBaseResources();
            if (!geometry.rectVertices.empty()) {
                ensureDynamicBuffer(windowRectBuffer_, windowRectCapacity_,
                                    checkedByteSize(geometry.rectVertices.size(), sizeof(float)),
                                    "rectangle");
            }
            if (!geometry.glyphVertices.empty()) {
                ensureWindowAtlas(*atlas);
                ensureDynamicBuffer(windowGlyphBuffer_, windowGlyphCapacity_,
                                    checkedByteSize(geometry.glyphVertices.size(), sizeof(float)),
                                    "glyph");
            }

            const QMatrix4x4 mvp = uiProjection(
                *rhi_, static_cast<float>(logicalSize.width), static_cast<float>(logicalSize.height));
            QRhiResourceUpdateBatch* updates = rhi_->nextResourceUpdateBatch();
            if (!geometry.rectVertices.empty()) {
                updates->updateDynamicBuffer(
                    windowRectBuffer_.data(), 0,
                    checkedByteSize(geometry.rectVertices.size(), sizeof(float)),
                    geometry.rectVertices.data());
            }
            if (!geometry.glyphVertices.empty()) {
                updates->updateDynamicBuffer(
                    windowGlyphBuffer_.data(), 0,
                    checkedByteSize(geometry.glyphVertices.size(), sizeof(float)),
                    geometry.glyphVertices.data());
                if (windowAtlasNeedsUpload_) {
                    updates->uploadTexture(windowAtlasTexture_.data(), atlasUpload(*atlas));
                    windowUploadedAtlasRevision_ = atlas->revision;
                    windowAtlasNeedsUpload_ = false;
                    ++statistics_.alphaAtlasUploadCount;
                }
            }
            updates->updateDynamicBuffer(
                windowUniform_.data(), 0, kUniformBufferSize, mvp.constData());

            QRhiCommandBuffer* commandBuffer = swapChain_->currentFrameCommandBuffer();
            commandBuffer->beginPass(swapChain_->currentFrameRenderTarget(), QColor(20, 20, 30),
                                     {1.0F, 0}, updates);
            recordBatches(*commandBuffer, geometry,
                          windowRectPipeline_.data(), windowRectSrb_.data(), windowRectBuffer_.data(),
                          windowGlyphPipeline_.data(), windowGlyphSrb_.data(),
                          windowGlyphBuffer_.data(),
                          static_cast<float>(pixelSize.width()),
                          static_cast<float>(pixelSize.height()));
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

    void ensureWindowBaseResources() {
        if (windowUniform_ == nullptr) {
            windowUniform_.reset(
                rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize));
            if (!windowUniform_->create()) {
                throw std::runtime_error("ur_gfx: failed to create window uniform buffer");
            }
        }
        if (windowRectPipeline_ == nullptr) {
            windowRectSrb_.reset(rhi_->newShaderResourceBindings());
            windowRectSrb_->setBindings({QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage |
                       QRhiShaderResourceBinding::FragmentStage,
                windowUniform_.data())});
            if (!windowRectSrb_->create()) {
                throw std::runtime_error("ur_gfx: failed to create window rectangle bindings");
            }
            windowRectPipeline_.reset(rhi_->newGraphicsPipeline());
            configureRectPipeline(*windowRectPipeline_, *windowRectSrb_, *swapChainRenderPass_);
        }
    }

    void ensureWindowAtlas(const AlphaAtlasView& atlas) {
        const QSize requiredSize = checkedQtSize(atlas.size);
        if (windowAtlasTexture_ == nullptr || windowAtlasSize_ != atlas.size) {
            windowGlyphPipeline_.reset();
            windowGlyphSrb_.reset();
            windowAtlasSampler_.reset();
            windowAtlasTexture_.reset();
            windowAtlasTexture_.reset(rhi_->newTexture(QRhiTexture::R8, requiredSize, 1));
            if (!windowAtlasTexture_->create()) {
                throw std::runtime_error("ur_gfx: failed to create window alpha atlas");
            }
            windowAtlasSampler_.reset(rhi_->newSampler(
                QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
            if (!windowAtlasSampler_->create()) {
                throw std::runtime_error("ur_gfx: failed to create window atlas sampler");
            }
            windowGlyphSrb_.reset(rhi_->newShaderResourceBindings());
            windowGlyphSrb_->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage, windowUniform_.data()),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage,
                    windowAtlasTexture_.data(), windowAtlasSampler_.data()),
            });
            if (!windowGlyphSrb_->create()) {
                throw std::runtime_error("ur_gfx: failed to create window glyph bindings");
            }
            windowGlyphPipeline_.reset(rhi_->newGraphicsPipeline());
            configureGlyphPipeline(
                *windowGlyphPipeline_, *windowGlyphSrb_, *swapChainRenderPass_);
            windowAtlasSize_ = atlas.size;
            windowUploadedAtlasRevision_ = std::numeric_limits<std::uint64_t>::max();
            windowAtlasNeedsUpload_ = true;
        }
        if (windowUploadedAtlasRevision_ != atlas.revision) {
            windowAtlasNeedsUpload_ = true;
        }
    }

    void ensureDynamicBuffer(QScopedPointer<QRhiBuffer>& buffer, quint32& capacity,
                             quint32 requiredBytes, const char* label) {
        if (buffer != nullptr && capacity >= requiredBytes) {
            return;
        }
        buffer.reset();
        buffer.reset(rhi_->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, requiredBytes));
        if (!buffer->create()) {
            throw std::runtime_error(std::string("ur_gfx: failed to create window ") + label +
                                     " vertex buffer");
        }
        capacity = requiredBytes;
    }

    void resetWindowUiResources() {
        windowGlyphPipeline_.reset();
        windowGlyphSrb_.reset();
        windowAtlasSampler_.reset();
        windowAtlasTexture_.reset();
        windowRectPipeline_.reset();
        windowRectSrb_.reset();
        windowUniform_.reset();
        windowGlyphBuffer_.reset();
        windowRectBuffer_.reset();
    }

    QVulkanInstance instance_;
    QScopedPointer<QRhi> rhi_;
    ur::platform::Window* platformWindow_ = nullptr;
    QWindow* qtWindow_ = nullptr;
    QScopedPointer<QRhiSwapChain> swapChain_;
    QScopedPointer<QRhiRenderBuffer> swapChainDepthStencil_;
    QScopedPointer<QRhiRenderPassDescriptor> swapChainRenderPass_;

    QScopedPointer<QRhiBuffer> windowUniform_;
    QScopedPointer<QRhiBuffer> windowRectBuffer_;
    quint32 windowRectCapacity_ = 0U;
    QScopedPointer<QRhiShaderResourceBindings> windowRectSrb_;
    QScopedPointer<QRhiGraphicsPipeline> windowRectPipeline_;

    QScopedPointer<QRhiBuffer> windowGlyphBuffer_;
    quint32 windowGlyphCapacity_ = 0U;
    QScopedPointer<QRhiTexture> windowAtlasTexture_;
    QScopedPointer<QRhiSampler> windowAtlasSampler_;
    QScopedPointer<QRhiShaderResourceBindings> windowGlyphSrb_;
    QScopedPointer<QRhiGraphicsPipeline> windowGlyphPipeline_;
    Extent2D windowAtlasSize_;
    std::uint64_t windowUploadedAtlasRevision_ = std::numeric_limits<std::uint64_t>::max();
    bool windowAtlasNeedsUpload_ = false;

    bool swapChainReady_ = false;
    RenderStatistics statistics_;
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

FrameReadback RenderDevice::renderRects(const std::vector<RectPrimitive>& rects,
                                        Extent2D targetSize) {
    UiFrame frame;
    frame.primitives.reserve(rects.size());
    for (const RectPrimitive& rect : rects) {
        frame.primitives.emplace_back(rect);
    }
    return renderUiFrame(frame, targetSize);
}

PresentResult RenderDevice::presentRects(const std::vector<RectPrimitive>& rects) {
    UiFrame frame;
    frame.primitives.reserve(rects.size());
    for (const RectPrimitive& rect : rects) {
        frame.primitives.emplace_back(rect);
    }
    return presentUiFrame(frame);
}

}  // namespace ur::gfx
