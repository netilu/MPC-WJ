# 左键长按倍速崩溃排查（2026-09-20）

## 已确认的证据

Windows 应用程序事件 1000/1001 和两个本机转储一致：

| 崩溃时间 | 转储 | 程序版本 | 异常 | 故障模块和偏移 |
| --- | --- | --- | --- | --- |
| 22:21:54 | mpc-be64.exe.15068.dmp | 1.9.1.0 | 0xc0000005 | igd9trinity64.dll + 0xafe7b1 |
| 22:23:18 | mpc-be64.exe.20332.dmp | 1.9.1.0 | 0xc0000005 | igd9trinity64.dll + 0xafd955 |

两次驱动文件版本均为 **32.0.101.8424**。应用路径为用户安装目录中的 `mpc-be64.exe`，不是新版本号的 1.9.2.1；1.9.2.1 的构建也没有包含针对本问题的修复。

采用 Windows SDK DbgHelp 和本机匹配时间戳/映像大小的 PE 异常表离线展开，两次崩溃工作线程共享如下调用路径（从调用方到故障点）：

```text
mpc-be64.exe + 0x12b33ad
mpc-be64.exe + 0x12b3496
mpc-be64.exe + 0x12b1adf
evr.dll + 0x488e / 0x4cd5 / 0x71d6
dxva2.dll + 0x1a9e
d3d9.dll + 0x3eec0 / 0x3f64a
igd9dxva64.dll
igd9trinity64.dll → 访问冲突
```

这将故障范围定位到 **EVR / DXVA2 / D3D9 的视频处理路径**。不是鼠标事件处理函数本身直接抛出异常。没有旧 EXE 对应的完整 PDB，因此没有强行将旧程序地址套用到新程序符号；仅凭栈不能确定驱动自身缺陷或应用向驱动提供了不正确的资源/时序，也不能据此断言关闭解码器硬件加速一定有效。

## 源码中的相关行为

- `MainFrm.cpp` 的 `TIMER_MOUSE_LEFT_LONGPRESS_SPEED` 调用 `SetPlayingRate()` 进入倍速；松开时 `CancelLeftLongPressSpeed(true)` 恢复原速。
- `OnMouseMove()` 对候选状态和已进入加速状态使用相同拖动阈值；进入加速后，鼠标移动超过 `SM_CXDRAG/SM_CYDRAG` 就立即恢复原速。这可能产生很短的倍速区间，但一次长按不会因此无限重复加速。
- 当前长按状态只在部分鼠标事件中清理，没有在失去应用焦点和媒体关闭流程中统一清理；这是独立的边界问题，尚无证据证明它就是两次驱动崩溃的原因。
- `SetPlayingRate()` 会在媒体不是运行状态时先发播放命令，再调用 `IMediaSeeking::SetRate()`。
- `EVRAllocatorPresenter.cpp` 的 `GetImageFromMixer()` 调用 `m_pMixer->ProcessOutput()`，与转储中的 EVR 视频处理路径相符；此处属于源码推断，未利用不匹配 PDB 断言具体调用行。

## 下一步用于区分原因的对照

固定同一视频、时间区间和倍速值，比较左键长按与菜单调整倍速。前者独有的崩溃更支持长按状态/切换时机问题；两者都崩溃则更支持通用倍速和视频处理路径问题。

随后保持输入和倍速操作相同，对比当前 EVR 路径与其他渲染路径。仅做软件解码对照不能完全排除 EVR 使用 GPU 视频处理的影响。以上对照尚未执行，不能将其写成已验证的修复。

本轮仅新增离线诊断工具和报告，没有修改播放器运行逻辑、用户播放器设置或显卡驱动，也没有发布新的修复程序。

## 本地重复分析

```bat
tests\inspect-crash-dump.cmd "C:\Users\cwj\AppData\Local\CrashDumps\mpc-be64.exe.20332.dmp"
```

工具使用本机 VS 2022 Enterprise 编译到已忽略的 `tests/player` 目录，只读取指定转储和匹配的本机模块，不附加进程、不请求符号服务器。适用于本次完整且可信的本机转储，不作为任意不可信转储文件的通用解析器。

诊断输出保存在 `tests/player/crash-15068.txt` 和 `tests/player/crash-20332.txt`，转储原文件未修改。
