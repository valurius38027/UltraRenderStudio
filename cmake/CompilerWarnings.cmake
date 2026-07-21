# 统一各模块的编译警告等级。任何 libs/* 或 apps/* target 都应调用
# ur_set_warnings(<target>) 来启用这套规则,而不是各自零散设置。
function(ur_set_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /w14640   # 线程不安全的静态成员初始化
            $<$<BOOL:${UR_WARNINGS_AS_ERRORS}>:/WX>
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            $<$<BOOL:${UR_WARNINGS_AS_ERRORS}>:-Werror>
        )
    endif()
endfunction()
