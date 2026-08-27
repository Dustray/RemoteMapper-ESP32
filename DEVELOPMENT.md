# RemoteMapper-ESP32 开发者手册与技术全景文档
## 小米蓝牙语音遥控器 → USB 复合设备（免驱物理麦克风 + 独立多媒体键盘 + Web 可视化后台）

> **适用固件版本**：v1.0.0+  
> **目标主控硬件**：ESP32-S3-DevKitC-1 N16R8（16MB Octal SPI Flash + 8MB Octal SPI PSRAM）  
> **文档受众**：嵌入式开发者、代码维护者与后续 AI Agent 协作开发  

---

# 目录
1. [系统整体架构与多核任务分配](#一-系统整体架构与多核任务分配)
2. [硬件选型与存储分区设计 (16MB Flash)](#二-硬件选型与存储分区设计-16mb-flash)
3. [BLE 与 ATVV 语音协议栈全流程解析](#三-ble-与-atvv-语音协议栈全流程解析)
4. [DSP 实时音频处理流水线算法设计](#四-dsp-实时音频处理流水线算法设计)
5. [按键映射手势状态机与键码转换](#五-按键映射手势状态机与键码转换)
6. [USB 复合设备架构 (UAC 1.0 + HID + CDC)](#六-usb-复合设备架构-uac-10--hid--cdc)
7. [Wi-Fi 双模网络与嵌入式 Web 控制台](#七-wi-fi-双模网络与嵌入式-web-控制台)
8. [RESTful API 完整接口规范](#八-restful-api-完整接口规范)
9. [源码文件索引与模块依赖关系](#九-源码文件索引与模块依赖关系)
10. [编译、烧录与测试指南](#十-编译烧录与测试指南)
11. [故障排查手册与常见问题 (Troubleshooting)](#十一-故障排查手册与常见问题-troubleshooting)
12. [未来扩展与演进路线 (Roadmap)](#十二-未来扩展与演进路线-roadmap)

---

# 一. 系统整体架构与多核任务分配

ESP32-S3 具有 Xtensa® 双核 32 位 LX7 微处理器（主频 240MHz）。为了保证音频实时解码不卡顿、USB Isochronous 报文不丢包、BLE 吞吐高响应，系统采用 **FreeRTOS 双核物理隔离调度架构**：

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                ESP32-S3 核心分配与流水线                                  │
├───────────────────────────────────────────┬────────────────────────────────────────────┤
│           Core 0 (专职蓝牙与 DSP)          │         Core 1 (专职 USB、网络与业务)        │
├───────────────────────────────────────────┼────────────────────────────────────────────┤
│ • NimBLE Central 协议栈 (扫描/连接/绑定)   │ • TinyUSB 复合设备 (UAC Mic + HID + CDC)   │
│ • HOGP 按键报文解析 (订阅 0x2A4D)         │ • UAC 麦克风 1ms Isochronous 数据泵 (16kHz)│
│ • ATVV 语音握手 (0x0A/0x0C/0x0E/0x00)     │ • KeyState 映射手势状态机 (单击/长按/连发) │
│ • 4-bit IMA-ADPCM 实时流式解码             │ • Wi-Fi 双模管理器 (AP 192.168.4.1 + STA)  │
│ • Declip 尖峰消除 + 3-Tap FIR 低通平滑    │ • Captive Portal DNS 强制门户 (Port 53)    │
│ • 动态 AGC 增益控制与软削波                │ • HTTP WebServer (Port 80 RESTful API)     │
│ • 音频数据注入 SPSC 无锁环形缓冲区         │ • Serial / CDC CLI 控制台指令调度          │
└───────────────────────────────────────────┴────────────────────────────────────────────┘
                                     │                     ▲
                                     └────── RingBuffer ───┘
                                         (4096 Samples PCM)
```

---

# 二. 硬件选型与存储分区设计 (16MB Flash)

### 1. 硬件连接规范
* **USB 接口**：必须连接 ESP32-S3 开发板的 **`USB/OTG` 接口**（对应 GPIO 19: D-，GPIO 20: D+），**不可连接 UART 口**；
* **天线**：板载 PCB 天线或 IPEX 外接天线；
* **供电**：USB 5V 直接供电，典型功耗约 120mA。

### 2. 16MB Flash 分区表 (`default_16MB.csv`)
为确保后续支持 OTA 无缝固件升级，Flash 分区规划如下：

| 分区名 | 类型 | 子类型 | 起始偏移 (Offset) | 大小 (Size) | 用途说明 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `nvs` | data | nvs | `0x9000` | 24 KB (`0x6000`) | BLE 配对密钥、Wi-Fi 配置、自定义按键存储 |
| `otadata` | data | ota | `0xf000` | 8 KB (`0x2000`) | OTA 升级状态标记 |
| `app0` | app | ota_0 | `0x20000` | 6 MB (`0x600000`) | 当前运行的主固件镜像 |
| `app1` | app | ota_1 | `0x620000` | 6 MB (`0x600000`) | 后续 OTA 固件备用镜像 |
| `spiffs` | data | spiffs | `0xc20000` | 3.75 MB (`0x3c0000`)| 嵌入式静态资源 / 扩展文件系统 |
| `coredump` | data | coredump | `0xfe0000` | 128 KB (`0x20000`) | 系统崩溃转储诊断日志 |

---

# 三. BLE 与 ATVV 语音协议栈全流程解析

### 1. GATT 服务与特征值定义
* **HOGP 规范服务**：UUID `0x1812` ➔ HID Report 特征值 `0x2A4D`（Notify，订阅捕获按键字节）；
* **ATVV 自定义服务**：UUID `ab5e0001-5a21-4f05-bc7d-af01f617b664`
  * `ab5e0002-5a21-4f05-bc7d-af01f617b664` (**CMD 通道**，Write Without Response)
  * `ab5e0003-5a21-4f05-bc7d-af01f617b664` (**AUDIO 通道**，Notify，接收 ADPCM 数据流)
  * `ab5e0004-5a21-4f05-bc7d-af01f617b664` (**CTL 通道**，Notify，接收语音启停状态)

### 2. ATVV 握手与语音状态机时序
```text
ESP32-S3 (Central)                                      小米遥控器 (Peripheral)
      │                                                           │
      │── 1. 扫描匹配 "MI RC" 或 MAC 前缀 "c0:5d:39" ─────────────►│
      │◄─ 2. 建立 BLE GATT 连接并启用 Bonding ─────────────────────│
      │                                                           │
      │── 3. 订阅 CTL(0x04)、AUDIO(0x03)、HOGP(0x2A4D) 特征值 ───►│
      │                                                           │
      │── 4. 发送 GET_CAPS: [0x0A, 0x01, 0x00, 0x00, 0x03, 0x03] ─►│
      │◄─ 5. 收到 CAPS_RESP [0x0B, ...]: 确认 16kHz ADPCM & 帧大小 ──│
      │                                                           │
      │── 6. 发送 MIC_OPEN: [0x0C, 0x00] ─────────────────────────►│
      │      (遥控器进入 HTT 按住说话待命模式)                     │
      │                                                           │
      │  ═════════════════ 用户按住遥控器语音键 ═════════════════   │
      │◄─ 7. CTL 通道上报 AUDIO_START HTT: [0x04, 0x03, 0x00, SID]│
      │      • 固件重置 ADPCM 预测器 & 滤波器                     │
      │      • 触发 USB HID 注入语音热键 (RAlt + ,)                │
      │      • 开启 UAC 麦克风音频推流                            │
      │                                                           │
      │◄─ 8. AUDIO 通道持续推送 ADPCM 数据帧 (每帧 120 字节) ──────│
      │      • 解码为 240 个 16-bit PCM 采样点并写入 RingBuffer    │
      │                                                           │
      │── 9. 每隔 2000ms 发送 MIC_EXTEND: [0x0E, SID] ───────────►│
      │      (延长语音会话防遥控器超时自动断流)                   │
      │                                                           │
      │  ═════════════════ 用户松开遥控器语音键 ═════════════════   │
      │◄─ 10. CTL 通道上报 AUDIO_STOP: [0x00, 0x02] ──────────────│
      │       • 释放语音热键                                      │
      │       • 停止麦克风推流，清空缓冲区                        │
      │       • 重新发送 MIC_OPEN [0x0C, 0x00] 防御下次按键失效    │
```

---

# 四. DSP 实时音频处理流水线算法设计

### 1. 4-bit IMA-ADPCM 流式解码
- **采样参数**：16000 Hz 单声道 16-bit PCM；
- **帧规格**：BLE 传输每包 120 字节（Hi-nibble 高 4 位优先，低 4 位其次），对应解码出 240 个 PCM 采样点；
- **跨帧连续性**：Predictor（预测器）和 StepIndex（步长索引 [0, 88]）在帧间保持连续，遇到 ATVV `0x0A` (`AUDIO_SYNC`) 指令时动态重置为同步点。

### 2. Declip 单点尖峰消除
消除蓝牙丢包或电磁干扰引起的孤立单点脉冲毛刺：
$$\text{若 } |x_i - x_{i-1}| > 1000 \text{ 且 } |x_i - x_{i+1}| > 1000 \text{ 且 } \min(dp, dn) > 2 \cdot |x_{i+1} - x_{i-1}| \implies x_i = \frac{x_{i-1} + x_{i+1}}{2}$$

### 3. 3-Tap 三角 FIR 低通平滑滤波器
抑制高频量化白噪，提升人声音质厚度与转写清晰度：
$$y_i = \frac{x_{i-1} + 2x_i + x_{i+1}}{4}$$

### 4. 动态 AGC (自动增益控制)
- **目标电平**：`28000.0`
- **包络衰减率**：`0.9997`（约 144ms 慢释放，避免呼吸底噪）
- **最大增益限制**：`30.0` 倍（避免空闲无声时放大本底噪声）
- **软削波**：防止波形硬切顶产生破音。

---

# 五. 按键映射手势状态机与键码转换

### 1. 遥控器全按键物理编码转换表

| 遥控器物理按键 | 小米原始 HID 报文 | ESP32 输出的 USB 信号 | 功能效果 |
| :--- | :--- | :--- | :--- |
| **音量+** | `0x80` | `Consumer: Volume Up (0x00E9)` | Windows 系统原生主音量增加 (支持高速连发) |
| **音量-** | `0x81` | `Consumer: Volume Down (0x00EA)`| Windows 系统原生主音量减小 (支持高速连发) |
| **返回键** | `0xF1` | `Consumer: AC Back (0x0224)` | 浏览器/文件管理器后退、播放器退出全屏 |
| **语音键** | ATVV 事件 | **注入 `RAlt + ,` + UAC 麦克风** | 唤醒微信输入法语音并实时录音 |
| **电源键** | `0xFF` / `0x66` | **单击 `Alt + Tab` / 长按 `Sleep`** | 窗口切换 / 系统睡眠 |
| **主页键** | `0x24` / `0x4A` | `Keyboard: Win + D` | 一键返回桌面 |
| **菜单键** | `0x5D` / `0x65` | `Keyboard: Space` | 视频播放 / 暂停 |
| **直播/TV 键** | `0xC0` / `0x35` | `Keyboard: F8` | 自定义快捷键 |
| **方向上/下/左/右** | `0x52/51/50/4F`| `Keyboard: Up/Down/Left/Right` | 播放器快进快退调进度 |
| **确定键 (OK)** | `0x28` | `Keyboard: Return` | 确认 / 回车 |

### 2. 状态机手势逻辑
- **单击 (Tap)**：按键按下后在 `long_ms` (600ms) 内释放，无双击需求时立即派发；
- **长按 (Hold)**：按键持续按下超过 `long_ms` (600ms)，触发长按动作（如关屏/睡眠），释放时不重复触发单击；
- **连发 (Repeat)**：首次按下延时 `350ms` 后，以 `70ms` 间隔持续发送（专用于音量键流畅调节）。

---

# 六. USB 复合设备架构 (UAC 1.0 + HID + CDC)

ESP32-S3 原生 USB OTG 控制器在枚举时向 Windows 呈现一个**复合多接口设备 (Composite Device)**：

1. **Interface 0 & 1：USB Audio Class 1.0 (麦克风)**
   - 格式：PCM 16-bit Mono, 16000Hz；
   - 端点：Isochronous IN 端点（端点大小 32 字节，每 1ms 轮询一次）；
   - 设备名：`RemoteMapper Wireless Mic`。
2. **Interface 2：USB HID Keyboard**
   - 标准 6KRO 键盘报文，负责发送 `Win+D`, `Alt+Tab`, `RAlt+,` 等修饰组合键。
3. **Interface 3：USB HID Consumer Control**
   - 16-bit Usage 控制报文，负责发送系统音量与 AC 后退。
4. **Interface 4：USB CDC Serial**
   - 虚拟串口，用于串口监控与 JSON 命令行交互。

---

# 七. Wi-Fi 双模网络与嵌入式 Web 控制台

### 1. 网络拓扑
* **AP 模式（热点）**：
  - SSID: `RemoteMapper-AP` (无密码)
  - IP: `192.168.4.1` (子网掩码 `255.255.255.0`)
  - DNS Captive Portal: 拦截所有未识别域名的 DNS 查询并重定向到 `192.168.4.1`。
* **STA 模式（局域网客户端）**：
  - 连接路由器 2.4GHz Wi-Fi；
  - mDNS: 注册服务名 `remotemapper`，局域网访问地址为 **`http://remotemapper.local`**。

### 2. Web 前端架构 (`src/web/web_ui.h`)
- **纯原生实现**：零外部 CDN、零 npm 依赖，HTML/CSS/JS 经 PROGMEM 嵌入 Flash；
- **响应式设计**：完美适配手机屏幕（触摸操作）与电脑宽屏浏览器；
- **暗黑科技主题**：高对比度仪表盘、实时 RSSI 状态指示条、自动滚动日志监视器。

---

# 八. RESTful API 完整接口规范

| 请求方法 | 路由 Path | 描述 | 请求体示例 | 响应体示例 |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/status` | 获取系统全量状态与遥测 | 无 | `{"version":"1.0.0","ble_state":3,"frames_decoded":1240,"free_heap":241500,"sta_ip":"192.168.1.108"}` |
| `GET` | `/api/logs` | 获取最近 60 条环形日志 | 无 | `{"logs":["[0012.340] [BLE] Connected","[0014.120] [VOICE] PRESSED"]}` |
| `POST` | `/api/logs/clear` | 清空内部日志环形缓冲 | 无 | `{"status":"cleared"}` |
| `GET` | `/api/wifi/scan` | 扫描周围 2.4GHz Wi-Fi | 无 | `{"networks":[{"ssid":"Home-WiFi","rssi":-58,"secure":true}]}` |
| `POST`| `/api/wifi/config` | 配置并连接家庭 Wi-Fi | `{"ssid":"MyHome","pass":"12345678"}` | `{"status":"ok"}` |
| `POST`| `/api/keymap/reset` | 重置按键映射为出厂默认 | 无 | `{"status":"reset_ok"}` |
| `POST`| `/api/ble/reconnect` | 触发重新扫描与连接遥控器 | 无 | `{"status":"reconnecting"}` |
| `POST`| `/api/system/restart` | 软重启 ESP32 | 无 | `{"status":"rebooting"}` |

---

# 九. 源码文件索引与模块依赖关系

```text
D:\tool\RemoteMapper-ESP32/
├── platformio.ini              # PlatformIO 构建配置文件 (Flash/PSRAM/编译选项)
├── default_16MB.csv            # 16MB Flash 分区表
├── include/
│   ├── app_config.h            # 全局配置 (采样率、引脚、UUID、看门狗超时)
│   └── version.h               # 固件版本号
├── src/
│   ├── main.cpp                # 系统入口与 FreeRTOS 双核任务调度
│   ├── log/
│   │   ├── app_log.h           # 全局日志接口
│   │   └── app_log.cpp         # 环形日志缓冲实现
│   ├── audio/                  # DSP 音频流水线模块
│   │   ├── adpcm_decoder.h/.c  # IMA-ADPCM 4-bit 解码器
│   │   ├── audio_filter.h/.c   # Declip 尖峰消除与 3-Tap FIR 低通平滑
│   │   ├── audio_agc.h/.c      # 动态 AGC 与软削波
│   │   ├── audio_ring_buffer.h/.c # SPSC 无锁环形缓冲区
│   │   └── audio_pipeline.h/.c # 音频流水线协调器
│   ├── ble/                    # 蓝牙与协议栈模块
│   │   ├── ble_remote_client.h # BLE Central 状态机
│   │   └── ble_remote_client.cpp # NimBLE 驱动、HOGP 按键与 ATVV 语音处理
│   ├── usb/                    # USB 复合设备模块
│   │   ├── usb_composite.h     # USB 设备抽象接口
│   │   └── usb_composite.cpp   # TinyUSB UAC 麦克风与 HID 键盘/多媒体派发
│   ├── keymap/                 # 按键映射状态机
│   │   ├── key_definitions.h   # 遥控器与 USB HID 键码映射表
│   │   ├── key_state_machine.h # 手势状态机接口
│   │   └── key_state_machine.c # 单击/长按/连发状态机实现
│   ├── wifi/                   # 网络模块
│   │   ├── wifi_manager.h      # Wi-Fi 管理接口
│   │   └── wifi_manager.cpp    # AP/STA 双模、DNS 强制门户与 mDNS
│   ├── web/                    # 嵌入式 Web 模块
│   │   ├── web_ui.h            # 嵌入式 HTML/CSS/JS 单页应用
│   │   ├── web_server.h        # WebServer 接口
│   │   └── web_server.cpp      # RESTful API 路由处理
│   └── cli/                    # 串口诊断模块
│       ├── cli_manager.h       # CDC CLI 接口
│       └── cli_manager.cpp     # JSON 格式命令行解析器
└── test/
    └── native/test_suite.py    # Python 算法单元验证测试套件
```

---

# 十. 编译、烧录与测试指南

### 1. 编译固件
```cmd
cd /d D:\tool\RemoteMapper-ESP32
build.bat
```
*(底层调用 `python -m platformio run -e esp32s3_n16r8`)*

### 2. 烧录固件
将 ESP32-S3 开发板通过 USB 数据线连接到电脑的 **`USB/OTG`** 接口：
```cmd
flash.bat
```

### 3. 查看串口监控日志
```cmd
monitor.bat
```

### 4. 运行单元测试
```cmd
test.bat
```

---

# 十一. 故障排查手册与常见问题 (Troubleshooting)

### Q1: 遥控器按键没有反应，串口一直显示 `Scanning for Xiaomi Remote...`？
- **排查步骤**：
  1. 遥控器是否已被电脑原本的蓝牙占用？请在 Windows 蓝牙设置中先点击「删除设备/断开连接」；
  2. 同时按住遥控器的 **主页键 + 菜单键** 进行配对，直到遥控器指示灯闪烁并发出蜂鸣声；
  3. 观察串口日志，ESP32 扫描到广播名含 `MI RC` 或 MAC `C0:5D:39` 会自动完成配对与服务绑定。

### Q2: 语音键按下后输入法唤醒了，但没有声音录入？
- **排查步骤**：
  1. 打开 Windows「声音设置」➔「录音设备」，查看是否存在 `RemoteMapper Wireless Mic`；
  2. 将微信输入法或讯飞输入法的麦克风直接选择为 `RemoteMapper Wireless Mic`；
  3. 在 Web 控制台查看「已解码音频帧」数字是否在说话时实时增长。

### Q3: 手机连接 `RemoteMapper-AP` 后无法打开配置界面？
- **排查步骤**：
  1. 手机连上热点后若提示「当前 Wi-Fi 无法访问互联网」，请选择「保持连接」；
  2. 手动在手机浏览器地址栏输入 **`http://192.168.4.1`**；
  3. 确保电脑/手机未开启全局代理或 VPN。

### Q4: 固件编译报错找不到 `NimBLEDevice.h` 或 `USBHIDKeyboard.h`？
- **排查步骤**：
  1. 确保 `platformio.ini` 中的 `framework = arduino` 且 `lib_deps` 包含 `NimBLE-Arduino`；
  2. 运行 `python -m platformio pkg install -e esp32s3_n16r8` 重新下载依赖库。

---

# 十二. 未来扩展与演进路线 (Roadmap)

1. **Web 端按键完全图形化拖拽自定义**：支持用户在浏览器中点击任意按键，弹出下拉框直接挑选 Windows 快捷键（如 `Ctrl+Alt+A` 截图、`Win+Shift+S` 等）并持久化到 NVS。
2. **WebSerial USB 离线免网改键**：通过浏览器 WebSerial API 直接走 USB 虚拟串口收发 JSON 改键指令，彻底无需 Wi-Fi。
3. **多设备配对管理与一键切换**：在 Web 端保存多个小米遥控器 MAC 地址并支持一键切换绑定。
