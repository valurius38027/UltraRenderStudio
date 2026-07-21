# Debug 构建默认启用 AddressSanitizer + UndefinedBehaviorSanitizer。
# CMakePresets.json 里的 *-debug 预设会把 UR_ENABLE_SANITIZERS 设为 ON。
# MSVC 目前只支持 ASan(无 UBSan),因此 Windows 下只挂 /fsanitize=address。
function(ur_apply_sanitizers target)
    if (NOT UR_ENABLE_SANITIZERS)
        return()
    endif()

    if (MSVC)
        target_compile_options(${target} PUBLIC /fsanitize=address)
    else()
        target_compile_options(${target} PUBLIC
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PUBLIC
            -fsanitize=address,undefined
        )
    endif()
endfunction()
