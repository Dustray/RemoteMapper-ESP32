# ESP32-S3 USB UAC 1.0 Isochronous IN Boundary Crossing Bug & Postmortem

**Date**: 2026-08-29  
**Platform**: ESP32-S3 (Synopsys DWC2 USB Controller)  
**Framework**: Arduino-ESP32 (TinyUSB)  

## 1. 现象描述 (The Symptoms)
在将小米蓝牙语音遥控器的 16kHz BLE 音频通过 ESP32-S3 转发为 USB UAC 1.0 麦克风时，出现了以下严重问题：
* **音频变慢且伴随严重锯齿/电流声**：音调（Pitch）正常，但播放速度慢了一倍，且声音断断续续。
* **Ring Buffer 溢出**：BLE 稳定以 16000 samples/sec 的速度写入音频流，但 USB IN Endpoint 只能以约 8000 samples/sec 的速度拉取数据，导致内存缓冲区积压崩溃。

## 2. 根本原因：SOF 边界跨越 Bug (The Boundary Crossing Bug)
ESP32-S3 硬件使用的是 Synopsys DWC2 USB 控制器，在预编译的 Arduino TinyUSB 环境中，**Start-Of-Frame (SOF) 中断默认被关闭 (`CFG_TUD_SOF_EN=0`)**。
因此，我们无法在 1ms 物理微帧的绝对起始点精确排队 USB 传输，只能依赖 FreeRTOS 的 `vTaskDelay` 或 `xfer_cb` 完成回调来排队数据。

这引发了致命的 **奇偶帧边界跨越 (Even/Odd Frame Boundary Crossing)** 缺陷：
1. Windows 主机每 1ms (1000Hz) 发送一个 IN Token 轮询音频数据。
2. 当 FreeRTOS 任务因为系统调度延迟，或者在 `xfer_cb` 中排队数据时，物理时间往往刚好跨过了 1.00ms 的边界（比如在 1.05ms 排队）。
3. TinyUSB 底层的 `dcd_dwc2.c` 读取当前物理帧号（N），由于已经迟到，它为了安全，强制将端点的硬件奇偶校验位（`EONUM`）设定为**下一帧（N+1）**。
4. 结果：当前这一帧被直接跳过，数据被推迟到了 2ms 后发送。
5. **最终表现**：USB UAC 传输率从 1000Hz 物理降频到了 500Hz，导致 16kHz 的音频变成了 8kHz 的传输率，引发一半的丢包和极端的锯齿电流音。

## 3. 失败的尝试 (Failed Attempts)
在锁定最终解法前，我们经历了以下惨烈的尝试：
1. **FreeRTOS `vTaskDelayUntil(1ms)`**：FreeRTOS 时钟和电脑 USB 硬件晶振存在相位漂移，当任务醒来早于传输完成时，端点处于忙碌状态，任务跳过该回合，导致 500Hz。
2. **`xfer_cb` 硬件回调排队**：TinyUSB 会在调用 `xfer_cb` 之后才清除 `ep_busy` 标志。如果在回调里调用 `usbd_edpt_xfer`，会直接被拒绝（死锁静音）。
3. **Semaphore (信号量) 严格同步**：通过回调释放信号量唤醒任务。由于唤醒+上下文切换依然需要 50-100us，完美必定跨越 SOF 边界，依然触发 500Hz DWC2 Bug。
4. **强写硬件寄存器 `DIEPCTL` (Hardware Override)**：试图在传输提交后强行覆盖寄存器中的 `EONUM` 奇偶位。导致 DMA 控制器状态机彻底崩溃，麦克风完全死锁静音。
5. **尝试增加包大小 (`wMaxPacketSize = 64` 且 `bInterval = 1`)**：Windows UAC 驱动执行严格的频宽数学校验。对于 16kHz 单声道 16-bit 音频，1ms 只能是 32 bytes。声明 64 bytes 导致 Windows 拒绝识别设备 (Error 43)。

## 4. 终极圣杯方案：2ms 降频缓冲策略 (The Holy Grail)
既然 1ms (1000Hz) 在 DWC2+FreeRTOS 环境下必定触发跨界丢帧，我们选择了“降维打击”——**合规地改变主机轮询频率**。

我们修改了 UAC Isochronous IN 端点描述符：
* `wMaxPacketSize = 64` (32 个采样)
* `bInterval = 2` (要求 Windows 每 2 毫秒轮询一次)

**为什么这是完美的？**
1. **满足严格的频宽校验**：64 bytes / 2ms = 32000 bytes/sec。完美匹配 16kHz 单声道 16-bit 的要求，Windows UAC 驱动毫无怨言地接受，不再报 Error 43。
2. **消灭边界 Bug**：FreeRTOS 任务只需设置为 2ms 周期的 `vTaskDelayUntil`。这给了硬件整整 1 毫秒的空闲冗余时间 (Slack Time) 来清空 `ep_busy`，无论怎样产生相位漂移，都绝对不会跨过下一个 2ms 边界！
3. **完美的音频还原**：缓冲填满了整整 2ms 的音频，一次性交给 DMA。没有任何的丢帧、断点或 CPU 100% 满载，从物理硬件层面彻底解决了抖动锯齿。

## 5. 结论
在没有 `SOF` 中断且受限于 Arduino 预编译栈的 ESP32-S3 项目中，开发 USB 1000Hz 实时同步音频设备，**不要试图在软件层和 1ms 时序死磕**。
利用 USB 规范中的 `bInterval > 1` 合理降频打包，是绕过芯片级 DMA 缺陷最优雅、最稳定的方法。
