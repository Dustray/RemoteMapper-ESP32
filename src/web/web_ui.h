#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RemoteMapper - 小米蓝牙遥控器硬件桥接器</title>
    <style>
        :root {
            --bg-primary: #0b0f17;
            --bg-card: #151d2a;
            --bg-hover: #1e293b;
            --accent-cyan: #06b6d4;
            --accent-blue: #3b82f6;
            --accent-green: #10b981;
            --accent-orange: #f59e0b;
            --accent-red: #ef4444;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --border-color: #243247;
            --radius-card: 16px;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background-color: var(--bg-primary); color: var(--text-main); line-height: 1.5; padding-bottom: 60px; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }

        header { display: flex; justify-content: space-between; align-items: center; padding: 16px 0; border-bottom: 1px solid var(--border-color); margin-bottom: 24px; }
        .logo { font-size: 24px; font-weight: 700; background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        .badge { background: rgba(6, 182, 212, 0.15); color: var(--accent-cyan); padding: 4px 10px; border-radius: 9999px; font-size: 12px; font-weight: 600; border: 1px solid rgba(6, 182, 212, 0.3); }

        .tabs { display: flex; gap: 8px; margin-bottom: 20px; overflow-x: auto; padding-bottom: 4px; }
        .tab-btn { background: var(--bg-card); border: 1px solid var(--border-color); color: var(--text-muted); padding: 10px 18px; border-radius: 10px; cursor: pointer; font-size: 14px; font-weight: 600; transition: all 0.2s; white-space: nowrap; }
        .tab-btn:hover { background: var(--bg-hover); color: var(--text-main); }
        .tab-btn.active { background: linear-gradient(135deg, var(--accent-blue), var(--accent-cyan)); color: #fff; border-color: transparent; box-shadow: 0 4px 12px rgba(6, 182, 212, 0.3); }
        .tab-content { display: none; }
        .tab-content.active { display: block; }

        .card { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: var(--radius-card); padding: 20px; margin-bottom: 20px; }
        .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; font-size: 16px; font-weight: 600; }
        .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .grid-4 { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; margin-bottom: 20px; }
        @media (max-width: 768px) { .grid-2 { grid-template-columns: 1fr; } }

        .stat-card { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: 12px; padding: 14px 18px; }
        .stat-title { font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
        .stat-val { font-size: 18px; font-weight: 700; color: #fff; }

        /* Real Xiaomi Silver Metallic Remote Visualizer */
        .remote-tester-container { display: flex; gap: 36px; align-items: flex-start; justify-content: center; flex-wrap: wrap; padding: 10px 0; }
        
        .real-remote-body {
            width: 220px;
            background: linear-gradient(180deg, #d4d4d8 0%, #e4e4e7 40%, #d4d4d8 70%, #a1a1aa 100%);
            border: 2px solid #e4e4e7;
            border-radius: 36px;
            padding: 26px 18px 20px 18px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.8), inset 0 2px 4px rgba(255,255,255,0.8);
            display: flex;
            flex-direction: column;
            align-items: center;
            position: relative;
        }

        .remote-top-row { display: flex; width: 100%; justify-content: space-between; margin-bottom: 20px; }
        .r-circle-btn {
            width: 44px; height: 44px;
            background: #27272a;
            border: 1px solid #3f3f46;
            border-radius: 50%;
            display: flex; align-items: center; justify-content: center;
            color: #d4d4d8; cursor: pointer;
            transition: all 0.12s; font-size: 16px; user-select: none;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        }
        .r-circle-btn:hover { background: #3f3f46; color: #fff; transform: translateY(-1px); }
        .r-circle-btn.pressed { background: #06b6d4 !important; color: #000 !important; transform: scale(0.92) !important; box-shadow: 0 0 20px #06b6d4 !important; }
        .r-circle-btn.power { background: #27272a; color: #f87171; }
        .r-circle-btn.voice { background: #27272a; color: #60a5fa; }

        /* D-Pad Section */
        .real-dpad-ring {
            width: 154px; height: 154px;
            border-radius: 50%;
            background: #27272a;
            border: 1px solid #3f3f46;
            position: relative;
            margin-bottom: 22px;
            display: flex; align-items: center; justify-content: center;
            box-shadow: 0 6px 12px rgba(0,0,0,0.35);
        }
        .dpad-part { position: absolute; background: transparent; border: none; color: #71717a; cursor: pointer; font-size: 14px; transition: all 0.12s; display: flex; align-items: center; justify-content: center; }
        .dpad-part:hover { color: #fff; }
        .dpad-part.pressed { color: #06b6d4 !important; transform: scale(0.9); text-shadow: 0 0 12px #06b6d4; }
        .d-up { top: 6px; width: 60px; height: 38px; }
        .d-down { bottom: 6px; width: 60px; height: 38px; }
        .d-left { left: 6px; width: 38px; height: 60px; }
        .d-right { right: 6px; width: 38px; height: 60px; }
        .d-center {
            width: 66px; height: 66px; border-radius: 50%;
            background: #18181b; border: 1px solid #3f3f46;
            z-index: 2; font-size: 13px; font-weight: bold; color: #d4d4d8;
        }
        .d-center.pressed { background: #06b6d4 !important; color: #000 !important; box-shadow: 0 0 20px #06b6d4 !important; }

        /* 2-Column Lower Controls */
        .remote-controls-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; width: 100%; margin-bottom: 24px; align-items: center; }
        .ctrl-col-left { display: flex; flex-direction: column; gap: 14px; align-items: center; }
        .ctrl-col-right { display: flex; flex-direction: column; gap: 14px; align-items: center; }

        /* Integrated Volume Rocker */
        .vol-pill {
            width: 48px; height: 102px;
            background: #27272a;
            border: 1px solid #3f3f46;
            border-radius: 24px;
            display: flex; flex-direction: column;
            overflow: hidden;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        }
        .vol-half {
            flex: 1; border: none; background: transparent;
            color: #d4d4d8; cursor: pointer;
            font-size: 18px; font-weight: bold;
            display: flex; align-items: center; justify-content: center;
            transition: all 0.12s;
        }
        .vol-half:hover { background: #3f3f46; color: #fff; }
        .vol-half.pressed { background: #06b6d4 !important; color: #000 !important; }
        .vol-half:first-child { border-bottom: 1px solid #3f3f46; }

        .btn-tv-box { width: 48px; height: 48px; border-radius: 50%; font-size: 12px; font-weight: bold; }
        .remote-footer { margin-top: 10px; display: flex; flex-direction: column; align-items: center; color: #71717a; font-size: 11px; }
        .remote-footer .nfc-icon { width: 14px; height: 14px; border: 1px solid #71717a; border-radius: 2px; display: flex; align-items: center; justify-content: center; font-size: 9px; font-weight: bold; margin-bottom: 14px; }

        /* Key Event Monitor Panel */
        .event-box { flex: 1; min-width: 320px; background: #090d16; border: 1px solid var(--border-color); border-radius: 14px; padding: 20px; }
        .stat-badge { font-size: 26px; font-weight: 700; color: var(--accent-cyan); margin: 6px 0 14px 0; }
        .event-field { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid rgba(255,255,255,0.05); font-size: 14px; }
        .event-field span:first-child { color: var(--text-muted); }
        .event-field span:last-child { font-weight: 600; font-family: monospace; color: #fff; }

        /* Ultra-Simple Interactive Remap Modal */
        .modal-overlay { position: fixed; inset: 0; background: rgba(0,0,0,0.8); backdrop-filter: blur(6px); display: none; align-items: center; justify-content: center; z-index: 100; padding: 20px; }
        .modal { background: var(--bg-card); border: 1px solid var(--border-color); border-radius: var(--radius-card); max-width: 520px; width: 100%; padding: 24px; box-shadow: 0 25px 50px rgba(0,0,0,0.6); }

        .key-recorder-box {
            border: 2px dashed var(--accent-blue);
            background: rgba(59, 130, 246, 0.08);
            border-radius: 12px;
            padding: 24px 16px;
            text-align: center;
            cursor: pointer;
            outline: none;
            transition: all 0.2s;
            margin-bottom: 18px;
        }
        .key-recorder-box:focus, .key-recorder-box.recording {
            border-color: var(--accent-cyan);
            background: rgba(6, 182, 212, 0.15);
            box-shadow: 0 0 20px rgba(6, 182, 212, 0.3);
        }
        .key-badge-display { font-size: 24px; font-weight: 800; color: #fff; margin-top: 8px; min-height: 36px; display: flex; align-items: center; justify-content: center; gap: 8px; flex-wrap: wrap; }
        .kbd-chip { background: #0f172a; border: 1px solid var(--accent-cyan); color: var(--accent-cyan); padding: 4px 12px; border-radius: 6px; font-size: 18px; box-shadow: 0 2px 6px rgba(0,0,0,0.5); }

        .btn { background: linear-gradient(135deg, var(--accent-blue), var(--accent-cyan)); color: #fff; border: none; padding: 10px 18px; border-radius: 8px; font-size: 14px; font-weight: 600; cursor: pointer; transition: opacity 0.2s; }
        .btn:hover { opacity: 0.9; }
        .btn-outline { background: transparent; border: 1px solid var(--border-color); color: var(--text-main); }
        .btn-outline:hover { background: var(--bg-hover); }
        .btn-danger { background: var(--accent-red); }

        .log-terminal { background: #000; border: 1px solid #1f2937; border-radius: 8px; padding: 12px; font-family: "SFMono-Regular", Consolas, Menlo, monospace; font-size: 12px; height: 380px; overflow-y: auto; color: #34d399; line-height: 1.6; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div style="display: flex; align-items: center; gap: 12px;">
                <div class="logo">RemoteMapper</div>
                <span class="badge">ESP32-S3 Hardware Bridge</span>
            </div>
            <div id="top-status" style="font-size: 13px; color: var(--text-muted);">正在连接硬件...</div>
        </header>

        <!-- System Overview Cards -->
        <div class="grid-4">
            <div class="stat-card">
                <div class="stat-title">蓝牙遥控器连接状态</div>
                <div class="stat-val" id="stat-ble-state" style="color: var(--accent-green);">已连接</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;" id="stat-ble-name">小米蓝牙语音遥控器</div>
            </div>
            <div class="stat-card">
                <div class="stat-title">Wi-Fi 局域网 IP</div>
                <div class="stat-val" id="stat-sta-ip">192.168.2.179</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;">热点: 192.168.4.1</div>
            </div>
            <div class="stat-card">
                <div class="stat-title">音频流水线状态</div>
                <div class="stat-val" id="stat-audio-frames">0 帧</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;">16kHz 16-Bit Mono UAC 1.0</div>
            </div>
            <div class="stat-card">
                <div class="stat-title">系统内存 / 运行时间</div>
                <div class="stat-val" id="stat-uptime">0s</div>
                <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;" id="stat-mem">SRAM: 200KB | PSRAM: 8MB</div>
            </div>
        </div>

        <div class="tabs">
            <button class="tab-btn active" onclick="switchTab('tab-tester')">🎮 遥控器与改键测试</button>
            <button class="tab-btn" onclick="switchTab('tab-ble')">📡 蓝牙配对管理</button>
            <button class="tab-btn" onclick="switchTab('tab-logs')">📜 运行日志</button>
            <button class="tab-btn" onclick="switchTab('tab-wifi')">📶 Wi-Fi 与系统配置</button>
        </div>

        <!-- TAB 1: Key Tester & Remapper Visualizer -->
        <div id="tab-tester" class="tab-content active">
            <div class="card">
                <div class="card-header">
                    <span>🎮 真机 1:1 遥控器测试器（按压实体遥控器实时联动，点击按键即可修改按键映射）</span>
                    <button class="btn btn-outline" style="font-size: 12px;" onclick="resetAllKeymaps()">恢复默认按键映射</button>
                </div>
                
                <div class="remote-tester-container">
                    <!-- Real Xiaomi Silver Metallic Remote DOM -->
                    <div class="real-remote-body">
                        <!-- Top Row: Power & Voice -->
                        <div class="remote-top-row">
                            <div class="r-circle-btn power" id="btn-0x66" onclick="openRemapModal(0x66, '电源键 (Power)')">⏻</div>
                            <div class="r-circle-btn voice" id="btn-0x04" onclick="openRemapModal(0x04, '语音键 (Voice)')">🎙</div>
                        </div>

                        <!-- Middle: D-Pad -->
                        <div class="real-dpad-ring">
                            <button class="dpad-part d-up" id="btn-0x52" onclick="openRemapModal(0x52, '方向上 (Up)')">●</button>
                            <button class="dpad-part d-down" id="btn-0x51" onclick="openRemapModal(0x51, '方向下 (Down)')">●</button>
                            <button class="dpad-part d-left" id="btn-0x50" onclick="openRemapModal(0x50, '方向左 (Left)')">●</button>
                            <button class="dpad-part d-right" id="btn-0x4F" onclick="openRemapModal(0x4F, '方向右 (Right)')">●</button>
                            <button class="dpad-part d-center" id="btn-0x28" onclick="openRemapModal(0x28, '确定键 (OK)')">OK</button>
                        </div>

                        <!-- Lower: 2 Columns Matching Real Remote -->
                        <div class="remote-controls-grid">
                            <!-- Left Column: Back, Home, Menu -->
                            <div class="ctrl-col-left">
                                <div class="r-circle-btn" id="btn-0xF1" onclick="openRemapModal(0xF1, '返回键 (Back)')">&lt;</div>
                                <div class="r-circle-btn" id="btn-0x24" onclick="openRemapModal(0x24, '主页键 (Home)')">⌂</div>
                                <div class="r-circle-btn" id="btn-0x5D" onclick="openRemapModal(0x5D, '菜单键 (Menu)')">≡</div>
                            </div>

                            <!-- Right Column: Vol Rocker (+/-) & TV -->
                            <div class="ctrl-col-right">
                                <div class="vol-pill">
                                    <button class="vol-half" id="btn-0x80" onclick="openRemapModal(0x80, '音量+ (Vol+)')">+</button>
                                    <button class="vol-half" id="btn-0x81" onclick="openRemapModal(0x81, '音量- (Vol-)')">−</button>
                                </div>
                                <div class="r-circle-btn btn-tv-box" id="btn-0xC0" onclick="openRemapModal(0xC0, '电视键 (TV)')">📺 TV</div>
                            </div>
                        </div>

                        <!-- Bottom Branding -->
                        <div class="remote-footer">
                            <div class="nfc-icon">N</div>
                            <span style="font-weight: 700; letter-spacing: 1px; font-size: 13px;">xiaomi</span>
                        </div>
                    </div>

                    <!-- Live Key Event Telemetry -->
                    <div class="event-box">
                        <h3 style="margin-bottom: 12px; font-size: 15px; color: var(--accent-cyan);">⚡ 实时按键遥测状态</h3>
                        <div class="stat-badge" id="live-key-name">等待按键...</div>
                        
                        <div class="event-field">
                            <span>物理键码 (HID Raw Code)</span>
                            <span id="live-key-code">0x00</span>
                        </div>
                        <div class="event-field">
                            <span>按键状态</span>
                            <span id="live-key-state" style="color: var(--text-muted);">IDLE</span>
                        </div>
                        <div class="event-field">
                            <span>按下持续时间 (Hold Time)</span>
                            <span id="live-key-dur">0 ms</span>
                        </div>
                        <div class="event-field">
                            <span>触发动作 (Action Type)</span>
                            <span id="live-act-type">ACTION_NONE</span>
                        </div>
                        <div class="event-field">
                            <span>注入键值 (Dispatched Key)</span>
                            <span id="live-act-val">None</span>
                        </div>

                        <div style="margin-top: 20px; padding: 12px; background: rgba(6,182,212,0.1); border-radius: 8px; border: 1px dashed rgba(6,182,212,0.3); font-size: 13px;">
                            💡 <b>极简改键说明</b>：直接点击左侧任意遥控器按键，然后<b>直接在电脑键盘上按下您想映射的按键或快捷键</b>（或点选常用多媒体功能），点击保存即可！
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- TAB 2: BLE Device Radar -->
        <div id="tab-ble" class="tab-content">
            <div class="card">
                <div class="card-header">
                    <span>📡 蓝牙设备雷达与遥控器配对</span>
                    <button class="btn" onclick="scanBleDevices()">🔍 扫描附近蓝牙设备</button>
                </div>
                <div id="ble-dev-list" style="margin-top: 14px;">点击上方按钮扫描附近的蓝牙遥控器...</div>
            </div>
        </div>

        <!-- TAB 3: Runtime Logs -->
        <div id="tab-logs" class="tab-content">
            <div class="card">
                <div class="card-header">
                    <span>📜 ESP32-S3 实时运行日志 (保留完整 250 行)</span>
                    <div style="display: flex; gap: 8px;">
                        <button class="btn btn-outline" style="font-size: 12px;" onclick="refreshLogs()">🔄 刷新</button>
                        <button class="btn btn-outline" style="font-size: 12px;" onclick="clearLogs()">🧹 清空</button>
                    </div>
                </div>
                <div class="log-terminal" id="log-terminal">正在加载运行日志...</div>
            </div>
        </div>

        <!-- TAB 4: Wi-Fi & System -->
        <div id="tab-wifi" class="tab-content">
            <div class="grid-2">
                <div class="card">
                    <div class="card-header">
                        <span>📶 Wi-Fi 网络配置与附近热点搜索</span>
                        <button class="btn btn-outline" style="font-size: 12px;" onclick="scanWifiNetworks()">🔍 搜索 Wi-Fi</button>
                    </div>
                    <div id="wifi-scan-list" style="margin-bottom: 16px; font-size: 13px; color: var(--text-muted);">
                        点击右上角“搜索 Wi-Fi”可扫描附近 2.4GHz 无线网络，点击即可自动填入 SSID。
                    </div>
                    <div class="form-group">
                        <label style="display:block; font-size:13px; color:var(--text-muted); margin-bottom:6px;">Wi-Fi 名称 (SSID)</label>
                        <input type="text" id="wifi-ssid" placeholder="输入或从上方选择您的 Wi-Fi 名称" style="width:100%; background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:10px 14px; color:#fff; font-size:14px; outline:none;">
                    </div>
                    <div class="form-group" style="margin-top:14px;">
                        <label style="display:block; font-size:13px; color:var(--text-muted); margin-bottom:6px;">Wi-Fi 密码</label>
                        <input type="password" id="wifi-pass" placeholder="输入 Wi-Fi 密码" style="width:100%; background:#0b0f17; border:1px solid var(--border-color); border-radius:8px; padding:10px 14px; color:#fff; font-size:14px; outline:none;">
                    </div>
                    <button class="btn" style="width: 100%; margin-top:16px;" onclick="saveWifi()">💾 保存并连接 Wi-Fi</button>
                </div>

                <div class="card">
                    <div class="card-header"><span>⚙️ 系统控制</span></div>
                    <p style="font-size: 14px; color: var(--text-muted); margin-bottom: 20px;">
                        当前固件支持 UAC 1.0 USB 麦克风录音设备与标准 HID 键盘/多媒体复合注入。
                    </p>
                    <button class="btn btn-danger" style="width: 100%;" onclick="restartDevice()">🔄 重启 ESP32-S3 设备</button>
                </div>
            </div>
        </div>
    </div>

    <!-- Ultra-Simple Interactive Remap Modal -->
    <div class="modal-overlay" id="remap-modal" onclick="if(event.target === this) closeRemapModal()">
        <div class="modal">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px;">
                <h3 style="font-size: 18px;" id="modal-title">设置按键映射</h3>
                <span id="modal-vk-badge" style="font-size: 12px; color: var(--accent-cyan); font-family: monospace;">0x00</span>
            </div>

            <!-- Voice Key Exclusive Banner -->
            <div id="voice-key-banner" style="display:none; background: rgba(59, 130, 246, 0.15); border: 1px solid rgba(59, 130, 246, 0.4); border-radius: 10px; padding: 12px 14px; margin-bottom: 16px; font-size: 13px; color: #93c5fd; line-height: 1.5;">
                🎙️ <b>语音对讲专属模式</b>：按住遥控器语音键时开始录音并注入快捷键，松开时停止录音并释放快捷键。
            </div>

            <!-- Keyboard Direct Capture Box -->
            <div class="key-recorder-box" id="key-recorder-box" tabindex="0" onclick="startKeyboardRecording()">
                <div style="font-size: 13px; color: var(--text-muted);" id="recorder-instruction">
                    ⌨️ <b>直接在键盘上按下任意按键或快捷键</b>（支持单键与 Ctrl/Alt/Win/Shift 组合键）
                </div>
                <div class="key-badge-display" id="recorded-badge-display">
                    <span style="color: var(--text-muted); font-size: 16px; font-weight: normal;">点击此处开始按键录制...</span>
                </div>
            </div>

            <!-- Advanced Manual Key Code & Quick Select Area (Always Expanded) -->
            <div style="background: #090d16; border: 1px solid var(--border-color); border-radius: 12px; padding: 16px; margin-bottom: 18px;">
                <div style="font-size: 13px; font-weight: 600; color: var(--text-main); margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center;">
                    <span>🛠️ 快捷选择与键码微调</span>
                </div>

                <!-- Quick Key Dropdown -->
                <div style="margin-bottom: 12px;">
                    <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:4px;">⚡ 快速选择特殊按键 / 多媒体功能</label>
                    <select id="quick-key-select" onchange="onQuickKeySelect(this.value)" style="width:100%; padding:8px 10px; background:#151d2a; border:1px solid #243247; color:#fff; border-radius:8px; font-size:13px; outline:none;">
                        <option value="">-- 点击选择常见按键 / 组合键 / 多媒体 --</option>
                        <optgroup label="常用控制键">
                            <option value="k:0:0x28">回车键 (Enter)</option>
                            <option value="k:0:0x29">Esc 键 (Escape)</option>
                            <option value="k:0:0x2C">空格键 (Space)</option>
                            <option value="k:0:0x2B">Tab 键</option>
                            <option value="k:0:0x2A">退格键 (Backspace)</option>
                            <option value="k:0:0x4C">删除键 (Delete)</option>
                            <option value="k:0:0x39">大写锁定 (CapsLock)</option>
                            <option value="k:0:0x46">屏幕截图 (PrintScreen)</option>
                        </optgroup>
                        <optgroup label="单修饰键 (直接触发)">
                            <option value="m:0x08:0">Windows 徽标键 (Win)</option>
                            <option value="m:0x01:0">Control 键 (Ctrl)</option>
                            <option value="m:0x04:0">Alt 键</option>
                            <option value="m:0x02:0">Shift 键</option>
                        </optgroup>
                        <optgroup label="方向与翻页导航">
                            <option value="k:0:0x52">方向上 (Arrow Up)</option>
                            <option value="k:0:0x51">方向下 (Arrow Down)</option>
                            <option value="k:0:0x50">方向左 (Arrow Left)</option>
                            <option value="k:0:0x4F">方向右 (Arrow Right)</option>
                            <option value="k:0:0x4B">上一页 (PageUp)</option>
                            <option value="k:0:0x4E">下一页 (PageDown)</option>
                            <option value="k:0:0x4A">行首 (Home)</option>
                            <option value="k:0:0x4D">行尾 (End)</option>
                        </optgroup>
                        <optgroup label="功能键 (F1 ~ F12)">
                            <option value="k:0:0x3A">F1</option>
                            <option value="k:0:0x3B">F2</option>
                            <option value="k:0:0x3C">F3</option>
                            <option value="k:0:0x3D">F4</option>
                            <option value="k:0:0x3E">F5 (刷新)</option>
                            <option value="k:0:0x3F">F6</option>
                            <option value="k:0:0x40">F7</option>
                            <option value="k:0:0x41">F8</option>
                            <option value="k:0:0x42">F9</option>
                            <option value="k:0:0x43">F10</option>
                            <option value="k:0:0x44">F11 (全屏)</option>
                            <option value="k:0:0x45">F12 (开发者工具)</option>
                        </optgroup>
                        <optgroup label="常用快捷组合键">
                            <option value="k:0x04:0x36">Alt + , (豆包/AI语音助手)</option>
                            <option value="k:0x08:0x0B">Win + H (Windows语音听写)</option>
                            <option value="k:0x08:0x07">Win + D (显示/隐藏桌面)</option>
                            <option value="k:0x04:0x2B">Alt + Tab (切换窗口任务)</option>
                            <option value="k:0x04:0x3D">Alt + F4 (关闭当前窗口)</option>
                            <option value="k:0x01:0x06">Ctrl + C (复制)</option>
                            <option value="k:0x01:0x19">Ctrl + V (粘贴)</option>
                            <option value="k:0x01:0x1D">Ctrl + Z (撤销)</option>
                        </optgroup>
                        <optgroup label="多媒体与系统控制 (仅普通按键)" id="quick-optgroup-media">
                            <option value="c:0:545">🔊 音量增加 (Volume Up)</option>
                            <option value="c:0:546">🔉 音量减少 (Volume Down)</option>
                            <option value="c:0:547">🔇 静音 (Mute)</option>
                            <option value="c:0:516">⏯️ 播放 / 暂停 (Play/Pause)</option>
                            <option value="c:0:537">⏭️ 下一曲 (Next Track)</option>
                            <option value="c:0:538">⏮️ 上一曲 (Previous Track)</option>
                            <option value="c:0:530">🌙 系统休眠 (Sleep)</option>
                            <option value="c:0:558">🔙 网页/应用返回 (AC Back)</option>
                            <option value="c:0:557">⌂ 网页/系统主页 (AC Home)</option>
                        </optgroup>
                    </select>
                </div>

                <!-- Numerical Inputs -->
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
                    <div>
                        <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:4px;">修饰键 (Mod: 1=Ctrl, 2=Shift, 4=Alt, 8=Win)</label>
                        <input type="number" id="adv-mod" value="0" min="0" max="255" oninput="onAdvInputChanged()" style="width:100%; padding:8px; background:#151d2a; border:1px solid #243247; color:#fff; border-radius:8px; font-size:13px; outline:none;">
                    </div>
                    <div>
                        <label style="display:block; font-size:12px; color:var(--text-muted); margin-bottom:4px;">按键码 (HID Key 或 Consumer 代码)</label>
                        <input type="number" id="adv-code" value="0" min="0" max="65535" oninput="onAdvInputChanged()" style="width:100%; padding:8px; background:#151d2a; border:1px solid #243247; color:#fff; border-radius:8px; font-size:13px; outline:none;">
                    </div>
                </div>
            </div>

            <!-- Action Buttons -->
            <div style="display: flex; justify-content: space-between; align-items: center; margin-top: 20px; border-top: 1px solid var(--border-color); padding-top: 16px;">
                <button class="btn btn-outline" style="font-size: 13px; color: var(--accent-red); border-color: rgba(239,68,68,0.3);" onclick="clearCurrentKeyBinding()">🗑️ 清空映射 (禁用此键)</button>
                <div style="display: flex; gap: 10px;">
                    <button class="btn btn-outline" onclick="closeRemapModal()">取消</button>
                    <button class="btn" onclick="saveRemapConfig()">💾 保存映射</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let currentKeymap = { bindings: [] };
        let editingKey = 0;
        let activeTrigger = 'click';
        let currentActionState = { type: 1, mod: 0, key: 0, cons: 0, text: '无' };
        let clearHighlightTimer = null;

        const KEY_NAMES = {
            0x66: '电源键 (Power)',
            0xFF: '电源键 (Power)',
            0x04: '语音键 (Voice)',
            0x52: '方向上 (Up)',
            0x51: '方向下 (Down)',
            0x50: '方向左 (Left)',
            0x4F: '方向右 (Right)',
            0x28: '确定键 (OK)',
            0xF1: '返回键 (Back)',
            0x24: '主页键 (Home)',
            0x4A: '主页键 (Home)',
            0x5D: '菜单键 (Menu)',
            0x65: '菜单键 (Menu)',
            0x80: '音量+ (Vol+)',
            0x81: '音量- (Vol-)',
            0xC0: '电视键 (TV)',
            0x35: '电视键 (TV)'
        };

        // DOM Key -> USB HID Keyboard Code Map
        const DOM_TO_HID = {
            'KeyA': 0x04, 'KeyB': 0x05, 'KeyC': 0x06, 'KeyD': 0x07, 'KeyE': 0x08,
            'KeyF': 0x09, 'KeyG': 0x0A, 'KeyH': 0x0B, 'KeyI': 0x0C, 'KeyJ': 0x0D,
            'KeyK': 0x0E, 'KeyL': 0x0F, 'KeyM': 0x10, 'KeyN': 0x11, 'KeyO': 0x12,
            'KeyP': 0x13, 'KeyQ': 0x14, 'KeyR': 0x15, 'KeyS': 0x16, 'KeyT': 0x17,
            'KeyU': 0x18, 'KeyV': 0x19, 'KeyW': 0x1A, 'KeyX': 0x1B, 'KeyY': 0x1C, 'KeyZ': 0x1D,
            'Digit1': 0x1E, 'Digit2': 0x1F, 'Digit3': 0x20, 'Digit4': 0x21, 'Digit5': 0x22,
            'Digit6': 0x23, 'Digit7': 0x24, 'Digit8': 0x25, 'Digit9': 0x26, 'Digit0': 0x27,
            'Enter': 0x28, 'Escape': 0x29, 'Backspace': 0x2A, 'Tab': 0x2B, 'Space': 0x2C,
            'Minus': 0x2D, 'Equal': 0x2E, 'BracketLeft': 0x2F, 'BracketRight': 0x30,
            'Backslash': 0x31, 'Semicolon': 0x33, 'Quote': 0x34, 'Backquote': 0x35,
            'Comma': 0x36, 'Period': 0x37, 'Slash': 0x38, 'CapsLock': 0x39,
            'F1': 0x3A, 'F2': 0x3B, 'F3': 0x3C, 'F4': 0x3D, 'F5': 0x3E, 'F6': 0x3F,
            'F7': 0x40, 'F8': 0x41, 'F9': 0x42, 'F10': 0x43, 'F11': 0x44, 'F12': 0x45,
            'PrintScreen': 0x46, 'ScrollLock': 0x47, 'Pause': 0x48, 'Insert': 0x49,
            'Home': 0x4A, 'PageUp': 0x4B, 'Delete': 0x4C, 'End': 0x4D, 'PageDown': 0x4E,
            'ArrowRight': 0x4F, 'ArrowLeft': 0x50, 'ArrowDown': 0x51, 'ArrowUp': 0x52
        };

        function switchTab(id) {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            event.target.classList.add('active');
            document.getElementById(id).classList.add('active');
        }

        async function fetchStatus() {
            try {
                const res = await fetch('/api/status');
                const d = await res.json();
                document.getElementById('top-status').innerHTML = `固件: ${d.version} | 运行: ${d.uptime_sec}s | IP: ${d.sta_ip}`;
                document.getElementById('stat-sta-ip').innerText = d.sta_ip;
                document.getElementById('stat-uptime').innerText = `${d.uptime_sec}s`;
                document.getElementById('stat-audio-frames').innerText = `${d.frames_decoded} 帧`;
                document.getElementById('stat-mem').innerText = `Heap: ${Math.round(d.free_heap/1024)}KB | PSRAM: ${Math.round(d.free_psram/1024/1024)}MB`;
                
                const bleInfoRes = await fetch('/api/ble/info');
                const bleInfo = await bleInfoRes.json();
                if (bleInfo.connected) {
                    document.getElementById('stat-ble-state').innerText = '已连接';
                    document.getElementById('stat-ble-state').style.color = 'var(--accent-green)';
                    document.getElementById('stat-ble-name').innerText = bleInfo.name || '小米蓝牙语音遥控器';
                } else {
                    document.getElementById('stat-ble-state').innerText = '扫描重连中...';
                    document.getElementById('stat-ble-state').style.color = 'var(--accent-orange)';
                    document.getElementById('stat-ble-name').innerText = bleInfo.bound_mac ? `已绑定: ${bleInfo.bound_mac}` : '未绑定遥控器';
                }
            } catch(e){}
        }

        async function fetchKeyTelemetry() {
            try {
                const res = await fetch('/api/keymap/telemetry');
                const t = await res.json();

                if (t.source_vk && t.source_vk !== 0) {
                    const vk = t.source_vk;
                    const hexCode = '0x' + vk.toString(16).toUpperCase().padStart(2, '0');
                    const btnName = KEY_NAMES[vk] || `按键 ${hexCode}`;
                    
                    document.getElementById('live-key-code').innerText = hexCode;
                    document.getElementById('live-key-state').innerText = t.is_pressed ? 'DOWN (按下)' : 'UP (松开)';
                    document.getElementById('live-key-state').style.color = t.is_pressed ? 'var(--accent-cyan)' : 'var(--accent-green)';
                    document.getElementById('live-key-dur').innerText = `${t.duration_ms || 0} ms`;
                    document.getElementById('live-act-type').innerText = `TYPE_${t.action_type || 0}`;
                    document.getElementById('live-act-val').innerText = `Key: 0x${(t.key_code||0).toString(16)} Cons: 0x${(t.consumer_code||0).toString(16)}`;

                    // Find DOM element
                    let targetHex = hexCode;
                    if (vk === 0xFF) targetHex = '0x66';
                    if (vk === 0x4A) targetHex = '0x24';
                    if (vk === 0x65) targetHex = '0x5D';
                    if (vk === 0x35) targetHex = '0xC0';

                    const btnEl = document.getElementById(`btn-${targetHex}`);
                    if (btnEl) {
                        document.getElementById('live-key-name').innerText = btnName;
                        btnEl.classList.add('pressed');
                        
                        if (clearHighlightTimer) clearTimeout(clearHighlightTimer);
                        clearHighlightTimer = setTimeout(() => {
                            btnEl.classList.remove('pressed');
                        }, 250);
                    }
                }
            } catch(e){}
        }

        async function loadKeymap() {
            try {
                const res = await fetch('/api/keymap');
                currentKeymap = await res.json();
            } catch(e){}
        }

        function openRemapModal(keyVk, keyName) {
            editingKey = keyVk;
            const isVoice = (keyVk === 0x04 || keyVk === 0x3E);
            
            document.getElementById('modal-title').innerText = isVoice ? '设置 语音键 (Voice) 呼出快捷键' : `设置 ${keyName} 映射`;
            document.getElementById('modal-vk-badge').innerText = '0x' + keyVk.toString(16).toUpperCase().padStart(2, '0');
            document.getElementById('voice-key-banner').style.display = isVoice ? 'block' : 'none';
            document.getElementById('quick-optgroup-media').style.display = isVoice ? 'none' : 'block';

            // Find current binding
            const b = currentKeymap.bindings.find(x => x.source_vk === keyVk || (isVoice && x.source_vk === 0x04));
            if (b && b.has_click) {
                renderCurrentAction(b.click_type, b.click_mod, b.click_key, b.click_cons);
            } else {
                renderCurrentAction(0, 0, 0, 0);
            }

            document.getElementById('quick-key-select').value = '';
            document.getElementById('remap-modal').style.display = 'flex';
            startKeyboardRecording();
        }

        function closeRemapModal() {
            document.getElementById('remap-modal').style.display = 'none';
        }

        function renderCurrentAction(type, mod, key, cons) {
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);
            if (isVoice) {
                type = 7; // Fixed to ACTION_VOICE_HOLD
                cons = 0;
            } else if (cons > 0) {
                type = 5; // Fixed to ACTION_CONSUMER_HOLD
                key = 0;
                mod = 0;
            } else if (key > 0 || mod > 0) {
                type = 2; // Fixed to ACTION_KEYBOARD_HOLD
                cons = 0;
            } else {
                type = 0;
            }

            currentActionState = { type, mod, key, cons };
            document.getElementById('adv-mod').value = mod || 0;
            document.getElementById('adv-code').value = key || cons || 0;

            const display = document.getElementById('recorded-badge-display');
            if (type === 0 || (!key && !cons && !mod)) {
                display.innerHTML = '<span style="color: var(--text-muted); font-size: 15px; font-weight: normal;">未设置（点击此处敲键盘录制，或从下方快速选择）</span>';
                return;
            }

            let chips = [];
            if (mod & 0x01) chips.push('Ctrl');
            if (mod & 0x04) chips.push('Alt');
            if (mod & 0x02) chips.push('Shift');
            if (mod & 0x08) chips.push('Win');

            if (cons > 0) {
                const consMap = {
                    545: '🔊 音量 +',
                    546: '🔉 音量 -',
                    547: '🔇 静音',
                    516: '⏯️ 播放/暂停',
                    537: '⏭️ 下一曲',
                    538: '⏮️ 上一曲',
                    539: '⏹️ 停止',
                    530: '🌙 系统休眠',
                    558: '🔙 网页返回',
                    557: '⌂ 网页主页'
                };
                chips.push(consMap[cons] || `多媒体 0x${cons.toString(16)}`);
            } else if (key > 0) {
                let keyName = `Key(0x${key.toString(16).toUpperCase()})`;
                for (let k in DOM_TO_HID) {
                    if (DOM_TO_HID[k] === key) {
                        keyName = k.replace('Key', '').replace('Digit', '').replace('Arrow', '');
                        break;
                    }
                }
                chips.push(keyName);
            }

            display.innerHTML = chips.map(c => `<span class="kbd-chip">${c}</span>`).join(' + ');
        }

        function onQuickKeySelect(val) {
            if (!val) return;
            const parts = val.split(':');
            const prefix = parts[0];
            const mod = parseInt(parts[1], 16) || 0;
            const code = parseInt(parts[2], parts[2].startsWith('0x') ? 16 : 10) || 0;
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);

            if (prefix === 'c') {
                if (isVoice) {
                    alert('语音键专用于语音录音与呼出快捷键，不可设为多媒体键');
                    document.getElementById('quick-key-select').value = '';
                    return;
                }
                renderCurrentAction(5, 0, 0, code);
            } else if (prefix === 'm') {
                renderCurrentAction(isVoice ? 7 : 2, mod, 0, 0);
            } else if (prefix === 'k') {
                renderCurrentAction(isVoice ? 7 : 2, mod, code, 0);
            }
        }

        function onAdvInputChanged() {
            const mod = parseInt(document.getElementById('adv-mod').value) || 0;
            const code = parseInt(document.getElementById('adv-code').value) || 0;
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);

            if (isVoice) {
                renderCurrentAction(7, mod, code, 0);
            } else if (code >= 500) {
                renderCurrentAction(5, 0, 0, code);
            } else {
                renderCurrentAction((code > 0 || mod > 0) ? 2 : 0, mod, code, 0);
            }
        }

        function startKeyboardRecording() {
            const box = document.getElementById('key-recorder-box');
            box.focus();
            box.classList.add('recording');
        }

        // Global Keyboard Event Capturer for Ultra-Intuitive Remapping
        window.addEventListener('keydown', function(e) {
            const modal = document.getElementById('remap-modal');
            if (modal.style.display !== 'flex') return;

            // If user is typing in advanced numeric inputs, let it through
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;

            e.preventDefault();
            e.stopPropagation();

            // Ignore standalone modifier presses (wait for actual key)
            if (['Control', 'Alt', 'Shift', 'Meta'].includes(e.key)) return;

            let mod = 0;
            if (e.ctrlKey) mod |= 0x01; // LCTRL
            if (e.shiftKey) mod |= 0x02; // LSHIFT
            if (e.altKey) mod |= 0x04; // LALT
            if (e.metaKey) mod |= 0x08; // LGUI (Win)

            const hidCode = DOM_TO_HID[e.code] || 0;
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);

            if (hidCode > 0) {
                renderCurrentAction(isVoice ? 7 : 2, mod, hidCode, 0);
            }
        });

        function clearCurrentKeyBinding() {
            renderCurrentAction(0, 0, 0, 0);
            document.getElementById('quick-key-select').value = '';
        }

        async function saveRemapConfig() {
            const isVoice = (editingKey === 0x04 || editingKey === 0x3E);
            let b = currentKeymap.bindings.find(x => x.source_vk === editingKey || (isVoice && x.source_vk === 0x04));
            if (!b) {
                b = { source_vk: isVoice ? 0x04 : editingKey };
                currentKeymap.bindings.push(b);
            }

            let actType = currentActionState.type;
            let mod = currentActionState.mod;
            let key = currentActionState.key;
            let cons = currentActionState.cons;

            if (isVoice) {
                b.source_vk = 0x04;
                b.has_click = true;
                b.click_type = 7; // ACTION_VOICE_HOLD
                b.click_mod = mod;
                b.click_key = key;
                b.click_cons = 0;
            } else if (cons > 0) {
                b.has_click = true;
                b.click_type = 5; // ACTION_CONSUMER_HOLD
                b.click_mod = 0;
                b.click_key = 0;
                b.click_cons = cons;
            } else if (key > 0 || mod > 0) {
                b.has_click = true;
                b.click_type = 2; // ACTION_KEYBOARD_HOLD
                b.click_mod = mod;
                b.click_key = key;
                b.click_cons = 0;
            } else {
                b.has_click = false;
                b.click_type = 0;
                b.click_mod = 0;
                b.click_key = 0;
                b.click_cons = 0;
            }

            // Remove legacy complex timers for 100% natural transparent physical forwarding
            b.has_long = false;
            b.has_double = false;

            await fetch('/api/keymap/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentKeymap)
            });

            closeRemapModal();
            alert('按键映射已保存到板载存储并立即生效！');
        }

        async function resetAllKeymaps() {
            if (!confirm('确定要将所有按键映射恢复为出厂默认值吗？')) return;
            await fetch('/api/keymap/reset', { method: 'POST' });
            await loadKeymap();
            alert('已恢复出厂按键映射！');
        }

        async function scanBleDevices() {
            const container = document.getElementById('ble-dev-list');
            container.innerHTML = '正在扫描周围蓝牙设备 (4秒)...';
            try {
                const res = await fetch('/api/ble/scan');
                const d = await res.json();
                if (!d.devices || d.devices.length === 0) {
                    container.innerHTML = '<div style="color:var(--text-muted);">未发现附近设备，请确保遥控器处于配对广播状态。</div>';
                    return;
                }
                let html = '<div style="display:grid; gap:10px;">';
                d.devices.forEach(dev => {
                    html += `<div style="display:flex; justify-content:space-between; align-items:center; background:#0b0f17; padding:12px; border-radius:8px; border:1px solid #243247;">
                        <div><b>${dev.name}</b> <span style="font-size:12px; color:var(--text-muted); font-family:monospace;">(${dev.mac}) RSSI: ${dev.rssi}dBm</span></div>
                        <button class="btn" style="padding:6px 14px; font-size:12px;" onclick="connectMac('${dev.mac}')">连接</button>
                    </div>`;
                });
                html += '</div>';
                container.innerHTML = html;
            } catch(e){ container.innerText = '扫描出错: ' + e; }
        }

        async function connectMac(mac) {
            const res = await fetch('/api/ble/connect', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ mac }) });
            alert('正在连接目标蓝牙遥控器，请查看运行日志...');
        }

        async function scanWifiNetworks() {
            const list = document.getElementById('wifi-scan-list');
            list.innerHTML = '正在搜索周围 2.4GHz Wi-Fi 网络...';
            try {
                const res = await fetch('/api/wifi/scan');
                const d = await res.json();
                if (!d.networks || d.networks.length === 0) {
                    list.innerHTML = '未扫描到无线网络';
                    return;
                }
                let html = '<div style="display:flex; flex-wrap:wrap; gap:8px; margin-top:8px;">';
                d.networks.forEach(net => {
                    if (net.ssid) {
                        html += `<button class="btn btn-outline" style="font-size:12px; padding:6px 12px;" onclick="selectWifi('${net.ssid}')">📶 ${net.ssid} (${net.rssi}dBm)</button>`;
                    }
                });
                html += '</div>';
                list.innerHTML = html;
            } catch(e){ list.innerText = '搜索出错: ' + e; }
        }

        function selectWifi(ssid) {
            document.getElementById('wifi-ssid').value = ssid;
            document.getElementById('wifi-pass').focus();
        }

        async function refreshLogs() {
            try {
                const res = await fetch('/api/logs');
                const d = await res.json();
                const terminal = document.getElementById('log-terminal');
                terminal.innerText = d.logs.join('\n');
                terminal.scrollTop = terminal.scrollHeight;
            } catch(e){}
        }

        async function clearLogs() {
            await fetch('/api/logs/clear', { method: 'POST' });
            refreshLogs();
        }

        async function saveWifi() {
            const ssid = document.getElementById('wifi-ssid').value;
            const pass = document.getElementById('wifi-pass').value;
            if (!ssid) return alert('请输入 Wi-Fi 名称');
            await fetch('/api/wifi/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ssid, pass }) });
            alert('Wi-Fi 配置已保存，ESP32 正在尝试连接！');
        }

        async function restartDevice() {
            if (!confirm('确定要重启 ESP32-S3 设备吗？')) return;
            await fetch('/api/system/restart', { method: 'POST' });
            alert('正在重启...');
        }

        // Periodic background pollers
        setInterval(fetchKeyTelemetry, 100);
        setInterval(fetchStatus, 3000);
        setInterval(refreshLogs, 2000);
        loadKeymap();
        fetchStatus();
    </script>
</body>
</html>
)rawliteral";
