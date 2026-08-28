#pragma once

#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RemoteMapper 控制台</title>
  <style>
    :root {
      --bg: #0d1117;
      --card-bg: #161b22;
      --card-border: #30363d;
      --accent: #58a6ff;
      --accent-glow: rgba(88, 166, 255, 0.2);
      --success: #238636;
      --success-glow: rgba(35, 134, 54, 0.3);
      --warning: #d29922;
      --danger: #da3633;
      --text: #c9d1d9;
      --text-bright: #f0f6fc;
      --text-muted: #8b949e;
      --radius: 8px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "PingFang SC", "Microsoft YaHei", sans-serif; }
    body { background-color: var(--bg); color: var(--text); padding: 16px; font-size: 14px; line-height: 1.5; }
    .container { max-width: 960px; margin: 0 auto; }
    header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 16px; border-bottom: 1px solid var(--card-border); margin-bottom: 20px; }
    .logo { display: flex; align-items: center; gap: 10px; }
    .logo-icon { font-size: 24px; background: var(--accent-glow); padding: 8px; border-radius: 8px; }
    .logo h1 { font-size: 18px; color: var(--text-bright); }
    .logo span { font-size: 12px; color: var(--text-muted); }
    .status-pills { display: flex; gap: 8px; flex-wrap: wrap; }
    .pill { font-size: 12px; padding: 4px 10px; border-radius: 12px; border: 1px solid var(--card-border); background: var(--card-bg); }
    .pill.green { color: #3fb950; border-color: #238636; }
    .pill.yellow { color: #d29922; border-color: #9e6a03; }
    .pill.blue { color: #58a6ff; border-color: #1f6feb; }

    /* Tabs */
    .tabs { display: flex; gap: 8px; margin-bottom: 20px; border-bottom: 1px solid var(--card-border); padding-bottom: 8px; overflow-x: auto; }
    .tab-btn { background: none; border: none; color: var(--text-muted); padding: 8px 16px; font-size: 14px; font-weight: 500; cursor: pointer; border-radius: var(--radius); transition: all 0.2s; white-space: nowrap; }
    .tab-btn:hover { color: var(--text-bright); background: rgba(255,255,255,0.05); }
    .tab-btn.active { color: var(--accent); background: var(--accent-glow); }
    .tab-content { display: none; }
    .tab-content.active { display: block; }

    /* Grid & Cards */
    .grid-2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 16px; margin-bottom: 20px; }
    .card { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: var(--radius); padding: 16px; margin-bottom: 16px; }
    .card-title { font-size: 15px; color: var(--text-bright); font-weight: 600; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }
    .stat-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255,255,255,0.05); }
    .stat-row:last-child { border-bottom: none; }
    .stat-label { color: var(--text-muted); }
    .stat-val { color: var(--text-bright); font-weight: 500; font-family: Consolas, monospace; }

    /* Buttons & Inputs */
    .btn { background: #21262d; color: var(--text-bright); border: 1px solid var(--card-border); padding: 8px 14px; border-radius: var(--radius); cursor: pointer; font-size: 13px; font-weight: 500; transition: all 0.2s; display: inline-flex; align-items: center; gap: 6px; }
    .btn:hover { background: #30363d; border-color: #8b949e; }
    .btn-primary { background: #1f6feb; border-color: #388bfd; color: #fff; }
    .btn-primary:hover { background: #388bfd; }
    .btn-success { background: #238636; border-color: #2ea043; color: #fff; }
    .btn-success:hover { background: #2ea043; }
    .btn-danger { background: #da3633; border-color: #f85149; color: #fff; }
    .btn-danger:hover { background: #f85149; }
    .form-group { margin-bottom: 12px; }
    .form-label { display: block; margin-bottom: 4px; color: var(--text-muted); font-size: 12px; }
    .form-control { width: 100%; padding: 8px 12px; background: #0d1117; border: 1px solid var(--card-border); border-radius: var(--radius); color: var(--text-bright); font-size: 13px; outline: none; }
    .form-control:focus { border-color: var(--accent); box-shadow: 0 0 0 2px var(--accent-glow); }

    /* Log Box */
    .log-box { background: #010409; border: 1px solid var(--card-border); border-radius: var(--radius); height: 360px; overflow-y: auto; padding: 12px; font-family: Consolas, monospace; font-size: 12px; line-height: 1.6; color: #7ee787; }
    .log-line { margin-bottom: 2px; word-break: break-all; }

    /* List items */
    .dev-item { display: flex; justify-content: space-between; align-items: center; padding: 12px; background: #0d1117; border: 1px solid var(--card-border); border-radius: var(--radius); margin-bottom: 8px; }
    .dev-name { font-weight: 600; color: var(--text-bright); }
    .dev-mac { font-family: monospace; font-size: 12px; color: var(--text-muted); }
    .key-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; background: #0d1117; border: 1px solid var(--card-border); border-radius: var(--radius); margin-bottom: 8px; }
    .key-badge { background: #21262d; padding: 4px 8px; border-radius: 4px; font-family: monospace; font-size: 12px; color: var(--accent); }
    .key-desc { font-size: 12px; color: var(--text-muted); }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="logo">
        <div class="logo-icon">🎙️</div>
        <div>
          <h1>RemoteMapper ESP32-S3</h1>
          <span>小米蓝牙语音遥控器 硬件复合中继系统</span>
        </div>
      </div>
      <div class="status-pills">
        <div class="pill yellow" id="pill-ble">BLE: 搜索中</div>
        <div class="pill blue" id="pill-wifi">Wi-Fi: AP+STA</div>
        <div class="pill yellow" id="pill-sta-ip">IP: 192.168.4.1</div>
      </div>
    </header>

    <div class="tabs">
      <button class="tab-btn active" onclick="showTab('dashboard')">📊 状态仪表盘</button>
      <button class="tab-btn" onclick="showTab('ble')">📡 蓝牙管理与配对</button>
      <button class="tab-btn" onclick="showTab('keymap')">🎮 按键配置</button>
      <button class="tab-btn" onclick="showTab('wifi')">📶 Wi-Fi 配网</button>
      <button class="tab-btn" onclick="showTab('logs')">📜 实时日志</button>
    </div>

    <!-- 1. Dashboard Tab -->
    <div id="tab-dashboard" class="tab-content active">
      <div class="grid-2">
        <div class="card">
          <div class="card-title">🎙️ BLE 遥控器与音频流</div>
          <div class="stat-row"><span class="stat-label">连接状态</span><span class="stat-val" id="stat-ble-state">Scanning...</span></div>
          <div class="stat-row"><span class="stat-label">已绑定遥控器</span><span class="stat-val" id="stat-bound-remote">未绑定</span></div>
          <div class="stat-row"><span class="stat-label">音频规格</span><span class="stat-val">16kHz 16-bit Mono (UAC 1.0)</span></div>
          <div class="stat-row"><span class="stat-label">已解码音频帧</span><span class="stat-val" id="stat-frames">0 帧</span></div>
          <div class="stat-row"><span class="stat-label">已推流采样点</span><span class="stat-val" id="stat-samples">0 点</span></div>
          <div style="margin-top: 14px; display: flex; gap: 8px;">
            <button class="btn btn-primary" onclick="showTab('ble')">📡 前往蓝牙配对</button>
            <button class="btn" onclick="apiAction('/api/ble/reconnect')">🔄 重新扫描</button>
          </div>
        </div>

        <div class="card">
          <div class="card-title">⚡ 硬件与系统资源</div>
          <div class="stat-row"><span class="stat-label">主控芯片</span><span class="stat-val">ESP32-S3 N16R8</span></div>
          <div class="stat-row"><span class="stat-label">运行时间</span><span class="stat-val" id="stat-uptime">0s</span></div>
          <div class="stat-row"><span class="stat-label">空闲堆内存 (Heap)</span><span class="stat-val" id="stat-heap">0 KB</span></div>
          <div class="stat-row"><span class="stat-label">空闲 PSRAM</span><span class="stat-val" id="stat-psram">0 KB</span></div>
          <div style="margin-top: 14px; display: flex; gap: 8px;">
            <button class="btn btn-danger" onclick="apiAction('/api/system/restart')">⚠️ 重启 ESP32</button>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-title">🌐 网络访问信息</div>
        <div class="stat-row"><span class="stat-label">AP 热点 IP (直连)</span><span class="stat-val">192.168.4.1 (SSID: RemoteMapper-AP)</span></div>
        <div class="stat-row"><span class="stat-label">家庭局域网 IP (STA)</span><span class="stat-val" id="stat-sta-ip">获取中...</span></div>
        <div class="stat-row"><span class="stat-label">局域网 mDNS 快速访问</span><span class="stat-val"><a href="http://remotemapper.local" target="_blank" style="color:var(--accent);">http://remotemapper.local</a></span></div>
      </div>
    </div>

    <!-- 2. BLE Management Tab -->
    <div id="tab-ble" class="tab-content">
      <div class="card">
        <div class="card-title">
          <span>🔗 当前绑定设备</span>
          <button class="btn btn-danger" onclick="unpairBle()">❌ 解除绑定</button>
        </div>
        <div class="stat-row"><span class="stat-label">设备名称</span><span class="stat-val" id="ble-info-name">未连接</span></div>
        <div class="stat-row"><span class="stat-label">MAC 地址</span><span class="stat-val" id="ble-info-mac">--</span></div>
      </div>

      <div class="card">
        <div class="card-title">
          <span>📡 扫描周围蓝牙设备 (手动配对)</span>
          <button class="btn btn-primary" id="btn-scan-ble" onclick="scanBle()">🔍 开始扫描 (4秒)</button>
        </div>
        <p style="color:var(--text-muted); margin-bottom: 10px;">
          提示：请先长按遥控器 <strong>主页键 + 菜单键</strong> 约 3 秒（指示灯闪烁），然后点击上方扫描按钮，在下方列表中点击“连接”。
        </p>
        <div id="ble-list">
          <p style="color:var(--text-muted); padding: 12px; text-align: center;">暂未扫描，请点击上方按钮扫描周围蓝牙遥控器...</p>
        </div>
      </div>
    </div>

    <!-- 3. Keymap Tab -->
    <div id="tab-keymap" class="tab-content">
      <div class="card">
        <div class="card-title">
          <span>🎮 物理按键映射规则</span>
          <button class="btn btn-primary" onclick="resetKeymap()">恢复默认映射</button>
        </div>
        <div class="key-item">
          <div><strong>音量加 (+) / 音量减 (-)</strong><div class="key-desc">0x80 / 0x81 (原始 Android 键码)</div></div>
          <div class="key-badge">USB Consumer Vol Up/Down (连续连发)</div>
        </div>
        <div class="key-item">
          <div><strong>返回键 (Back)</strong><div class="key-desc">0xF1 (原始 Android 键码)</div></div>
          <div class="key-badge">USB Consumer AC Back (浏览器/播放器后退)</div>
        </div>
        <div class="key-item">
          <div><strong>语音键 (Voice HTT)</strong><div class="key-desc">按住说话 / 松开结束</div></div>
          <div class="key-badge">UAC 麦克风音频流 + 注入 RAlt+Comma</div>
        </div>
        <div class="key-item">
          <div><strong>电源键 (Power)</strong><div class="key-desc">单击 / 长按</div></div>
          <div class="key-badge">单击: Alt+Tab | 长按: Sleep</div>
        </div>
        <div class="key-item">
          <div><strong>主页键 (Home)</strong><div class="key-desc">单击</div></div>
          <div class="key-badge">USB Keyboard Win+D (显示桌面)</div>
        </div>
        <div class="key-item">
          <div><strong>菜单键 (Menu)</strong><div class="key-desc">单击</div></div>
          <div class="key-badge">USB Keyboard Space (播放/暂停)</div>
        </div>
        <div class="key-item">
          <div><strong>直播/TV 键</strong><div class="key-desc">单击</div></div>
          <div class="key-badge">USB Keyboard F8</div>
        </div>
      </div>
    </div>

    <!-- 4. Wi-Fi Tab -->
    <div id="tab-wifi" class="tab-content">
      <div class="card">
        <div class="card-title">📶 连接家庭局域网 Wi-Fi</div>
        <p style="color:var(--text-muted); margin-bottom: 12px;">配置连接路由器后，您可以在家庭局域网内任意手机或电脑通过 <strong>http://remotemapper.local</strong> 直接打开本配置面板。</p>
        <div class="form-group">
          <label class="form-label">周围 2.4GHz Wi-Fi</label>
          <div style="display:flex; gap:8px;">
            <select class="form-control" id="wifi-ssid-select" onchange="document.getElementById('wifi-ssid').value = this.value">
              <option value="">-- 点击右侧扫描获取列表 --</option>
            </select>
            <button class="btn" onclick="scanWifi()">🔍 扫描</button>
          </div>
        </div>
        <div class="form-group">
          <label class="form-label">Wi-Fi 名称 (SSID)</label>
          <input type="text" class="form-control" id="wifi-ssid" placeholder="输入或上方选择 Wi-Fi 名称">
        </div>
        <div class="form-group">
          <label class="form-label">Wi-Fi 密码</label>
          <input type="password" class="form-control" id="wifi-pass" placeholder="输入 Wi-Fi 密码">
        </div>
        <button class="btn btn-success" onclick="saveWifi()">💾 保存并连接</button>
      </div>
    </div>

    <!-- 5. Logs Tab -->
    <div id="tab-logs" class="tab-content">
      <div class="card">
        <div class="card-title">
          <span>📜 ESP32-S3 实时运行日志</span>
          <div style="display:flex; gap:8px;">
            <button class="btn" onclick="refreshLogs()">🔄 刷新</button>
            <button class="btn btn-danger" onclick="clearLogs()">🧹 清空</button>
          </div>
        </div>
        <div class="log-box" id="log-container">加载日志中...</div>
      </div>
    </div>
  </div>

  <script>
    function showTab(id) {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      event.target.classList.add('active');
      document.getElementById('tab-' + id).classList.add('active');
      if (id === 'logs') refreshLogs();
      if (id === 'ble') fetchBleInfo();
    }

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        const stateText = data.ble_state === 3 ? 'Connected (已连接)' : (data.ble_state === 4 ? 'Talking (语音推流中)' : 'Scanning / Disconnected');
        document.getElementById('stat-ble-state').innerText = stateText;
        document.getElementById('pill-ble').innerText = 'BLE: ' + (data.ble_state >= 3 ? '已连接' : '未连接');
        document.getElementById('pill-ble').className = 'pill ' + (data.ble_state >= 3 ? 'green' : 'yellow');
        document.getElementById('stat-frames').innerText = (data.frames_decoded || 0) + ' 帧';
        document.getElementById('stat-samples').innerText = (data.samples_pushed || 0) + ' 点';
        document.getElementById('stat-uptime').innerText = (data.uptime_sec || 0) + ' 秒';
        document.getElementById('stat-heap').innerText = Math.round((data.free_heap || 0) / 1024) + ' KB';
        document.getElementById('stat-psram').innerText = Math.round((data.free_psram || 0) / 1024) + ' KB';
        document.getElementById('stat-sta-ip').innerText = data.sta_ip || 'Disconnected';
        document.getElementById('pill-sta-ip').innerText = 'IP: ' + (data.sta_ip || '192.168.4.1');
      } catch (e) {}
    }

    async function fetchBleInfo() {
      try {
        const res = await fetch('/api/ble/info');
        const data = await res.json();
        document.getElementById('ble-info-name').innerText = (data.name || '未连接') + (data.connected ? ' (在线)' : '');
        document.getElementById('ble-info-mac').innerText = data.mac || (data.bound_mac ? data.bound_mac + ' (已保存)' : '--');
        document.getElementById('stat-bound-remote').innerText = data.name ? (data.name + ' (' + data.mac + ')') : '未绑定';
      } catch (e) {}
    }

    async function scanBle() {
      const btn = document.getElementById('btn-scan-ble');
      const box = document.getElementById('ble-list');
      btn.innerText = '⏳ 正在扫描中...';
      btn.disabled = true;
      box.innerHTML = '<p style="color:var(--accent); padding:12px; text-align:center;">正在扫描周围蓝牙设备 (4秒)，请确保遥控器处于配对闪烁状态...</p>';

      try {
        const res = await fetch('/api/ble/scan');
        const data = await res.json();
        box.innerHTML = '';
        if (!data.devices || data.devices.length === 0) {
          box.innerHTML = '<p style="color:var(--warning); padding:12px; text-align:center;">未发现蓝牙设备，请长按遥控器 主页+菜单 键后重试！</p>';
        } else {
          data.devices.forEach(dev => {
            const isMi = dev.name.includes('小米') || dev.name.includes('MI') || dev.name.includes('Xiaomi') || dev.name.includes('Remote');
            const div = document.createElement('div');
            div.className = 'dev-item';
            div.innerHTML = `
              <div>
                <div class="dev-name">${dev.name} ${isMi ? '⭐' : ''}</div>
                <div class="dev-mac">${dev.mac} | 信号: ${dev.rssi} dBm</div>
              </div>
              <button class="btn btn-primary" onclick="connectBle('${dev.mac}')">🔗 连接此设备</button>
            `;
            box.appendChild(div);
          });
        }
      } catch (e) {
        box.innerHTML = '<p style="color:var(--danger); padding:12px; text-align:center;">扫描出错，请重试！</p>';
      } finally {
        btn.innerText = '🔍 开始扫描 (4秒)';
        btn.disabled = false;
      }
    }

    async function connectBle(mac) {
      if (confirm('确定要连接并绑定 MAC: ' + mac + ' 吗？')) {
        const res = await fetch('/api/ble/connect', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ mac })
        });
        const data = await res.json();
        if (data.status === 'connected') {
          alert('连接成功并已保存绑定！');
          fetchBleInfo();
          fetchStatus();
        } else {
          alert('连接失败，请确保遥控器在旁边且处于配对状态！');
        }
      }
    }

    async function unpairBle() {
      if (confirm('确定解除当前遥控器绑定？')) {
        await fetch('/api/ble/unpair', { method: 'POST' });
        alert('已清除绑定！');
        fetchBleInfo();
        fetchStatus();
      }
    }

    async function refreshLogs() {
      try {
        const res = await fetch('/api/logs');
        const data = await res.json();
        const box = document.getElementById('log-container');
        box.innerHTML = '';
        (data.logs || []).forEach(line => {
          const div = document.createElement('div');
          div.className = 'log-line';
          div.innerText = line;
          box.appendChild(div);
        });
        box.scrollTop = box.scrollHeight;
      } catch (e) {}
    }

    async function clearLogs() {
      await fetch('/api/logs/clear', { method: 'POST' });
      refreshLogs();
    }

    async function apiAction(url) {
      if (confirm('确定要执行此操作吗？')) {
        await fetch(url, { method: 'POST' });
        alert('操作已发送！');
      }
    }

    async function scanWifi() {
      const sel = document.getElementById('wifi-ssid-select');
      sel.innerHTML = '<option>正在扫描周围 Wi-Fi...</option>';
      try {
        const res = await fetch('/api/wifi/scan');
        const data = await res.json();
        sel.innerHTML = '<option value="">-- 选择 Wi-Fi --</option>';
        (data.networks || []).forEach(n => {
          const opt = document.createElement('option');
          opt.value = n.ssid;
          opt.innerText = n.ssid + ' (' + n.rssi + ' dBm)' + (n.secure ? ' 🔒' : '');
          sel.appendChild(opt);
        });
      } catch (e) {
        sel.innerHTML = '<option>扫描失败，请重试</option>';
      }
    }

    async function saveWifi() {
      const ssid = document.getElementById('wifi-ssid').value.trim();
      const pass = document.getElementById('wifi-pass').value.trim();
      if (!ssid) { alert('请输入 Wi-Fi 名称'); return; }
      const res = await fetch('/api/wifi/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, pass })
      });
      alert('Wi-Fi 配置已保存，ESP32 正在尝试连接路由器！');
    }

    async function resetKeymap() {
      if (confirm('确定恢复出厂默认按键映射？')) {
        await fetch('/api/keymap/reset', { method: 'POST' });
        alert('按键映射已恢复为默认配置！');
      }
    }

    setInterval(fetchStatus, 2000);
    fetchStatus();
    fetchBleInfo();
  </script>
</body>
</html>
)rawliteral";
