# RemoteMapper-ESP32 · 小米蓝牙语音遥控器 USB 复合硬件桥接固件

> 🎯 **基于 ESP32-S3 N16R8 (16MB Flash + 8MB Octal PSRAM) 硬件方案**  
> 彻底解决 Windows 内核键盘驱动丢弃按键、物理键盘冲突、虚拟声卡 (VB-Cable) 依赖等问题！

---

## 🌟 核心特性与优势

1. 🎙️ **真·免驱动 USB 物理麦克风 (UAC 1.0)**：
   - ESP32-S3 原生 USB OTG 模拟标准 16kHz 16-bit Mono 麦克风设备；
   - 遥控器语音键按下即说话，松开即停止，即插即用，无需安装任何第三方声卡驱动。
2. ⌨️ **独立硬件级 USB HID 键盘 & 多媒体控制器**：
   - 小米遥控器音量加（`0x80`）、音量减（`0x81`）直接转换为 PC 标准 **Consumer Volume Up/Down**；
   - 返回键（`0xF1`）转换为 **Consumer AC Back / Esc**（网页与播放器原生后退）；
   - 电源键（`Alt+Tab` / 长按关屏）、主页键（`Win+D`）、菜单键（`Space` 播放暂停）、TV 键（`F8` 语音唤醒）；
   - 与物理主键盘物理隔离，彻底规避软件钩子冲突。
3. ⚡ **双核高吞吐实时 DSP 流水线**：
   - **Core 0**：专门负责 NimBLE 5.0 协议栈、HOGP 按键监听、ATVV 语音流握手与 IMA-ADPCM 4-bit 解码 + Declip 尖峰消除 + 3-Tap FIR 低通平滑 + 动态 AGC；
   - **Core 1**：负责 TinyUSB 复合设备（UAC 麦克风音频泵、HID 键盘派发、按键手势状态机与 CDC 诊断串口）。
4. 🛡️ **健壮的保活与看门狗机制**：
   - 450ms 音频静音看门狗：防止由于蓝牙丢包导致的语音键状态卡住；
   - 自动重连与绑定恢复：NVS 存储配对信息，开机与遥控器唤醒时秒连。

---

## 📁 目录结构

```text
D:\tool\RemoteMapper-ESP32/
├── default_16MB.csv           # 16MB Flash 分区表
├── platformio.ini             # PlatformIO 编译与环境配置
├── build.bat                  # 一键编译脚本
├── flash.bat                  # 一键烧录脚本
├── monitor.bat                # 串口监控日志脚本
├── test.bat                   # 单元测试运行脚本
├── include/
│   ├── app_config.h           # 全局引脚、采样率、缓冲区容量宏定义
│   └── version.h              # 固件版本号
├── src/
│   ├── main.cpp               # FreeRTOS 双核主入口与任务调度
│   ├── audio/                 # ADPCM 解码、Declip、FIR 平滑、AGC 与环形缓冲区
│   ├── ble/                   # NimBLE Central、HOGP 监听与 ATVV 协议客户端
│   ├── usb/                   # TinyUSB UAC 麦克风与 HID 键盘/多媒体控制器
│   ├── keymap/                # 按键手势状态机与键码映射引擎
│   └── cli/                   # Serial CDC 诊断与 JSON 状态接口
└── test/
    └── native/                # 算法与状态机原生验证单元测试
```

---

## 🚀 编译与烧录

### 1. 运行单元测试
```cmd
test.bat
```

### 2. 编译固件
```cmd
build.bat
```

### 3. 烧录到 ESP32-S3
将 ESP32-S3 开发板通过 USB 数据线连接到电脑（注意插在 `USB/OTG` 接口）：
```cmd
flash.bat
```

### 4. 查看实时串口日志
```cmd
monitor.bat
```
