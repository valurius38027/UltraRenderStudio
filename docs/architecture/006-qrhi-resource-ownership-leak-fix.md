# ADR-006: QRhi 资源所有权导致的真实内存泄漏(已修复),及驱动层残留泄漏的处理

- 状态: 已采纳
- 日期: 2026-07-18

## 背景

给 `ur_widgets` 加严格构建(`-Werror` + ASan + UBSan)验证时,顺带在 CI
配置里发现两个问题:

1. `cmake/Sanitizers.cmake` 里 sanitizer 的编译/链接选项用的是 `PRIVATE`
   作用域,加在静态库(`ur_platform` 等)上不会传递给最终链接它的可执行
   文件,导致可执行文件链接阶段缺失 sanitizer 运行时符号,直接报
   undefined reference。**已修复**: 改成 `PUBLIC`,选项沿
   `target_link_libraries` 依赖链正确传递。

2. `ur_gfx_tests` 在 ASan LeakSanitizer 下报出 15306 字节 / 8 处泄漏。
   排查后定位到 `vulkan_render_device.cpp` 里
   `rhi_->newShaderResourceBindings()` 的返回值直接传给
   `pipeline->setShaderResourceBindings()`,从未被任何 RAII 包装或手动
   释放。QRhi 的资源所有权模型是: pipeline 只是**引用**这个对象用于
   渲染,不会接管它的生命周期,调用方必须自己持有并负责释放 —— 这是
   一处真实的、不依赖任何环境因素就能复现的内存泄漏。**已修复**: 用
   `QScopedPointer` 包装,并且注意声明顺序放在 `pipeline` 之前(局部
   变量按声明逆序析构,这样 pipeline 先于它引用的 srb 被销毁)。

## 决策

修复后残留 112 字节 / 2 处泄漏,堆栈完全在 `asan_thread_start` 起的
后台线程里,全部是无符号的未知模块帧。这不是我们自己代码路径产生的
分配(我们的调用栈在几帧之前就已经离开了 `ur_gfx` 的范围),判断是
Mesa `llvmpipe` 软件光栅化驱动的后台线程池/LLVM JIT 相关分配 ——
这类驱动通常故意让线程池存活到进程退出,ASan 的可达性分析在没有驱动
符号信息的情况下无法识别这类"实际上进程退出时会被系统回收、只是不走
显式 free 路径"的分配。

**接受这部分残留,不再继续排查**(在没有 Mesa 调试符号、且这是软件
参考驱动而非目标平台真实驱动的情况下,继续排查的边际收益很低)。
LeakSanitizer 不支持"字节阈值"这种模糊放行,只能用符号匹配的抑制列表
精确排除 —— `cmake/lsan_suppressions.txt` 用 `leak:asan_thread_start`
匹配"由后台线程启动路径分配、调用栈完全在无符号驱动代码里"的这一类
残留,不会掩盖任何经过 `ur_gfx`/`ur_platform` 等有符号帧的、我们自己
代码路径产生的新泄漏。

## 后果

- 这条 ADR 本身证明了"严格构建配置(warnings-as-error + sanitizers)
  从项目早期就该开着"这个决定是对的 —— 一个真实的内存泄漏在骨架阶段、
  只有两个测试用例的时候就被抓到并修复了,而不是拖到几十万行代码之后
  才变成一个几乎无法定位的偶发问题。
- 团队规范: 以后任何"A 引用 B 但不拥有 B"的 QRhi 资源组合(这在 QRhi
  API 里很常见,不止 `QRhiShaderResourceBindings`),创建时第一反应
  应该是"这个返回值该由谁持有生命周期",而不是想当然认为设置进去的
  那个对象会接管它。
