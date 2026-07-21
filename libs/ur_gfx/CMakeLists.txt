find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(Vulkan REQUIRED)

qt6_add_resources(UR_GFX_QRC_SOURCES src/shaders/shaders.qrc)

add_library(ur_gfx STATIC
    src/vulkan_render_device.cpp
    ${UR_GFX_QRC_SOURCES}
)
add_library(ur::gfx ALIAS ur_gfx)

target_include_directories(ur_gfx
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_gfx
    PUBLIC Qt6::Core Qt6::Gui
    PUBLIC ur::platform
    PRIVATE Qt6::GuiPrivate Vulkan::Vulkan
)

# QRhi remains a Qt private API, but Qt 6.8 exports the versioned private
# include paths through Qt6::GuiPrivate. Imported-target includes are treated
# as system headers, so project warnings-as-errors do not apply to Qt internals.

ur_set_warnings(ur_gfx)
ur_apply_sanitizers(ur_gfx)

if (UR_BUILD_TESTS)
    add_executable(ur_gfx_tests tests/render_device_test.cpp)
    target_link_libraries(ur_gfx_tests PRIVATE ur::gfx GTest::gtest)
    ur_set_warnings(ur_gfx_tests)
    add_test(NAME ur_gfx_tests COMMAND ur_gfx_tests)
    # 见 docs/architecture/006-qrhi-resource-ownership-leak-fix.md ——
    # 排查后确认残留泄漏是 Mesa llvmpipe 驱动后台线程池的已知行为,
    # 用符号精确匹配的抑制列表排除,不是笼统关闭 leak detection。
    set_tests_properties(ur_gfx_tests PROPERTIES
        ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan_suppressions.txt"
    )
endif()
