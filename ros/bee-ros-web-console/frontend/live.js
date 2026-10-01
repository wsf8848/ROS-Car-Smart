(() => {
  const root = document.getElementById('bee-console');
  if (!root) return;

  const map = root.querySelector('.map');
  const occupancy = root.querySelector('.occupancy-grid');
  const mapTitle = root.querySelector('.map-title');
  const headerPills = [...root.querySelectorAll('.header-right .status-pill')];
  const mode = root.querySelector('.mode');
  const taskCards = [...root.querySelectorAll('.task')];
  const controlBoxes = [...root.querySelectorAll('.control-box')];
  const telemetry = [...root.querySelectorAll('.metric')];
  const panelMeta = root.querySelector('.telemetry .panel-meta');
  const mapReadout = [...root.querySelectorAll('.map-readout .map-chip')];
  const layerButtons = [...root.querySelectorAll('.layer-toggle')];
  const mapTools = [...root.querySelectorAll('.map-tools button')];
  const powerButtons = [...root.querySelectorAll('[data-power-action]')];
  const primaryViewButtons = [...root.querySelectorAll('[data-primary-view]')];
  const voiceToggle = root.querySelector('[data-voice-toggle]');
  const authUser = root.querySelector('[data-auth-user]');
  const logoutButton = root.querySelector('[data-auth-logout]');
  const speechSupported = 'speechSynthesis' in window && 'SpeechSynthesisUtterance' in window;
  let voiceEnabled = false;
  let lastVoiceAt = 0;
  let lastVoiceKey = '';
  let lastBoardTimeSyncAt = 0;
  let boardTimeSyncBusy = false;
  try { voiceEnabled = window.localStorage.getItem('beeRosVoiceEnabled') === '1'; } catch (_error) { voiceEnabled = false; }

  const syncBoardTime = async (force = false) => {
    const now = Date.now();
    if (boardTimeSyncBusy || (!force && now - lastBoardTimeSyncAt < 10 * 60 * 1000)) return;
    boardTimeSyncBusy = true;
    try {
      const response = await fetch('/api/system/time-sync', {
        method: 'POST',
        credentials: 'same-origin',
        cache: 'no-store',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({epoch_ms: now, timezone_offset_min: new Date().getTimezoneOffset()}),
      });
      if (response.ok) lastBoardTimeSyncAt = now;
    } catch (_error) {
      // 时间同步失败不能阻塞地图、雷达或控制台加载，板卡健康页会继续显示告警。
    } finally {
      boardTimeSyncBusy = false;
    }
  };

  fetch('/api/auth/status', {credentials: 'same-origin', cache: 'no-store'})
    .then((response) => response.json().then((body) => ({response, body})))
    .then(({response, body}) => {
      if (!response.ok || !body.authenticated) {
        window.location.replace('/login.html');
        return;
      }
      if (authUser) authUser.textContent = body.username || '管理员';
      syncBoardTime(true);
    })
    .catch(() => { if (authUser) authUser.textContent = '认证检查失败'; });

  logoutButton?.addEventListener('click', async () => {
    logoutButton.disabled = true;
    try {
      await fetch('/api/auth/logout', {
        method: 'POST',
        credentials: 'same-origin',
        headers: {'Content-Type': 'application/json'},
        body: '{}',
      });
    } finally {
      window.location.replace('/login.html');
    }
  });

  const updateVoiceControl = () => {
    if (!voiceToggle) return;
    const unavailable = !speechSupported;
    voiceToggle.classList.toggle('on', voiceEnabled && !unavailable);
    voiceToggle.classList.toggle('unsupported', unavailable);
    voiceToggle.setAttribute('aria-pressed', voiceEnabled && !unavailable ? 'true' : 'false');
    voiceToggle.title = unavailable ? '当前Windows浏览器不支持语音提示' : '打开或关闭Windows语音提示';
    voiceToggle.innerHTML = `<i data-lucide="${voiceEnabled && !unavailable ? 'volume-2' : 'volume-x'}" aria-hidden="true"></i><span>${voiceEnabled && !unavailable ? '语音开启' : unavailable ? '语音不可用' : '语音关闭'}</span>`;
    if (globalThis.lucide) lucide.createIcons({ attrs: { width: 15, height: 15, strokeWidth: 1.8 } });
  };
  const announce = (text, key = text, cooldownMs = 2500) => {
    if (!voiceEnabled || !speechSupported || !text) return;
    const now = Date.now();
    if (key === lastVoiceKey && now - lastVoiceAt < cooldownMs) return;
    lastVoiceKey = key;
    lastVoiceAt = now;
    const utterance = new SpeechSynthesisUtterance(text);
    utterance.lang = 'zh-CN';
    utterance.rate = 0.96;
    utterance.pitch = 1;
    utterance.volume = 0.9;
    const voices = window.speechSynthesis.getVoices();
    const chineseVoice = voices.find((voice) => /^zh(-|_)/i.test(voice.lang));
    if (chineseVoice) utterance.voice = chineseVoice;
    window.speechSynthesis.cancel();
    window.speechSynthesis.speak(utterance);
  };
  updateVoiceControl();
  voiceToggle?.addEventListener('click', () => {
    if (!speechSupported) return;
    if (voiceEnabled) announce('语音提示已关闭', 'voice-disabled', 0);
    voiceEnabled = !voiceEnabled;
    try { window.localStorage.setItem('beeRosVoiceEnabled', voiceEnabled ? '1' : '0'); } catch (_error) { /* 浏览器禁用存储时仍保留本页状态 */ }
    updateVoiceControl();
    if (voiceEnabled) announce('语音提示已开启', 'voice-enabled', 0);
  });

  const headerRight = root.querySelector('.header-right');
  const diagnosticsButton = document.createElement('button');
  diagnosticsButton.className = 'diag-toggle';
  diagnosticsButton.type = 'button';
  diagnosticsButton.innerHTML = '<i data-lucide="activity" aria-hidden="true"></i><span>雷达诊断</span>';
  if (headerRight) headerRight.insertBefore(diagnosticsButton, mode || voiceToggle?.nextSibling || null);

  const mapManagerButton = document.createElement('button');
  mapManagerButton.className = 'diag-toggle map-manager-toggle';
  mapManagerButton.type = 'button';
  mapManagerButton.innerHTML = '<i data-lucide="map-cog" aria-hidden="true"></i><span>建图管理</span>';
  if (headerRight) headerRight.insertBefore(mapManagerButton, mode || diagnosticsButton.nextSibling || null);

  const systemHealthButton = document.createElement('button');
  systemHealthButton.className = 'diag-toggle system-health-toggle';
  systemHealthButton.type = 'button';
  systemHealthButton.innerHTML = '<i data-lucide="cpu" aria-hidden="true"></i><span>板卡健康</span>';
  if (headerRight) headerRight.insertBefore(systemHealthButton, mode || mapManagerButton.nextSibling || null);

  const commissioningButton = document.createElement('button');
  commissioningButton.className = 'diag-toggle commissioning-toggle';
  commissioningButton.type = 'button';
  commissioningButton.innerHTML = '<i data-lucide="clipboard-check" aria-hidden="true"></i><span>联调检查</span>';
  if (headerRight) headerRight.insertBefore(commissioningButton, mode || systemHealthButton.nextSibling || null);

  const systemHealthDialog = document.createElement('div');
  systemHealthDialog.className = 'diag-dialog system-health-dialog';
  systemHealthDialog.setAttribute('role', 'dialog');
  systemHealthDialog.setAttribute('aria-modal', 'true');
  systemHealthDialog.setAttribute('aria-label', 'RDK X5板卡健康');
  systemHealthDialog.innerHTML = `
    <div class="diag-card system-health-card">
      <div class="diag-head"><div class="diag-title"><i data-lucide="cpu" aria-hidden="true"></i>RDK X5 板卡健康 <small>只读实时遥测</small></div><button class="diag-close" type="button" aria-label="关闭板卡健康"><i data-lucide="x" aria-hidden="true"></i></button></div>
      <div class="diag-body">
        <div class="system-health-summary"><span class="system-health-dot"></span><strong data-health-summary>正在读取板卡状态</strong><span data-health-refreshed>--</span></div>
        <div class="diag-grid system-health-grid">
          <div class="diag-metric"><span>CPU 温度</span><strong data-health="temperature">--</strong><small data-health-detail="temperature">自动识别 CPU 温区</small></div>
          <div class="diag-metric"><span>CPU 使用率</span><strong data-health="cpu">--</strong><small>约 1 秒采样差值</small></div>
          <div class="diag-metric"><span>内存使用率</span><strong data-health="memory">--</strong><small data-health-detail="memory">--</small></div>
          <div class="diag-metric"><span>磁盘使用率</span><strong data-health="disk">--</strong><small data-health-detail="disk">根分区 /</small></div>
          <div class="diag-metric"><span>系统运行时长</span><strong data-health="uptime">--</strong><small>Linux 本次启动</small></div>
          <div class="diag-metric"><span>板卡时间</span><strong data-health="time">--</strong><small data-health-detail="time">等待RTC自检</small></div>
        </div>
        <div class="diag-note system-health-note">颜色为网页预警：CPU温度 70°C、CPU/内存 75%、磁盘 80% 起提示；不等同于芯片官方降频阈值。</div>
      </div>
    </div>`;
  root.appendChild(systemHealthDialog);
  const systemHealthFields = Object.fromEntries([...systemHealthDialog.querySelectorAll('[data-health]')].map((node) => [node.dataset.health, node]));
  const systemHealthDetails = Object.fromEntries([...systemHealthDialog.querySelectorAll('[data-health-detail]')].map((node) => [node.dataset.healthDetail, node]));
  const systemHealthSummary = systemHealthDialog.querySelector('[data-health-summary]');
  const systemHealthRefreshed = systemHealthDialog.querySelector('[data-health-refreshed]');
  const systemHealthDot = systemHealthDialog.querySelector('.system-health-dot');
  let systemHealthTimer = null;
  let systemHealthBusy = false;

  const formatCapacity = (bytes) => {
    const value = Number(bytes);
    if (!Number.isFinite(value) || value < 0) return '--';
    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    let scaled = value;
    let index = 0;
    while (scaled >= 1024 && index < units.length - 1) {
      scaled /= 1024;
      index += 1;
    }
    return `${scaled.toFixed(index >= 3 ? 1 : 0)} ${units[index]}`;
  };
  const healthLevel = (value, warning, danger) => {
    const number = Number(value);
    if (!Number.isFinite(number)) return 'unavailable';
    if (number >= danger) return 'danger';
    if (number >= warning) return 'warning';
    return 'normal';
  };
  const setHealthValue = (node, text, level = 'normal') => {
    if (!node) return;
    node.textContent = text;
    node.className = level === 'normal' ? 'metric-good' : level === 'warning' ? 'metric-warn' : level === 'danger' ? 'diag-error' : 'health-unavailable';
  };
  const setHealthUnavailable = (message = '数据暂不可用') => {
    Object.values(systemHealthFields).forEach((node) => setHealthValue(node, '--', 'unavailable'));
    systemHealthSummary.textContent = message;
    systemHealthSummary.className = 'diag-error';
    systemHealthRefreshed.textContent = '等待重新连接';
    systemHealthDot.className = 'system-health-dot danger';
  };
  const refreshSystemHealth = async () => {
    if (systemHealthBusy || document.hidden || !systemHealthDialog.classList.contains('open')) return;
    systemHealthBusy = true;
    try {
      const response = await fetch('/api/system/health', { credentials: 'same-origin', cache: 'no-store' });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const payload = await response.json();
      const cpuUsage = payload.cpu?.usage_percent;
      const temperature = payload.cpu?.temperature_c;
      const memoryUsage = payload.memory?.usage_percent;
      const diskUsage = payload.disk?.usage_percent;
      const timeHealthy = payload.time?.healthy === true;
      const levels = [
        healthLevel(temperature, 70, 80),
        healthLevel(cpuUsage, 75, 90),
        healthLevel(memoryUsage, 75, 90),
        healthLevel(diskUsage, 80, 90),
        timeHealthy ? 'normal' : 'warning',
      ];
      setHealthValue(systemHealthFields.temperature, Number.isFinite(Number(temperature)) ? `${Number(temperature).toFixed(1)} °C` : '--', levels[0]);
      setHealthValue(systemHealthFields.cpu, Number.isFinite(Number(cpuUsage)) ? `${Number(cpuUsage).toFixed(1)} %` : '--', levels[1]);
      setHealthValue(systemHealthFields.memory, Number.isFinite(Number(memoryUsage)) ? `${Number(memoryUsage).toFixed(1)} %` : '--', levels[2]);
      setHealthValue(systemHealthFields.disk, Number.isFinite(Number(diskUsage)) ? `${Number(diskUsage).toFixed(1)} %` : '--', levels[3]);
      setHealthValue(systemHealthFields.uptime, payload.uptime_seconds == null ? '--' : formatUptime(payload.uptime_seconds), payload.uptime_seconds == null ? 'unavailable' : 'normal');
      const boardTime = payload.time?.system_utc ? new Date(payload.time.system_utc) : null;
      setHealthValue(systemHealthFields.time, boardTime && !Number.isNaN(boardTime.getTime()) ? boardTime.toLocaleString('zh-CN', { hour12: false }) : '--', timeHealthy ? 'normal' : 'warning');
      const thermalType = payload.cpu?.thermal_zone?.type;
      systemHealthDetails.temperature.textContent = thermalType ? `温区 ${thermalType}` : '未发现可用 CPU 温区';
      systemHealthDetails.memory.textContent = `${formatCapacity(payload.memory?.used_bytes)} / ${formatCapacity(payload.memory?.total_bytes)}`;
      systemHealthDetails.disk.textContent = `${formatCapacity(payload.disk?.used_bytes)} / ${formatCapacity(payload.disk?.total_bytes)} · 根分区 /`;
      const rtcDelta = Number(payload.time?.rtc_delta_seconds);
      const timeSource = payload.time?.source;
      const timeSourceLabels = {
        ntp: 'NTP自动同步',
        browser_sync: 'Windows浏览器自动校时',
        rtc_offline: 'RTC离线回退',
        last_trusted_fallback: '上次可信时间回退',
        system_unverified: '系统时间未验证',
      };
      const timeSourceLabel = timeSourceLabels[timeSource] || timeSource || '未知时间源';
      systemHealthDetails.time.textContent = timeHealthy
        ? `${timeSourceLabel}${Number.isFinite(rtcDelta) ? ` · 系统/RTC差 ${Math.abs(rtcDelta).toFixed(1)}秒` : ''}`
        : '时间自检未通过，请打开网页自动校时或检查网络时间源';
      const worst = levels.includes('danger') ? 'danger' : levels.includes('warning') ? 'warning' : payload.ok ? 'normal' : 'warning';
      systemHealthSummary.textContent = worst === 'danger' ? '板卡资源进入危险预警' : worst === 'warning' ? '板卡资源需要关注' : '板卡运行正常';
      systemHealthSummary.className = worst === 'danger' ? 'diag-error' : worst === 'warning' ? 'metric-warn' : 'metric-good';
      systemHealthDot.className = `system-health-dot ${worst}`;
      const sampledAt = payload.timestamp ? new Date(payload.timestamp) : null;
      systemHealthRefreshed.textContent = sampledAt && !Number.isNaN(sampledAt.getTime()) ? `更新 ${sampledAt.toLocaleTimeString('zh-CN', { hour12: false })}` : '刚刚更新';
    } catch (_error) {
      setHealthUnavailable('板卡健康接口不可用');
    } finally {
      systemHealthBusy = false;
    }
  };
  const stopSystemHealthRefresh = () => {
    clearInterval(systemHealthTimer);
    systemHealthTimer = null;
  };
  const startSystemHealthRefresh = () => {
    stopSystemHealthRefresh();
    if (document.hidden || !systemHealthDialog.classList.contains('open')) return;
    refreshSystemHealth();
    systemHealthTimer = setInterval(refreshSystemHealth, 2000);
  };
  const closeSystemHealth = () => {
    systemHealthDialog.classList.remove('open');
    systemHealthButton.classList.remove('active');
    stopSystemHealthRefresh();
  };
  systemHealthButton.addEventListener('click', () => {
    systemHealthDialog.classList.add('open');
    systemHealthButton.classList.add('active');
    startSystemHealthRefresh();
    announce('已打开板卡健康状态', 'system-health-open', 0);
  });
  systemHealthDialog.querySelector('.diag-close').addEventListener('click', closeSystemHealth);
  systemHealthDialog.addEventListener('click', (event) => { if (event.target === systemHealthDialog) closeSystemHealth(); });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) stopSystemHealthRefresh();
    else {
      syncBoardTime();
      if (systemHealthDialog.classList.contains('open')) startSystemHealthRefresh();
    }
  });
  setInterval(() => { if (!document.hidden) syncBoardTime(); }, 10 * 60 * 1000);

  const commissioningDialog = document.createElement('div');
  commissioningDialog.className = 'diag-dialog commissioning-dialog';
  commissioningDialog.setAttribute('role', 'dialog');
  commissioningDialog.setAttribute('aria-modal', 'true');
  commissioningDialog.setAttribute('aria-label', 'STM32到货联调检查');
  commissioningDialog.innerHTML = `
    <div class="diag-card">
      <div class="diag-head"><div class="diag-title"><i data-lucide="clipboard-check" aria-hidden="true"></i>STM32 到货联调检查 <small>只读预检报告</small></div><button class="diag-close" type="button" aria-label="关闭联调检查"><i data-lucide="x" aria-hidden="true"></i></button></div>
      <div class="diag-body">
        <div class="diag-grid">
          <div class="diag-metric"><span>协议自检</span><strong data-commissioning="protocol">--</strong><small>CRC / 帧往返 / 错帧拒绝</small></div>
          <div class="diag-metric"><span>网站基线</span><strong data-commissioning="baseline">--</strong><small data-commissioning-detail="baseline">总自动验收</small></div>
          <div class="diag-metric"><span>串口设备</span><strong data-commissioning="serial">--</strong><small data-commissioning-detail="serial">等待报告</small></div>
          <div class="diag-metric"><span>STM32遥测</span><strong data-commissioning="telemetry">--</strong><small data-commissioning-detail="telemetry">73字节正式载荷</small></div>
          <div class="diag-metric"><span>安全空闲</span><strong data-commissioning="idle">--</strong><small>底盘/仿真/执行器未运行</small></div>
          <div class="diag-metric"><span>ROS导航依赖</span><strong data-commissioning="ros">--</strong><small>Nav2 / SLAM / 融合</small></div>
          <div class="diag-metric"><span>运动测试</span><strong data-commissioning="motion">禁止</strong><small>必须完成车轮架空与实体验收</small></div>
        </div>
        <div class="diag-note" data-commissioning-blockers>正在读取到货预检报告…</div>
        <div class="diag-actions"><button type="button" data-commissioning-refresh>刷新报告</button></div>
      </div>
    </div>`;
  root.appendChild(commissioningDialog);
  const commissioningFields = Object.fromEntries([...commissioningDialog.querySelectorAll('[data-commissioning]')].map((node) => [node.dataset.commissioning, node]));
  const commissioningDetails = Object.fromEntries([...commissioningDialog.querySelectorAll('[data-commissioning-detail]')].map((node) => [node.dataset.commissioningDetail, node]));
  const commissioningBlockers = commissioningDialog.querySelector('[data-commissioning-blockers]');
  const blockerLabels = {
    configured_serial_device_missing: '配置的STM32串口设备不存在',
    serial_device_not_readable: '当前用户没有串口读取权限',
    motion_or_simulation_service_active: '底盘或仿真服务仍在运行，不能独占探测串口',
    stm32_telemetry_probe_not_run: '尚未执行STM32遥测只读探测',
    no_valid_stm32_telemetry: '未收到合法STM32正式遥测帧',
    physical_chassis_verification_not_completed: '尚未完成实体底盘与安全验证',
  };
  const refreshCommissioning = async () => {
    try {
      const response = await fetch('/api/commissioning', {credentials: 'same-origin', cache: 'no-store'});
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const payload = await response.json();
      const report = payload.report;
      const deliveryAudit = payload.delivery_audit || {};
      const summary = report.summary || {};
      const baselineReady = deliveryAudit.baseline_pass === true
        && Number(deliveryAudit.failure_count || 0) === 0
        && Number(deliveryAudit.warning_count || 0) === 0;
      const protocolReady = summary.protocol_ready === true;
      const serialReady = summary.device_node_present === true;
      const telemetryReady = summary.telemetry_validated === true;
      const idleReady = summary.safe_idle === true;
      const packages = report.ros_packages || {};
      const rosReady = ['nav2_bringup', 'slam_toolbox', 'robot_localization'].every((name) => packages[name]?.installed === true);
      setHealthValue(commissioningFields.protocol, protocolReady ? '通过' : '失败', protocolReady ? 'normal' : 'danger');
      setHealthValue(commissioningFields.baseline, baselineReady ? '通过' : '待检查', baselineReady ? 'normal' : 'warning');
      setHealthValue(commissioningFields.serial, serialReady ? '节点存在' : '未发现', serialReady ? 'warning' : 'danger');
      setHealthValue(commissioningFields.telemetry, telemetryReady ? '已验证' : '未验证', telemetryReady ? 'normal' : 'warning');
      setHealthValue(commissioningFields.idle, idleReady ? '安全空闲' : '服务占用', idleReady ? 'normal' : 'danger');
      setHealthValue(commissioningFields.ros, rosReady ? '依赖完整' : '依赖缺失', rosReady ? 'normal' : 'danger');
      setHealthValue(commissioningFields.motion, summary.can_begin_motion_test ? '允许进入' : '禁止', summary.can_begin_motion_test ? 'normal' : 'warning');
      commissioningDetails.baseline.textContent = deliveryAudit.generated_at
        ? `${deliveryAudit.release || '未标版本'} · 失败 ${deliveryAudit.failure_count || 0} · 警告 ${deliveryAudit.warning_count || 0}`
        : '请先运行总自动验收';
      commissioningDetails.serial.textContent = `${report.configured_link?.device || '--'} @ ${report.configured_link?.baud || '--'} · ${report.serial?.stable_links?.length ? '稳定别名可用' : '尚未绑定稳定别名'}`;
      commissioningDetails.telemetry.textContent = telemetryReady
        ? `${report.probe?.frequency_hz || '--'} Hz · CRC ${report.probe?.crc_errors || 0} · 跳号 ${report.probe?.sequence_gaps || 0}`
        : '必须显式运行只读probe，设备节点存在不等于STM32在线';
      const blockers = Array.isArray(summary.blockers) ? summary.blockers : [];
      const combinedBlockers = baselineReady ? blockers : ['网站总自动验收尚未达到0失败0警告', ...blockers];
      commissioningBlockers.textContent = combinedBlockers.length
        ? `当前阻塞项：\n${combinedBlockers.map((item, index) => `${index + 1}. ${blockerLabels[item] || item}`).join('\n')}`
        : '所有只读检查已通过；仍需按车轮架空→低速落地顺序完成实车验收。';
    } catch (_error) {
      Object.values(commissioningFields).forEach((node) => setHealthValue(node, '--', 'unavailable'));
      commissioningBlockers.textContent = '联调报告不可用，请先运行STM32到货预检和总自动验收。';
    }
  };
  const closeCommissioning = () => {
    commissioningDialog.classList.remove('open');
    commissioningButton.classList.remove('active');
  };
  commissioningButton.addEventListener('click', () => {
    commissioningDialog.classList.add('open');
    commissioningButton.classList.add('active');
    refreshCommissioning();
    announce('已打开STM32到货联调检查', 'commissioning-open', 0);
  });
  commissioningDialog.querySelector('.diag-close').addEventListener('click', closeCommissioning);
  commissioningDialog.querySelector('[data-commissioning-refresh]').addEventListener('click', refreshCommissioning);
  commissioningDialog.addEventListener('click', (event) => { if (event.target === commissioningDialog) closeCommissioning(); });

  const diagnosticsDialog = document.createElement('div');
  diagnosticsDialog.className = 'diag-dialog';
  diagnosticsDialog.setAttribute('role', 'dialog');
  diagnosticsDialog.setAttribute('aria-modal', 'true');
  diagnosticsDialog.setAttribute('aria-label', 'LD14诊断与录包');
  diagnosticsDialog.innerHTML = `
    <div class="diag-card">
      <div class="diag-head"><div class="diag-title"><i data-lucide="radar" aria-hidden="true"></i>LD14诊断与录包 <small>真实 /scan</small></div><button class="diag-close" type="button" aria-label="关闭雷达诊断"><i data-lucide="x" aria-hidden="true"></i></button></div>
      <div class="diag-body">
        <div class="diag-grid">
          <div class="diag-metric"><span>雷达状态</span><strong data-diag="online">--</strong><small>数据超时阈值1.5秒</small></div>
          <div class="diag-metric"><span>扫描频率</span><strong data-diag="frequency">--</strong><small>ROS2 /scan</small></div>
          <div class="diag-metric"><span>累计帧数</span><strong data-diag="frames">--</strong><small>网关本次运行</small></div>
          <div class="diag-metric"><span>有效点比例</span><strong data-diag="ratio">--</strong><small>有限距离点 / 总点</small></div>
          <div class="diag-metric"><span>15–30cm近距点</span><strong data-diag="near">--</strong><small>当前扫描帧</small></div>
          <div class="diag-metric"><span>最近障碍</span><strong data-diag="minimum">--</strong><small>标称下限0.15m</small></div>
          <div class="diag-metric"><span>当前延迟</span><strong data-diag="latency">--</strong><small>网关接收年龄</small></div>
          <div class="diag-metric"><span>运行时间</span><strong data-diag="uptime">--</strong><small>网关本次运行</small></div>
        </div>
        <div class="diag-section">
          <div class="diag-section-head"><span>真实雷达录包</span><span class="diag-status" data-bag-status>空闲</span></div>
          <div class="diag-actions"><button class="primary" type="button" data-bag-action="record_start">开始录制 /scan</button><button class="warn" type="button" data-bag-action="record_stop">停止录制</button><button type="button" data-bag-action="play_stop">停止回放</button><button type="button" data-diag-refresh>刷新</button></div>
          <div class="diag-note">回放强制发布到测试话题 <b>/bee/replay/scan</b>，不会覆盖真实 /scan，也不会驱动底盘。</div>
        </div>
        <div class="diag-section"><div class="diag-section-head"><span>录包列表</span><span class="diag-status" data-bag-count>0个</span></div><div class="diag-bags"></div></div>
      </div>
    </div>`;
  root.appendChild(diagnosticsDialog);
  const diagnosticsFields = Object.fromEntries([...diagnosticsDialog.querySelectorAll('[data-diag]')].map((node) => [node.dataset.diag, node]));
  const bagStatus = diagnosticsDialog.querySelector('[data-bag-status]');
  const bagCount = diagnosticsDialog.querySelector('[data-bag-count]');
  const bagsList = diagnosticsDialog.querySelector('.diag-bags');
  let diagnosticsTimer = null;
  let diagnosticsBusy = false;
  const formatUptime = (seconds) => {
    const value = Math.max(0, Math.round(Number(seconds) || 0));
    const hours = Math.floor(value / 3600);
    const minutes = Math.floor((value % 3600) / 60);
    const secs = value % 60;
    return `${hours ? `${hours}h ` : ''}${minutes}m ${secs}s`;
  };
  const formatBytes = (bytes) => {
    const value = Number(bytes) || 0;
    if (value >= 1048576) return `${(value / 1048576).toFixed(1)} MB`;
    if (value >= 1024) return `${(value / 1024).toFixed(1)} KB`;
    return `${value} B`;
  };
  const renderBagState = (status = {}) => {
    bagStatus.textContent = status.recording ? `录制中 · ${status.record_id}` : status.playing ? `回放中 · ${status.play_id}` : '空闲';
    bagStatus.classList.toggle('diag-error', false);
    diagnosticsDialog.querySelector('[data-bag-action="record_start"]').disabled = Boolean(status.recording);
    diagnosticsDialog.querySelector('[data-bag-action="record_stop"]').disabled = !status.recording;
    diagnosticsDialog.querySelector('[data-bag-action="play_stop"]').disabled = !status.playing;
  };
  const refreshDiagnostics = async () => {
    if (diagnosticsBusy) return;
    diagnosticsBusy = true;
    try {
      const [diagResponse, bagsResponse] = await Promise.all([
        fetch('/api/diagnostics', { cache: 'no-store' }),
        fetch('/api/bags', { cache: 'no-store' }),
      ]);
      if (!diagResponse.ok || !bagsResponse.ok) throw new Error('diagnostics_unavailable');
      const diagnostics = await diagResponse.json();
      const bags = await bagsResponse.json();
      diagnosticsFields.online.textContent = diagnostics.online ? '在线' : '超时';
      diagnosticsFields.online.className = diagnostics.online ? 'metric-good' : 'diag-error';
      diagnosticsFields.frequency.textContent = `${Number(diagnostics.frequency_hz || 0).toFixed(2)} Hz`;
      diagnosticsFields.frames.textContent = Number(diagnostics.frames_total || 0).toLocaleString();
      diagnosticsFields.ratio.textContent = `${(Number(diagnostics.valid_ratio || 0) * 100).toFixed(1)}%`;
      diagnosticsFields.near.textContent = `${Number(diagnostics.near_points_15_30cm || 0)} 点`;
      diagnosticsFields.minimum.textContent = Number.isFinite(diagnostics.last_min_range) ? `${Number(diagnostics.last_min_range).toFixed(3)} m` : '--';
      diagnosticsFields.latency.textContent = diagnostics.last_age_ms == null ? '--' : `${diagnostics.last_age_ms} ms`;
      diagnosticsFields.uptime.textContent = formatUptime(diagnostics.uptime_seconds);
      renderBagState(bags.status || diagnostics.bags);
      const items = Array.isArray(bags.items) ? bags.items : [];
      bagCount.textContent = `${items.length}个`;
      bagsList.replaceChildren();
      if (!items.length) {
        const empty = document.createElement('div');
        empty.className = 'diag-empty';
        empty.textContent = '暂无录包。点击“开始录制 /scan”创建真实雷达数据包。';
        bagsList.appendChild(empty);
      } else {
        items.forEach((item) => {
          const row = document.createElement('div');
          row.className = 'diag-bag';
          row.innerHTML = `<div class="diag-bag-copy"><div class="diag-bag-name"></div><div class="diag-bag-meta"></div></div><div class="diag-bag-actions"><button type="button" data-play-bag>隔离回放</button></div>`;
          row.querySelector('.diag-bag-name').textContent = item.id;
          row.querySelector('.diag-bag-meta').textContent = `${formatBytes(item.bytes)} · /scan`;
          const playButton = row.querySelector('[data-play-bag]');
          playButton.disabled = Boolean((bags.status || {}).playing);
          playButton.addEventListener('click', () => runBagAction('play_start', item.id));
          bagsList.appendChild(row);
        });
      }
    } catch (_error) {
      bagStatus.textContent = '诊断接口不可用';
      bagStatus.classList.add('diag-error');
    } finally {
      diagnosticsBusy = false;
    }
  };
  const runBagAction = async (action, bagId = null) => {
    if (diagnosticsBusy) return;
    diagnosticsBusy = true;
    try {
      const body = bagId ? { action, bag_id: bagId } : { action };
      const response = await fetch('/api/bags/action', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const payload = await response.json();
      if (!response.ok) throw new Error(payload.error || 'bag_action_failed');
      const spoken = { record_start: 'LD14真实数据录制已开始', record_stop: 'LD14录制已停止并保存', play_start: 'LD14录包隔离回放已开始', play_stop: 'LD14录包回放已停止' };
      announce(spoken[action], `bag-${action}`, 0);
    } catch (error) {
      bagStatus.textContent = `操作失败 · ${error.message}`;
      bagStatus.classList.add('diag-error');
    } finally {
      diagnosticsBusy = false;
      await refreshDiagnostics();
    }
  };
  diagnosticsDialog.querySelectorAll('[data-bag-action]').forEach((button) => button.addEventListener('click', () => runBagAction(button.dataset.bagAction)));
  diagnosticsDialog.querySelector('[data-diag-refresh]').addEventListener('click', refreshDiagnostics);
  const closeDiagnostics = () => {
    diagnosticsDialog.classList.remove('open');
    diagnosticsButton.classList.remove('active');
    clearInterval(diagnosticsTimer);
    diagnosticsTimer = null;
  };
  diagnosticsButton.addEventListener('click', () => {
    diagnosticsDialog.classList.add('open');
    diagnosticsButton.classList.add('active');
    refreshDiagnostics();
    clearInterval(diagnosticsTimer);
    diagnosticsTimer = setInterval(refreshDiagnostics, 1500);
    announce('已打开LD14雷达诊断', 'diagnostics-open', 0);
  });
  diagnosticsDialog.querySelector('.diag-close').addEventListener('click', closeDiagnostics);
  diagnosticsDialog.addEventListener('click', (event) => { if (event.target === diagnosticsDialog) closeDiagnostics(); });

  const mapManagerDialog = document.createElement('div');
  mapManagerDialog.className = 'diag-dialog map-manager-dialog';
  mapManagerDialog.setAttribute('role', 'dialog');
  mapManagerDialog.setAttribute('aria-modal', 'true');
  mapManagerDialog.setAttribute('aria-label', '实验建图与地图管理');
  mapManagerDialog.innerHTML = `
    <div class="diag-card map-manager-card">
      <div class="diag-head"><div class="diag-title"><i data-lucide="map-cog" aria-hidden="true"></i>实验建图与地图管理 <small>真实LD14 · 无底盘验证</small></div><button class="diag-close" type="button" aria-label="关闭建图管理"><i data-lucide="x" aria-hidden="true"></i></button></div>
      <div class="diag-body">
        <div class="map-manager-warning">当前使用实验单位TF，仅验证真实 /scan → SLAM → /map 链路，不代表整车里程计、IMU、定位或导航已经完成。</div>
        <div class="diag-section">
          <div class="diag-section-head"><span>实验建图服务</span><span class="diag-status" data-slam-status>读取中</span></div>
          <div class="map-save-fields"><label>任务地图名字<input type="text" data-map-name maxlength="80" placeholder="例如：仓库一层主通道"></label><label>场地<input type="text" data-map-site maxlength="80" placeholder="例如：客户A厂房 / 一层"></label></div>
          <div class="diag-actions"><button class="primary" type="button" data-slam-action="start">开始实验建图</button><button class="warn" type="button" data-slam-action="stop">停止实验建图</button><button type="button" data-map-action="save">保存当前地图</button><button type="button" data-map-refresh>刷新</button></div>
        </div>
        <div class="diag-section"><div class="diag-section-head"><span>已保存地图</span><span class="diag-status" data-map-count>0张</span></div><div class="diag-bags map-list"></div></div>
      </div>
    </div>`;
  root.appendChild(mapManagerDialog);
  const slamStatus = mapManagerDialog.querySelector('[data-slam-status]');
  const mapCount = mapManagerDialog.querySelector('[data-map-count]');
  const mapList = mapManagerDialog.querySelector('.map-list');
  const mapNameInput = mapManagerDialog.querySelector('[data-map-name]');
  const mapSiteInput = mapManagerDialog.querySelector('[data-map-site]');
  let mapManagerTimer = null;
  let mapManagerBusy = false;
  const renderMapState = (payload = {}) => {
    const status = payload.status || {};
    const slam = payload.slam || {};
    slamStatus.textContent = slam.active ? '实验建图运行中' : status.loaded ? `已加载 · ${status.load_id}` : '已停止';
    slamStatus.classList.toggle('diag-error', false);
    mapManagerDialog.querySelector('[data-slam-action="start"]').disabled = Boolean(slam.active || status.loaded);
    mapManagerDialog.querySelector('[data-slam-action="stop"]').disabled = !slam.active;
    mapManagerDialog.querySelector('[data-map-action="save"]').disabled = !slam.active || Boolean(status.saving);
    const items = Array.isArray(payload.items) ? payload.items : [];
    mapCount.textContent = `${items.length}张`;
    mapList.replaceChildren();
    if (!items.length) {
      const empty = document.createElement('div');
      empty.className = 'diag-empty';
      empty.textContent = '暂无保存地图。启动实验建图并观察到 /map 后再保存。';
      mapList.appendChild(empty);
      return;
    }
    items.forEach((item) => {
      const row = document.createElement('div');
      row.className = 'diag-bag';
      row.innerHTML = '<div class="diag-bag-copy"><div class="diag-bag-name"></div><div class="diag-bag-meta"></div></div><div class="diag-bag-actions"><button type="button" data-load-map>加载</button><button type="button" data-unload-map>卸载</button></div>';
      row.querySelector('.diag-bag-name').textContent = item.name;
      const createdAt = item.created_at ? new Date(item.created_at).toLocaleString('zh-CN', { hour12: false }) : '时间未知';
      row.querySelector('.diag-bag-meta').textContent = `${item.site} · ${createdAt} · ${formatBytes(item.yaml_bytes + item.image_bytes)} · ${item.complete ? '完整PGM/YAML' : '文件不完整'}`;
      row.querySelector('[data-load-map]').disabled = Boolean(slam.active || status.loaded || !item.complete);
      row.querySelector('[data-unload-map]').disabled = status.load_id !== item.id;
      row.querySelector('[data-load-map]').addEventListener('click', () => runMapAction('load', item.id));
      row.querySelector('[data-unload-map]').addEventListener('click', () => runMapAction('unload'));
      mapList.appendChild(row);
    });
  };
  const refreshMapManager = async () => {
    if (mapManagerBusy) return;
    mapManagerBusy = true;
    try {
      const response = await fetch('/api/maps', { cache: 'no-store' });
      const payload = await response.json();
      if (!response.ok) throw new Error(payload.error || 'maps_unavailable');
      renderMapState(payload);
    } catch (error) {
      slamStatus.textContent = `接口不可用 · ${error.message}`;
      slamStatus.classList.add('diag-error');
    } finally { mapManagerBusy = false; }
  };
  const runMapAction = async (action, mapId = null) => {
    if (mapManagerBusy) return;
    mapManagerBusy = true;
    try {
      const isSlam = action === 'start' || action === 'stop';
      const body = mapId ? { action, map_id: mapId } : action === 'save' ? { action, name: mapNameInput.value.trim(), site: mapSiteInput.value.trim() } : { action };
      const response = await fetch(isSlam ? '/api/slam/action' : '/api/maps/action', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const payload = await response.json();
      if (!response.ok) throw new Error(payload.error || 'map_action_failed');
      const spoken = { start: '实验建图已启动', stop: '实验建图已停止', save: '地图保存已开始', load: '保存地图已加载', unload: '保存地图已卸载' };
      announce(spoken[action], `map-${action}`, 0);
      if (action === 'save') { mapNameInput.value = ''; mapSiteInput.value = ''; }
    } catch (error) {
      slamStatus.textContent = `操作失败 · ${error.message}`;
      slamStatus.classList.add('diag-error');
    } finally {
      mapManagerBusy = false;
      window.setTimeout(refreshMapManager, action === 'save' ? 1800 : 200);
      window.setTimeout(() => refreshTaskContext(), action === 'save' ? 1900 : 250);
    }
  };
  mapManagerDialog.querySelectorAll('[data-slam-action]').forEach((button) => button.addEventListener('click', () => runMapAction(button.dataset.slamAction)));
  mapManagerDialog.querySelector('[data-map-action="save"]').addEventListener('click', () => runMapAction('save'));
  mapManagerDialog.querySelector('[data-map-refresh]').addEventListener('click', refreshMapManager);
  const closeMapManager = () => { mapManagerDialog.classList.remove('open'); mapManagerButton.classList.remove('active'); clearInterval(mapManagerTimer); mapManagerTimer = null; };
  mapManagerButton.addEventListener('click', () => { mapManagerDialog.classList.add('open'); mapManagerButton.classList.add('active'); refreshMapManager(); clearInterval(mapManagerTimer); mapManagerTimer = setInterval(refreshMapManager, 1800); announce('已打开实验建图与地图管理', 'map-manager-open', 0); });
  mapManagerDialog.querySelector('.diag-close').addEventListener('click', closeMapManager);
  mapManagerDialog.addEventListener('click', (event) => { if (event.target === mapManagerDialog) closeMapManager(); });
  if (globalThis.lucide) lucide.createIcons({ attrs: { width: 15, height: 15, strokeWidth: 1.8 } });

  if (mapTitle) mapTitle.innerHTML = '实时地图 <span>/map栅格 + LD14独立扫描层</span>';
  if (mode) mode.innerHTML = '<strong>地图联调</strong>';
  if (root.querySelector('.danger')) {
    root.querySelector('.danger').disabled = true;
    root.querySelector('.danger').title = 'STM32底盘未接入，急停控制尚不可用';
  }

  const setPill = (index, text, state) => {
    const pill = headerPills[index];
    if (!pill) return;
    pill.className = `status-pill ${state}`;
    pill.innerHTML = `<span class="dot"></span>${text}`;
  };
  setPill(0, 'RDK X5 在线', 'online');
  setPill(1, 'STM32 未接入', 'offline');
  setPill(2, 'LD14 连接中', 'warning');
  setPill(3, 'IMU 未接入', 'offline');

  if (mapReadout[0]) mapReadout[0].innerHTML = '<span class="dot amber"></span><b>等待 /map</b>';
  if (mapReadout[1]) mapReadout[1].innerHTML = '坐标 <b>--</b>';
  if (mapReadout[2]) mapReadout[2].innerHTML = '航向 <b>--</b>';
  if (mapReadout[3]) mapReadout[3].innerHTML = '地图 <b>未建图</b>';
  mapTools.forEach((button) => {
    button.disabled = true;
    button.title = '地图、定位与Nav2尚未接入';
  });
  layerButtons.forEach((button) => {
    const isLive = true;
    button.classList.toggle('active', isLive);
    button.disabled = false;
    button.title = button.dataset.layer === 'scan'
      ? '真实LD14扫描；当前没有map→laser TF，不代表已配准到地图'
      : button.dataset.layer === 'grid'
        ? '真实ROS2 /map OccupancyGrid'
        : '仿真测试图层；不代表实体车辆数据';
  });
  const layerCount = root.querySelector('.layer-head span:last-child');
  if (layerCount) layerCount.textContent = '10 / 10 测试链路';

  taskCards.forEach((card) => {
    card.classList.remove('active');
    card.classList.add('disabled');
    const sub = card.querySelector('.task-sub');
    const state = card.querySelector('.task-state');
    if (sub) sub.textContent = '等待底盘、里程计与地图';
    if (state) state.textContent = '未接入';
    card.querySelectorAll('button').forEach((button) => { button.disabled = !button.hasAttribute('data-task-open-patrol'); });
  });
  const mission = root.querySelector('.side-panel .mission');
  if (mission) {
    mission.classList.add('stage-disabled');
    const progress = mission.querySelector('.progress i');
    if (progress) progress.style.width = '0%';
  }
  const controlSourceRows = root.querySelectorAll('.side-panel:nth-child(2) .mission-row');
  const fault = root.querySelector('.side-panel:nth-child(2) .fault');
  if (fault) fault.innerHTML = '<span class="dot amber"></span><span>当前为 <b>只读地图联调</b></span>';
  if (controlSourceRows[0]) controlSourceRows[0].innerHTML = '<span>STM32底盘</span><span>未接入</span>';
  if (controlSourceRows[1]) controlSourceRows[1].innerHTML = '<span>运动指令</span><span>已禁用</span>';
  controlBoxes.forEach((box) => box.classList.add('disabled'));
  root.querySelectorAll('.action-row button,.takeover,.joy').forEach((button) => {
    button.disabled = true;
  });
  root.querySelectorAll('.map-context button').forEach((button) => {
    button.disabled = true;
  });

  const powerDialog = document.createElement('div');
  powerDialog.className = 'power-dialog';
  powerDialog.setAttribute('role', 'dialog');
  powerDialog.setAttribute('aria-modal', 'true');
  powerDialog.innerHTML = '<div class="power-dialog-card"><div class="power-dialog-title"></div><div class="power-dialog-copy"></div><div class="power-dialog-warning">操作后网页会断开。重启通常需要等待约1分钟再刷新；关机后需要重新给板卡上电才能恢复。</div><div class="power-dialog-actions"><button type="button" data-power-cancel>取消</button><button class="confirm" type="button" data-power-confirm>确认执行</button></div></div>';
  root.appendChild(powerDialog);
  const powerToast = document.createElement('div');
  powerToast.className = 'power-toast';
  powerToast.setAttribute('role', 'status');
  root.appendChild(powerToast);
  let pendingPowerAction = null;
  let toastTimer;
  const showPowerToast = (message, isError = false) => {
    clearTimeout(toastTimer);
    powerToast.textContent = message;
    powerToast.className = `power-toast show${isError ? ' error' : ''}`;
    toastTimer = setTimeout(() => { powerToast.className = 'power-toast'; }, 6000);
  };
  const closePowerDialog = () => {
    pendingPowerAction = null;
    powerDialog.classList.remove('open');
  };
  powerButtons.forEach((button) => button.addEventListener('click', () => {
    pendingPowerAction = button.dataset.powerAction;
    const isReboot = pendingPowerAction === 'reboot';
    powerDialog.querySelector('.power-dialog-title').textContent = isReboot ? '确认重启RDK X5？' : '确认关闭RDK X5？';
    powerDialog.querySelector('.power-dialog-copy').textContent = isReboot
      ? '系统将安全重启，ROS2、雷达和网页服务会暂时离线。'
      : '系统将安全关机，ROS2、雷达和网页服务会全部停止。';
    powerDialog.querySelector('[data-power-confirm]').textContent = isReboot ? '确认重启' : '确认关机';
    powerDialog.classList.add('open');
    powerDialog.querySelector('[data-power-cancel]').focus();
    announce(isReboot ? '请确认是否重启板卡' : '请确认是否关闭板卡', `power-confirm-${pendingPowerAction}`, 0);
  }));
  powerDialog.querySelector('[data-power-cancel]').addEventListener('click', closePowerDialog);
  powerDialog.addEventListener('click', (event) => { if (event.target === powerDialog) closePowerDialog(); });
  document.addEventListener('keydown', (event) => { if (event.key === 'Escape') closePowerDialog(); });
  powerDialog.querySelector('[data-power-confirm]').addEventListener('click', async () => {
    if (!pendingPowerAction) return;
    const action = pendingPowerAction;
    const label = action === 'reboot' ? '重启' : '关机';
    const confirmButton = powerDialog.querySelector('[data-power-confirm]');
    confirmButton.disabled = true;
    confirmButton.textContent = '正在提交…';
    powerButtons.forEach((button) => { button.disabled = true; });
    try {
      const response = await fetch('/api/system/power', {
        method: 'POST',
        credentials: 'same-origin',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action }),
      });
      const result = await response.json().catch(() => ({}));
      if (!response.ok || !result.ok) throw new Error(result.error || `HTTP ${response.status}`);
      closePowerDialog();
      showPowerToast(result.dry_run ? `${label}请求验证通过（测试模式，板卡不会执行）` : `${label}请求已接受，板卡将在约2秒后离线`);
      announce(result.dry_run ? `${label}请求验证通过，当前为测试模式` : `${label}请求已接受，板卡即将离线`, `power-${action}-accepted`, 0);
      if (!result.dry_run) {
        setPill(0, `RDK X5 即将${label}`, 'warning');
      } else {
        powerButtons.forEach((button) => { button.disabled = false; });
      }
    } catch (error) {
      showPowerToast(`${label}请求失败：${error.message}`, true);
      announce(`${label}请求失败`, `power-${action}-failed`, 0);
      powerButtons.forEach((button) => { button.disabled = false; });
    } finally {
      confirmButton.disabled = false;
      confirmButton.textContent = '确认执行';
    }
  });

  if (occupancy) {
    occupancy.innerHTML = '';
  }

  const ns = 'http://www.w3.org/2000/svg';
  const radar = document.createElementNS(ns, 'svg');
  radar.setAttribute('class', 'live-radar-grid');
  radar.setAttribute('viewBox', '0 0 1000 550');
  radar.setAttribute('preserveAspectRatio', 'none');
  radar.setAttribute('aria-label', 'LD14实时激光雷达扫描');
  radar.dataset.layerTarget = 'scan';
  radar.innerHTML = `
    <g class="radar-reference">
      <circle class="live-radar-ring" cx="500" cy="275" r="65"/>
      <circle class="live-radar-ring" cx="500" cy="275" r="130"/>
      <circle class="live-radar-ring" cx="500" cy="275" r="195"/>
      <circle class="live-radar-ring" cx="500" cy="275" r="245"/>
      <line class="live-radar-axis" x1="255" y1="275" x2="745" y2="275"/>
      <line class="live-radar-axis" x1="500" y1="30" x2="500" y2="520"/>
    </g>
    <g class="live-radar-points"></g>
    <circle class="live-radar-hover" cx="500" cy="275" r="11"/>
    <circle class="live-radar-origin" cx="500" cy="275" r="8"/>
    <path class="live-radar-heading" d="M500 252 l-7 14 h14 z"/>
  `;
  map.appendChild(radar);
  const pointsGroup = radar.querySelector('.live-radar-points');
  const radarCaption = document.createElement('div');
  radarCaption.className = 'live-radar-caption';
  radarCaption.textContent = 'LD14独立扫描 · 未配准';
  map.appendChild(radarCaption);
  const hoverMarker = radar.querySelector('.live-radar-hover');
  const radarTooltip = document.createElement('div');
  radarTooltip.className = 'live-radar-tooltip';
  radarTooltip.setAttribute('role', 'tooltip');
  map.appendChild(radarTooltip);
  const radarMinimizeButton = document.createElement('button');
  radarMinimizeButton.className = 'radar-minimize';
  radarMinimizeButton.type = 'button';
  radarMinimizeButton.innerHTML = '<span aria-hidden="true">↙</span> 最小化雷达';
  radarMinimizeButton.setAttribute('aria-label', '最小化LD14扫描并返回栅格地图');
  map.appendChild(radarMinimizeButton);
  const cameraView = document.createElement('div');
  cameraView.className = 'camera-view';
  cameraView.setAttribute('aria-label', 'WHEELTEC C70实时RGB画面');
  cameraView.innerHTML = '<img class="camera-frame" alt="WHEELTEC C70实时RGB摄像头画面"><button class="camera-minimize" type="button" aria-label="最小化摄像头并返回栅格地图"><i data-lucide="minimize-2" aria-hidden="true"></i><span>最小化画面</span></button><div class="camera-status" role="status"><span class="dot"></span><span>WHEELTEC C70 · 待机</span></div>';
  map.appendChild(cameraView);
  if (globalThis.lucide) lucide.createIcons({attrs: {width: 15, height: 15, strokeWidth: 1.8}});
  const cameraImg = cameraView.querySelector('.camera-frame');
  const cameraMinimizeButton = cameraView.querySelector('.camera-minimize');
  const cameraStatus = cameraView.querySelector('.camera-status');
  const cameraPlaceholder = 'data:image/gif;base64,R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=';
  let radarHitPoints = [];
  let radarExpanded = false;
  let cameraExpanded = false;
  let cameraVisible = false;
  let cameraDragging = false;
  let cameraDragMoved = false;
  let cameraDragPointerId = null;
  let cameraDragOffsetX = 0;
  let cameraDragOffsetY = 0;
  let primaryView = 'map';
  const radarPointer = { inside: false, clientX: 0, clientY: 0 };

  const gridLayerButton = layerButtons.find((button) => button.dataset.layer === 'grid');
  const scanLayerButton = layerButtons.find((button) => button.dataset.layer === 'scan');

  const updateCameraStatus = async () => {
    try {
      const response = await fetch('/api/camera/status', {credentials: 'same-origin', cache: 'no-store'});
      const status = await response.json();
      if (!response.ok || !status.present) throw new Error(status.error || 'camera_not_present');
      cameraStatus.classList.toggle('online', Boolean(status.online));
      cameraStatus.classList.remove('error');
      const fps = status.measured_fps > 0 ? ` · ${status.measured_fps.toFixed(1)} fps` : '';
      cameraStatus.querySelector('span:last-child').textContent = status.online
        ? `${status.model} · ${status.output.width}×${status.output.height}${fps}`
        : `${status.model} · ${cameraVisible ? '实时小窗' : '待机'}`;
    } catch (_error) {
      cameraStatus.classList.remove('online');
      cameraStatus.classList.add('error');
      cameraStatus.querySelector('span:last-child').textContent = 'RGB摄像头不可用';
    }
  };

  const setCameraStreaming = (enabled) => {
    if (enabled && cameraImg.dataset.streaming !== '1') {
      cameraStatus.classList.remove('online', 'error');
      cameraStatus.querySelector('span:last-child').textContent = 'WHEELTEC C70 · 正在启动视频…';
      cameraImg.dataset.streaming = '1';
      cameraImg.setAttribute('src', `/api/camera/stream.mjpg?t=${Date.now()}`);
      updateCameraStatus();
    } else if (!enabled && cameraImg.dataset.streaming === '1') {
      cameraImg.dataset.streaming = '0';
      cameraImg.setAttribute('src', cameraPlaceholder);
      cameraStatus.classList.remove('online', 'error');
      cameraStatus.querySelector('span:last-child').textContent = 'WHEELTEC C70 · 待机';
      window.setTimeout(updateCameraStatus, 250);
    }
  };

  cameraImg.addEventListener('error', () => {
    cameraStatus.classList.remove('online');
    cameraStatus.classList.add('error');
    cameraStatus.querySelector('span:last-child').textContent = 'RGB视频连接失败';
  });
  cameraView.addEventListener('click', (event) => event.stopPropagation());
  cameraView.addEventListener('contextmenu', (event) => event.preventDefault());

  const setPrimaryView = (view, shouldAnnounce = false) => {
    if (!['map', 'radar', 'camera'].includes(view)) view = 'map';
    primaryView = view;
    radarExpanded = view === 'radar';
    cameraExpanded = view === 'camera';
    if (cameraExpanded) cameraVisible = true;
    map.classList.toggle('radar-expanded', radarExpanded);
    map.classList.toggle('camera-expanded', cameraExpanded);
    map.classList.toggle('camera-visible', cameraVisible);
    map.classList.toggle('camera-minimized', cameraVisible && !cameraExpanded);
    gridLayerButton?.classList.toggle('primary-view', view === 'map');
    scanLayerButton?.classList.toggle('primary-view', radarExpanded);
    setCameraStreaming(cameraVisible);
    if (!radarExpanded) hideRadarTooltip();
    primaryViewButtons.forEach((button) => {
      const active = button.dataset.primaryView === view;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
    if (mapTitle) {
      mapTitle.innerHTML = cameraExpanded
        ? 'RGB实时画面 <span>WHEELTEC C70 · 车载前视摄像头</span>'
        : radarExpanded
          ? 'LD14实时扫描 <span>独立雷达坐标 · 未配准到 /map</span>'
          : '实时地图 <span>/map栅格 + LD14独立扫描层</span>';
    }
    if (shouldAnnounce) {
      const message = cameraExpanded ? '已打开RGB摄像头实时画面' : radarExpanded ? '已切换到LD14雷达主画面' : cameraVisible ? '摄像头已最小化到右下角' : '已切换到栅格地图主画面';
      announce(message, `primary-view-${view}`, 500);
    }
  };

  const hideRadarTooltip = () => {
    hoverMarker.classList.remove('visible');
    radarTooltip.classList.remove('visible');
  };

  const updateRadarHover = (clientX, clientY, preserveOnMiss = false) => {
    if (!radarHitPoints.length) {
      if (!preserveOnMiss) hideRadarTooltip();
      return false;
    }
    const screenMatrix = radar.getScreenCTM();
    if (!screenMatrix) {
      if (!preserveOnMiss) hideRadarTooltip();
      return false;
    }
    let nearest = null;
    let nearestPixels = 12;
    radarHitPoints.forEach((point) => {
      const screenPoint = new DOMPoint(point.cx, point.cy).matrixTransform(screenMatrix);
      const pixelDistance = Math.hypot(screenPoint.x - clientX, screenPoint.y - clientY);
      if (pixelDistance < nearestPixels) {
        nearest = point;
        nearestPixels = pixelDistance;
      }
    });
    if (!nearest) {
      if (!preserveOnMiss) hideRadarTooltip();
      return false;
    }
    hoverMarker.setAttribute('cx', nearest.cx);
    hoverMarker.setAttribute('cy', nearest.cy);
    hoverMarker.classList.add('visible');
    const angleDegrees = nearest.angle * 180 / Math.PI;
    const laserX = nearest.range * Math.cos(nearest.angle);
    const laserY = nearest.range * Math.sin(nearest.angle);
    radarTooltip.innerHTML = `<div class="radar-tooltip-title">LD14真实回波 · 雷达坐标</div><strong>${nearest.range.toFixed(2)} m</strong><div class="radar-tooltip-grid"><span>角度</span><b>${angleDegrees.toFixed(1)}°</b><span>X 前向</span><b>${laserX.toFixed(2)} m</b><span>Y 左向</span><b>${laserY.toFixed(2)} m</b><span>强度</span><b>${nearest.intensity.toFixed(0)}</b></div>`;
    const mapRect = map.getBoundingClientRect();
    const tooltipWidth = 188;
    const tooltipHeight = 112;
    radarTooltip.style.left = `${Math.max(8, Math.min(mapRect.width - tooltipWidth - 8, clientX - mapRect.left + 14))}px`;
    radarTooltip.style.top = `${Math.max(8, Math.min(mapRect.height - tooltipHeight - 8, clientY - mapRect.top + 14))}px`;
    radarTooltip.classList.add('visible');
    return true;
  };

  radar.addEventListener('mousemove', (event) => {
    radarPointer.inside = true;
    radarPointer.clientX = event.clientX;
    radarPointer.clientY = event.clientY;
    updateRadarHover(event.clientX, event.clientY);
  });
  radar.addEventListener('mouseleave', () => {
    radarPointer.inside = false;
    hideRadarTooltip();
  });

  const interceptPrimaryViewButton = (button, view) => {
    button?.addEventListener('click', (event) => {
      event.preventDefault();
      event.stopImmediatePropagation();
      setPrimaryView(view, true);
    }, true);
  };
  interceptPrimaryViewButton(gridLayerButton, 'map');
  interceptPrimaryViewButton(scanLayerButton, 'radar');
  primaryViewButtons.forEach((button) => button.addEventListener('click', () => setPrimaryView(button.dataset.primaryView, true)));
  radarMinimizeButton.addEventListener('click', () => setPrimaryView('map', true));
  cameraMinimizeButton.addEventListener('click', (event) => {
    event.preventDefault();
    event.stopPropagation();
    setPrimaryView('map', true);
  });
  cameraView.addEventListener('pointerdown', (event) => {
    if (!cameraVisible || cameraExpanded || event.button !== 0) return;
    const rect = cameraView.getBoundingClientRect();
    cameraDragging = true;
    cameraDragMoved = false;
    cameraDragPointerId = event.pointerId;
    cameraDragOffsetX = event.clientX - rect.left;
    cameraDragOffsetY = event.clientY - rect.top;
    cameraView.classList.add('dragging');
    cameraView.setPointerCapture?.(event.pointerId);
    event.preventDefault();
  });
  cameraView.addEventListener('pointermove', (event) => {
    if (!cameraDragging || event.pointerId !== cameraDragPointerId) return;
    const mapRect = map.getBoundingClientRect();
    const viewRect = cameraView.getBoundingClientRect();
    const left = Math.max(4, Math.min(mapRect.width - viewRect.width - 4, event.clientX - mapRect.left - cameraDragOffsetX));
    const top = Math.max(4, Math.min(mapRect.height - viewRect.height - 4, event.clientY - mapRect.top - cameraDragOffsetY));
    if (Math.abs(left - (viewRect.left - mapRect.left)) > 2 || Math.abs(top - (viewRect.top - mapRect.top)) > 2) cameraDragMoved = true;
    cameraView.style.left = `${left}px`;
    cameraView.style.top = `${top}px`;
    cameraView.style.right = 'auto';
    cameraView.style.bottom = 'auto';
    event.preventDefault();
  });
  const finishCameraDrag = (event) => {
    if (!cameraDragging || (event.pointerId !== undefined && event.pointerId !== cameraDragPointerId)) return;
    cameraDragging = false;
    cameraView.classList.remove('dragging');
    if (cameraDragPointerId !== null) cameraView.releasePointerCapture?.(cameraDragPointerId);
    cameraDragPointerId = null;
  };
  cameraView.addEventListener('pointerup', finishCameraDrag);
  cameraView.addEventListener('pointercancel', finishCameraDrag);
  cameraView.addEventListener('click', () => {
    if (cameraVisible && !cameraExpanded && !cameraDragMoved) setPrimaryView('camera', true);
    cameraDragMoved = false;
  });
  window.addEventListener('pagehide', () => setCameraStreaming(false));
  window.setInterval(() => { if (primaryView === 'camera') updateCameraStatus(); }, 1500);
  setPrimaryView('map');

  const stage = document.createElement('div');
  stage.className = 'live-stage-badge';
  stage.textContent = '正在连接实时雷达网关…';
  map.appendChild(stage);

  const mapStage = document.createElement('div');
  mapStage.className = 'live-map-badge';
  mapStage.textContent = '等待ROS2 /map…';
  map.appendChild(mapStage);

  const legend = root.querySelector('.map-legend.rich');
  if (legend) legend.innerHTML = '<span class="legend-item"><i class="swatch free"></i>可通行</span><span class="legend-item"><i class="swatch occupied"></i>障碍物</span><span class="legend-item"><i class="swatch" style="background:#27343d;height:8px;width:8px"></i>未知</span><span class="legend-item"><i class="swatch scan-dot"></i>LD14独立扫描</span><span class="legend-item"><i class="swatch blue"></i>选中格</span>';
  const demoBadge = root.querySelector('.demo-badge');
  if (demoBadge) demoBadge.textContent = '真实LD14 · /map等待中 · 暂无TF配准';

  const setMetric = (index, label, value, unit, sub) => {
    const item = telemetry[index];
    if (!item) return;
    item.querySelector('.metric-label').textContent = label;
    item.querySelector('.metric-value').innerHTML = `${value}<span class="metric-unit">${unit || ''}</span>`;
    item.querySelector('.metric-sub').textContent = sub;
  };
  setMetric(0, '雷达频率', '--', 'Hz', '等待 /scan');
  setMetric(1, '有效点数', '--', '点', '等待有效帧');
  setMetric(2, '最近障碍', '--', 'm', 'LD14量程下限0.15m');
  setMetric(3, '最远回波', '--', 'm', '当前帧有效回波');
  setMetric(4, 'STM32底盘', '未接入', '', '本阶段仅验证RDK + LD14');
  setMetric(5, '地图栅格', '等待', '', '订阅ROS2 /map');

  let mapMeta = null;
  let selectionMarker = null;
  let selectionArrowGroup = null;
  let waypointLayer = null;
  let targetPose = null;
  let activePoseKind = 'goal';
  let selectedWaypointIndex = -1;
  let headingDragActive = false;
  let headingDragPending = false;
  let headingDragPointerId = null;
  let headingDragTimer = null;
  let headingDragLastEvent = null;
  let suppressMapClickUntil = 0;
  let patrolEditMode = false;
  let patrolDraft = {id: null, name: '新建巡逻任务', repeat_count: 1, waypoints: []};
  let savedPatrolSnapshot = null;
  let loadedMapId = null;
  let taskMotionEnabled = false;
  const simLayers = new Map();
  let announcedMapSignature = '';
  const renderMap = (data) => {
    if (!occupancy || data.data.length !== data.width * data.height) return;
    mapMeta = data;
    occupancy.innerHTML = '';
    occupancy.setAttribute('viewBox', `0 0 ${data.width} ${data.height}`);
    occupancy.setAttribute('preserveAspectRatio', 'xMidYMid meet');
    occupancy.classList.add('live-map-active');
    map.classList.add('map-present');
    occupancy.dataset.freeCount = String(data.counts.free);
    occupancy.dataset.occupiedCount = String(data.counts.occupied);
    occupancy.dataset.unknownCount = String(data.counts.unknown);
    const canvas = document.createElement('canvas');
    canvas.width = data.width;
    canvas.height = data.height;
    const context = canvas.getContext('2d', { alpha: false });
    const pixels = context.createImageData(data.width, data.height);
    data.data.forEach((value, index) => {
      const col = index % data.width;
      const rosRow = Math.floor(index / data.width);
      const svgRow = data.height - 1 - rosRow;
      const pixelIndex = (svgRow * data.width + col) * 4;
      const color = value < 0 ? [23, 36, 44] : value >= 50 ? [189, 57, 69] : [22, 124, 85];
      pixels.data[pixelIndex] = color[0];
      pixels.data[pixelIndex + 1] = color[1];
      pixels.data[pixelIndex + 2] = color[2];
      pixels.data[pixelIndex + 3] = 255;
    });
    context.putImageData(pixels, 0, 0);
    const image = document.createElementNS(ns, 'image');
    image.setAttribute('class', 'live-map-image');
    image.setAttribute('width', data.width);
    image.setAttribute('height', data.height);
    image.setAttribute('preserveAspectRatio', 'none');
    image.setAttribute('href', canvas.toDataURL('image/png'));
    image.dataset.layerTarget = 'grid';
    occupancy.appendChild(image);
    const defs = document.createElementNS(ns, 'defs');
    defs.innerHTML = '<pattern id="bee-map-grid-pattern" width="1" height="1" patternUnits="userSpaceOnUse"><path d="M1 0H0V1" fill="none" stroke="#102029" stroke-width="0.035"/></pattern>';
    occupancy.appendChild(defs);
    const gridOverlay = document.createElementNS(ns, 'rect');
    gridOverlay.setAttribute('class', 'live-map-grid-overlay');
    gridOverlay.setAttribute('width', data.width);
    gridOverlay.setAttribute('height', data.height);
    gridOverlay.dataset.layerTarget = 'grid';
    occupancy.appendChild(gridOverlay);
    selectionArrowGroup = document.createElementNS(ns, 'g');
    selectionArrowGroup.setAttribute('class', 'live-goal-control');
    selectionArrowGroup.setAttribute('visibility', 'hidden');
    selectionArrowGroup.innerHTML = '<circle class="live-goal-center-hit" r=".72"></circle><line class="live-goal-arrow-hit" x1=".12" y1="0" x2="1.82" y2="0"></line><circle class="live-map-selection" r=".42"></circle><line class="live-goal-arrow" x1=".22" y1="0" x2="1.45" y2="0"></line><path class="live-goal-arrowhead" d="M1.72 0 L1.30 -.29 L1.30 .29 Z"></path><circle class="live-goal-handle" cx="1.86" cy="0" r=".27"></circle>';
    occupancy.appendChild(selectionArrowGroup);
    selectionMarker = selectionArrowGroup.querySelector('.live-map-selection');
    waypointLayer = document.createElementNS(ns, 'g');
    waypointLayer.dataset.layerTarget = 'waypoint';
    occupancy.appendChild(waypointLayer);
    simLayers.clear();
    renderGoalPose();
    renderPatrolMarkers();

    const mapLabel = data.is_test ? '测试地图' : '真实 /map';
    mapStage.textContent = `${mapLabel} · ${data.width}×${data.height} · ${data.resolution.toFixed(2)} m/格`;
    mapStage.className = `live-map-badge ${data.is_test ? 'test' : 'online'}`;
    if (mapReadout[0]) mapReadout[0].innerHTML = `<span class="dot ${data.is_test ? 'amber' : ''}"></span><b>${mapLabel}</b>`;
    if (mapReadout[3]) mapReadout[3].innerHTML = `地图 <b>${data.frame_id}</b>`;
    const missionRows = [...(mission?.querySelectorAll('.mission-row') || [])];
    if (missionRows[2]) missionRows[2].innerHTML = `<span>地图来源</span><span>${mapLabel}</span>`;
    setMetric(5, '地图栅格', `${data.width}×${data.height}`, '', `绿${data.counts.free} · 红${data.counts.occupied} · 灰${data.counts.unknown}`);
    if (demoBadge) demoBadge.textContent = `${mapLabel} + 真实LD14 · 暂无map→laser TF配准`;
    const mapSignature = `${data.frame_id}:${data.width}x${data.height}:${data.resolution}`;
    if (announcedMapSignature !== mapSignature) {
      announcedMapSignature = mapSignature;
      announce(`${mapLabel}已接收，地图尺寸${data.width}乘${data.height}`, `map-ready-${mapSignature}`, 0);
    }
  };

  const clearMap = () => {
    mapMeta = null;
    targetPose = null;
    activePoseKind = 'goal';
    selectedWaypointIndex = -1;
    patrolEditMode = false;
    patrolDraft = {id: null, name: '新建巡逻任务', map_id: null, repeat_count: 1, waypoints: []};
    savedPatrolSnapshot = null;
    loadedMapId = null;
    occupancy.replaceChildren();
    occupancy.classList.remove('live-map-active');
    map.classList.remove('map-present', 'patrol-editing');
    selectionMarker = null;
    selectionArrowGroup = null;
    waypointLayer = null;
    simLayers.clear();
    mapStage.textContent = '等待ROS2 /map…';
    mapStage.className = 'live-map-badge';
    if (mapReadout[0]) mapReadout[0].innerHTML = '<span class="dot amber"></span><b>等待 /map</b>';
    if (mapReadout[1]) mapReadout[1].innerHTML = '坐标 <b>--</b>';
    if (mapReadout[3]) mapReadout[3].innerHTML = '地图 <b>未加载</b>';
    const missionRows = [...(mission?.querySelectorAll('.mission-row') || [])];
    if (missionRows[2]) missionRows[2].innerHTML = '<span>地图来源</span><span>等待 /map</span>';
    setMetric(5, '地图栅格', '等待', '', '订阅ROS2 /map');
    if (demoBadge) demoBadge.textContent = '真实LD14 · /map等待中 · 暂无TF配准';
    if (poseEditor) poseEditor.classList.remove('open');
    if (patrolDialog) {
      patrolDialog.querySelector('[data-patrol-name]').value = patrolDraft.name;
      patrolDialog.querySelector('[data-patrol-saved]').replaceChildren(new Option('新建任务', ''));
      patrolDialog.querySelector('[data-patrol-tool="delete"]').disabled = true;
      renderPatrolEditorList();
      updatePatrolSummary();
    }
    announce('保存地图已卸载，地图显示已清空', 'map-cleared', 0);
  };

  const mapPointToSvg = (x, y) => {
    if (!mapMeta) return null;
    const dx = x - mapMeta.origin.x;
    const dy = y - mapMeta.origin.y;
    const cosYaw = Math.cos(mapMeta.origin.yaw);
    const sinYaw = Math.sin(mapMeta.origin.yaw);
    const localX = (cosYaw * dx + sinYaw * dy) / mapMeta.resolution;
    const localY = (-sinYaw * dx + cosYaw * dy) / mapMeta.resolution;
    return { x: localX, y: mapMeta.height - localY };
  };
  const layerGroup = (name) => {
    let group = simLayers.get(name);
    if (!group || !group.isConnected) {
      group = document.createElementNS(ns, 'g');
      group.dataset.simLayer = name;
      group.dataset.layerTarget = name;
      occupancy.appendChild(group);
      simLayers.set(name, group);
    }
    return group;
  };
  const updateLayerVisibility = (name) => {
    const button = layerButtons.find((item) => item.dataset.layer === name);
    const group = simLayers.get(name);
    if (group) group.dataset.hidden = button?.classList.contains('active') ? '0' : '1';
  };
  layerButtons.forEach((button) => button.addEventListener('click', () => {
    if (button.dataset.layer === 'grid' || button.dataset.layer === 'scan') return;
    button.classList.toggle('active');
    updateLayerVisibility(button.dataset.layer);
  }));
  const renderSimLayer = (data) => {
    if (!mapMeta || data.frame_id !== mapMeta.frame_id) return;
    const group = layerGroup(data.layer);
    group.replaceChildren();
    if (['global', 'local', 'trail'].includes(data.layer)) {
      const points = data.points.map(([x, y]) => mapPointToSvg(x, y)).filter(Boolean);
      const line = document.createElementNS(ns, 'polyline');
      line.setAttribute('class', `sim-${data.layer}`);
      line.setAttribute('points', points.map((point) => `${point.x},${point.y}`).join(' '));
      group.appendChild(line);
    } else if (data.layer === 'robot' || data.layer === 'goal') {
      const point = mapPointToSvg(data.pose.x, data.pose.y);
      if (point) {
        const marker = document.createElementNS(ns, data.layer === 'robot' ? 'path' : 'circle');
        marker.setAttribute('class', `sim-${data.layer}`);
        if (data.layer === 'robot') {
          marker.setAttribute('d', 'M0,-.55 L.38,.42 L0,.25 L-.38,.42 Z');
          marker.setAttribute('transform', `translate(${point.x} ${point.y}) rotate(${data.pose.yaw * 180 / Math.PI})`);
        } else {
          marker.setAttribute('cx', point.x); marker.setAttribute('cy', point.y); marker.setAttribute('r', '.45');
        }
        group.appendChild(marker);
      }
    } else if (data.layer === 'waypoint' || data.layer === 'obstacle') {
      data.poses.forEach(([x, y], index) => {
        const point = mapPointToSvg(x, y);
        if (!point) return;
        const marker = document.createElementNS(ns, 'circle');
        marker.setAttribute('class', `sim-${data.layer}`);
        marker.setAttribute('cx', point.x); marker.setAttribute('cy', point.y); marker.setAttribute('r', data.layer === 'waypoint' ? '.34' : '.42');
        group.appendChild(marker);
        if (data.layer === 'waypoint') {
          const label = document.createElementNS(ns, 'text'); label.setAttribute('x', point.x + .4); label.setAttribute('y', point.y - .3); label.setAttribute('fill', '#bcd8ff'); label.setAttribute('font-size', '.65'); label.textContent = `${index + 1}`; group.appendChild(label);
        }
      });
    } else if (data.layer === 'cost' && data.width === mapMeta.width && data.height === mapMeta.height) {
      data.data.forEach((value, index) => {
        if (value <= 0) return;
        const rect = document.createElementNS(ns, 'rect');
        rect.setAttribute('class', 'sim-cost'); rect.setAttribute('x', index % data.width); rect.setAttribute('y', data.height - 1 - Math.floor(index / data.width)); rect.setAttribute('width', '1'); rect.setAttribute('height', '1'); rect.setAttribute('opacity', Math.min(.55, value / 150)); group.appendChild(rect);
      });
    }
    updateLayerVisibility(data.layer);
    if (demoBadge) demoBadge.textContent = '仿真八图层 + 真实LD14 · 非实车状态';
  };

  const clientToMap = (event) => {
    if (!mapMeta || !occupancy) return null;
    const rect = occupancy.getBoundingClientRect();
    const scale = Math.min(rect.width / mapMeta.width, rect.height / mapMeta.height);
    const drawWidth = mapMeta.width * scale;
    const drawHeight = mapMeta.height * scale;
    const offsetX = (rect.width - drawWidth) / 2;
    const offsetY = (rect.height - drawHeight) / 2;
    const localX = (event.clientX - rect.left - offsetX) / scale;
    const svgY = (event.clientY - rect.top - offsetY) / scale;
    if (localX < 0 || svgY < 0 || localX >= mapMeta.width || svgY >= mapMeta.height) return null;
    const localY = mapMeta.height - svgY;
    const cosYaw = Math.cos(mapMeta.origin.yaw);
    const sinYaw = Math.sin(mapMeta.origin.yaw);
    const metricX = localX * mapMeta.resolution;
    const metricY = localY * mapMeta.resolution;
    return {
      col: Math.floor(localX),
      row: Math.max(0, Math.min(mapMeta.height - 1, Math.floor(localY))),
      svgX: localX,
      svgY,
      x: mapMeta.origin.x + cosYaw * metricX - sinYaw * metricY,
      y: mapMeta.origin.y + sinYaw * metricX + cosYaw * metricY,
    };
  };

  const normalizeYaw = (yaw) => {
    let value = Number(yaw) || 0;
    while (value > Math.PI) value -= Math.PI * 2;
    while (value <= -Math.PI) value += Math.PI * 2;
    return value;
  };
  const yawDegrees = (yaw) => Math.round(normalizeYaw(yaw) * 180 / Math.PI);
  const goalMarkerScale = () => {
    const matrix = occupancy?.getScreenCTM?.();
    const pixelsPerMapUnit = matrix ? Math.hypot(matrix.a, matrix.b) : 0;
    if (!Number.isFinite(pixelsPerMapUnit) || pixelsPerMapUnit <= 0) return 1.35;
    return Math.max(1.35, Math.min(42, 10 / (.42 * pixelsPerMapUnit)));
  };
  const activePose = () => activePoseKind === 'waypoint' && patrolDraft.waypoints[selectedWaypointIndex]
    ? patrolDraft.waypoints[selectedWaypointIndex] : targetPose;
  const pointValue = (point) => mapMeta?.data?.[point.row * mapMeta.width + point.col];
  const isTraversable = (point) => {
    const value = pointValue(point);
    return Number.isFinite(value) && value >= 0 && value < 50;
  };
  const mapFeedback = root.querySelector('.map-feedback');
  let mapFeedbackTimer = null;
  const showMapFeedback = (message, error = false) => {
    if (!mapFeedback) return;
    clearTimeout(mapFeedbackTimer);
    mapFeedback.textContent = message;
    mapFeedback.classList.toggle('error', error);
    mapFeedback.classList.add('show');
    mapFeedbackTimer = setTimeout(() => mapFeedback.classList.remove('show'), 3200);
  };
  const renderGoalPose = () => {
    if (!selectionArrowGroup || !mapMeta) return;
    const pose = activePose();
    if (!pose) {
      selectionArrowGroup.setAttribute('visibility', 'hidden');
      return;
    }
    const point = mapPointToSvg(pose.x, pose.y);
    if (!point) return;
    const screenDegrees = -(normalizeYaw(pose.yaw) - mapMeta.origin.yaw) * 180 / Math.PI;
    selectionArrowGroup.setAttribute('transform', `translate(${point.x} ${point.y}) rotate(${screenDegrees}) scale(${goalMarkerScale()})`);
    selectionArrowGroup.setAttribute('visibility', 'visible');
    const editor = root.querySelector('.goal-pose-editor');
    if (editor) {
      editor.classList.add('open');
      editor.querySelector('[data-goal-angle-range]').value = String(yawDegrees(pose.yaw));
      editor.querySelector('[data-goal-angle-number]').value = String(yawDegrees(pose.yaw));
      editor.querySelector('[data-goal-pose-label]').textContent = activePoseKind === 'waypoint'
        ? `航点 ${selectedWaypointIndex + 1}` : '目标朝向';
      editor.querySelector('[data-goal-pose-coordinate]').textContent = `${pose.x.toFixed(2)}, ${pose.y.toFixed(2)} m`;
    }
    if (mapReadout[2]) mapReadout[2].innerHTML = `航向 <b>${yawDegrees(pose.yaw)}°</b>`;
  };
  const renderPatrolMarkers = () => {
    if (!waypointLayer || !mapMeta) return;
    waypointLayer.replaceChildren();
    patrolDraft.waypoints.forEach((pose, index) => {
      const point = mapPointToSvg(pose.x, pose.y);
      if (!point) return;
      const group = document.createElementNS(ns, 'g');
      const localYaw = -(normalizeYaw(pose.yaw) - mapMeta.origin.yaw) * 180 / Math.PI;
      group.setAttribute('transform', `translate(${point.x} ${point.y}) scale(${goalMarkerScale()})`);
      group.dataset.waypointIndex = String(index);
      group.innerHTML = `<circle class="live-waypoint-hit" r=".72"></circle><line class="live-waypoint-heading" x1=".30" y1="0" x2="1.02" y2="0" transform="rotate(${localYaw})"></line><circle class="live-waypoint-marker${index === selectedWaypointIndex ? ' selected' : ''}" r=".42"></circle><text class="live-waypoint-label" y=".02">${index + 1}</text>`;
      waypointLayer.appendChild(group);
    });
    const waypointButton = layerButtons.find((button) => button.dataset.layer === 'waypoint');
    waypointLayer.dataset.hidden = waypointButton?.classList.contains('active') ? '0' : '1';
  };
  const updatePatrolSummary = () => {
    const repeat = Math.max(1, Math.min(99, Number(patrolDraft.repeat_count) || 1));
    patrolDraft.repeat_count = repeat;
    root.querySelectorAll('[data-patrol-repeat]').forEach((input) => { input.value = String(repeat); });
    const summary = root.querySelector('[data-patrol-summary]');
    const name = root.querySelector('[data-patrol-name-summary]');
    if (summary) summary.textContent = `${patrolDraft.waypoints.length} 个航点 · ${repeat} 次`;
    if (name) name.textContent = patrolDraft.name || '未命名任务';
    const save = patrolDialog.querySelector('[data-patrol-tool="save"]');
    if (save) save.disabled = !loadedMapId || !patrolDraft.waypoints.length;
    const start = root.querySelector('[data-patrol-action="start"]');
    if (start) {
      start.disabled = !taskMotionEnabled || !loadedMapId || !patrolDraft.waypoints.length;
      start.title = taskMotionEnabled ? '按设置次数开始巡逻' : '实体底盘、定位和Nav2验收后才允许启动';
    }
  };
  const selectPose = (pose, kind = 'goal', waypointIndex = -1) => {
    if (kind === 'goal') targetPose = {x: pose.x, y: pose.y, yaw: normalizeYaw(pose.yaw ?? targetPose?.yaw ?? 0)};
    activePoseKind = kind;
    selectedWaypointIndex = kind === 'waypoint' ? waypointIndex : -1;
    renderPatrolMarkers();
    renderGoalPose();
  };
  const setActiveYawDegrees = (degrees) => {
    const pose = activePose();
    if (!pose) return;
    const bounded = Math.max(-180, Math.min(180, Number(degrees) || 0));
    pose.yaw = normalizeYaw(bounded * Math.PI / 180);
    renderGoalPose();
    renderPatrolMarkers();
    renderPatrolEditorList();
  };

  const poseEditor = document.createElement('div');
  poseEditor.className = 'goal-pose-editor';
  poseEditor.innerHTML = '<strong data-goal-pose-label>目标朝向</strong><input type="range" data-goal-angle-range min="-180" max="180" step="1" value="0" aria-label="目标朝向角度"><input type="number" data-goal-angle-number min="-180" max="180" step="1" value="0" aria-label="目标朝向角度数值"><button type="button" data-goal-navigate>确认导航</button><span class="pose-hint"><b data-goal-pose-coordinate>--</b> · 左键按住蓝色目标点或箭头后拖动，松开固定方向；0°朝地图X正方向</span>';
  map.appendChild(poseEditor);
  poseEditor.querySelector('[data-goal-angle-range]').addEventListener('input', (event) => setActiveYawDegrees(event.target.value));
  poseEditor.querySelector('[data-goal-angle-number]').addEventListener('change', (event) => setActiveYawDegrees(event.target.value));

  const patrolDialog = document.createElement('div');
  patrolDialog.className = 'patrol-editor-dialog';
  patrolDialog.setAttribute('role', 'dialog');
  patrolDialog.setAttribute('aria-modal', 'true');
  patrolDialog.innerHTML = `<div class="patrol-editor-card"><div class="patrol-editor-head"><div>巡逻航点编辑器 <small>地图坐标 / yaw</small></div><button class="patrol-editor-close" type="button" aria-label="关闭航点编辑">×</button></div><div class="patrol-editor-body"><div class="patrol-form"><label>任务名称<input type="text" data-patrol-name maxlength="80" value="新建巡逻任务"></label><label>巡逻次数（1–99）<input type="number" data-patrol-repeat min="1" max="99" value="1"></label></div><div class="patrol-editor-note">打开编辑后，在绿色可通行格上单击即可依次添加航点；选中航点后可拖动地图蓝色箭头设置到点朝向。红色障碍格和深灰未知格不能添加。</div><div class="patrol-editor-toolbar"><button class="primary" type="button" data-patrol-tool="map-add">地图点击添加：关闭</button><button type="button" data-patrol-tool="add-current">添加当前目标</button><button class="danger-lite" type="button" data-patrol-tool="clear">清空航点</button><span class="patrol-map-status" data-patrol-map-status>读取地图状态…</span></div><div class="patrol-waypoint-list"></div><div class="patrol-editor-footer"><button type="button" data-patrol-tool="close">完成编辑</button><button class="primary" type="button" data-patrol-tool="save">保存到地瓜派</button></div></div></div>`;
  patrolDialog.querySelector('.patrol-editor-body').insertAdjacentHTML('afterbegin', '<div class="patrol-saved"><label>已保存任务<select data-patrol-saved aria-label="选择已保存巡逻任务"><option value="">新建任务</option></select></label><button type="button" data-patrol-tool="new" title="新建任务">新建</button><button type="button" data-patrol-tool="copy" title="另存为新任务">另存</button><button type="button" data-patrol-tool="delete" title="删除当前已保存任务">删除</button></div>');
  root.appendChild(patrolDialog);
  const patrolList = patrolDialog.querySelector('.patrol-waypoint-list');
  const renderPatrolEditorList = () => {
    if (!patrolList) return;
    patrolList.replaceChildren();
    if (!patrolDraft.waypoints.length) {
      const empty = document.createElement('div');
      empty.className = 'patrol-editor-empty';
      empty.textContent = '暂无航点。打开“地图点击添加”，然后在绿色地图格上依次单击。';
      patrolList.appendChild(empty);
    }
    patrolDraft.waypoints.forEach((pose, index) => {
      const row = document.createElement('div');
      row.className = `patrol-waypoint-row${index === selectedWaypointIndex ? ' selected' : ''}`;
      row.innerHTML = `<span class="patrol-waypoint-index">${index + 1}</span><span class="patrol-waypoint-coord">X ${pose.x.toFixed(2)} · Y ${pose.y.toFixed(2)}<small>map坐标</small></span><label class="patrol-waypoint-yaw">朝向 <input type="number" min="-180" max="180" step="1" value="${yawDegrees(pose.yaw)}" data-waypoint-yaw></label><span class="patrol-waypoint-actions"><button type="button" data-waypoint-move="-1" title="上移">↑</button><button type="button" data-waypoint-move="1" title="下移">↓</button><button type="button" data-waypoint-delete title="删除">×</button></span>`;
      row.addEventListener('click', (event) => {
        if (event.target.closest('button,input')) return;
        selectPose(pose, 'waypoint', index);
        renderPatrolEditorList();
      });
      row.querySelector('[data-waypoint-yaw]').addEventListener('change', (event) => {
        selectPose(pose, 'waypoint', index);
        setActiveYawDegrees(event.target.value);
      });
      row.querySelectorAll('[data-waypoint-move]').forEach((button) => button.addEventListener('click', () => {
        const destination = index + Number(button.dataset.waypointMove);
        if (destination < 0 || destination >= patrolDraft.waypoints.length) return;
        [patrolDraft.waypoints[index], patrolDraft.waypoints[destination]] = [patrolDraft.waypoints[destination], patrolDraft.waypoints[index]];
        selectedWaypointIndex = destination;
        renderPatrolMarkers(); renderPatrolEditorList(); updatePatrolSummary();
      }));
      row.querySelector('[data-waypoint-delete]').addEventListener('click', () => {
        patrolDraft.waypoints.splice(index, 1);
        selectedWaypointIndex = Math.min(selectedWaypointIndex, patrolDraft.waypoints.length - 1);
        if (selectedWaypointIndex >= 0) selectPose(patrolDraft.waypoints[selectedWaypointIndex], 'waypoint', selectedWaypointIndex);
        else { activePoseKind = 'goal'; renderGoalPose(); }
        renderPatrolMarkers(); renderPatrolEditorList(); updatePatrolSummary();
      });
      patrolList.appendChild(row);
    });
  };
  const setPatrolEditMode = (enabled) => {
    patrolEditMode = Boolean(enabled);
    if (patrolEditMode) positionPatrolEditor();
    const button = patrolDialog.querySelector('[data-patrol-tool="map-add"]');
    button.textContent = patrolEditMode ? '结束地图添加' : '地图点击添加';
    button.classList.toggle('primary', patrolEditMode);
    map.classList.toggle('patrol-editing', patrolEditMode);
    patrolDialog.classList.toggle('map-pick-mode', patrolEditMode);
    patrolDialog.setAttribute('aria-modal', patrolEditMode ? 'false' : 'true');
    if (patrolEditMode) showMapFeedback('航点添加已开启：请单击绿色可通行格');
  };
  const positionPatrolEditor = () => {
    const top = Math.min(Math.max(map.getBoundingClientRect().top + 80, 76), window.innerHeight - 240);
    patrolDialog.style.setProperty('--patrol-map-top', `${top}px`);
  };
  window.addEventListener('resize', () => { if (patrolEditMode) positionPatrolEditor(); });
  const savedSelect = patrolDialog.querySelector('[data-patrol-saved]');
  const taskReasonLabels = {
    physical_chassis_not_verified: '等待实体底盘验收',
    chassis_driver_offline: '等待STM32底盘驱动',
    laser_scan_offline: '等待LD14有效扫描',
    map_not_loaded: '等待加载任务地图',
    nav2_stack_offline: '等待Nav2导航栈',
    task_executor_not_ready: '等待任务执行器部署',
    motion_interlock_closed: '运动安全门保持关闭',
  };
  const setTaskCard = (name, ready, subtitle, stateText) => {
    const card = root.querySelector(`[data-task-card="${name}"]`);
    if (!card) return;
    card.classList.toggle('ready', ready);
    card.classList.toggle('blocked', !ready);
    card.classList.remove('active', 'disabled');
    const sub = card.querySelector('.task-sub');
    const state = card.querySelector('.task-state');
    if (sub) sub.textContent = subtitle;
    if (state) state.textContent = stateText;
  };
  const updateTaskPanel = (status) => {
    const prerequisites = status?.prerequisites || {};
    const locked = !status?.motion_enabled;
    const reason = taskReasonLabels[status?.reason] || (status?.reason ? String(status.reason) : '任务状态不可用');
    const readyCount = Number(status?.ready_count || 0);
    const requiredCount = Number(status?.required_count || 6);
    const modeLabel = root.querySelector('[data-task-mode]');
    if (modeLabel) modeLabel.textContent = status ? `ROS2 / Nav2 · ${locked ? '安全锁定' : '运动已解锁'}` : 'ROS2 / Nav2 · 状态离线';
    setTaskCard(
      'navigate',
      Boolean(status?.motion_enabled),
      status?.motion_enabled ? '定位与Nav2已就绪，可下发目标' : reason,
      status?.motion_enabled ? '就绪' : '锁定',
    );
    setTaskCard(
      'avoidance',
      Boolean(prerequisites.laser_scan && prerequisites.nav2_stack),
      prerequisites.laser_scan ? (prerequisites.nav2_stack ? '扫描与局部规划链路就绪' : 'LD14在线，等待Nav2局部规划') : '等待LD14有效扫描',
      prerequisites.laser_scan && prerequisites.nav2_stack ? '就绪' : '待检',
    );
    setTaskCard(
      'planning',
      Boolean(prerequisites.map_loaded && prerequisites.nav2_stack),
      prerequisites.map_loaded ? (prerequisites.nav2_stack ? '地图与Nav2规划器就绪' : '地图已加载，等待Nav2') : '等待加载任务地图',
      prerequisites.map_loaded && prerequisites.nav2_stack ? '就绪' : '待检',
    );
    setTaskCard(
      'patrol',
      Boolean(status?.motion_enabled),
      status?.motion_enabled ? '可执行已保存多航点任务' : '可编辑保存，实体执行未解锁',
      status?.motion_enabled ? '就绪' : '编辑可用',
    );
    const stateField = root.querySelector('[data-task-field="state"]');
    const readinessField = root.querySelector('[data-task-field="readiness"]');
    const reasonField = root.querySelector('[data-task-field="reason"]');
    if (stateField) stateField.textContent = status ? (locked ? '安全锁定' : String(status.state || '待机')) : '状态离线';
    if (readinessField) readinessField.textContent = `${readyCount} / ${requiredCount}`;
    if (reasonField) {
      reasonField.textContent = reason;
      reasonField.title = reason;
    }
    const progress = root.querySelector('[data-task-progress]');
    if (progress) progress.style.width = `${Math.max(0, Math.min(100, Number(status?.progress_percent || 0)))}%`;
    root.querySelectorAll('[data-task-action]').forEach((button) => {
      const action = button.dataset.taskAction;
      button.disabled = !Array.isArray(status?.allowed_actions) || !status.allowed_actions.includes(action);
    });
  };
  const draftSnapshot = () => JSON.stringify({id: patrolDraft.id, name: patrolDialog.querySelector('[data-patrol-name]').value.trim(), repeat_count: patrolDraft.repeat_count, waypoints: patrolDraft.waypoints});
  const hasUnsavedPatrol = () => Boolean(patrolDraft.waypoints.length || patrolDraft.id) && draftSnapshot() !== savedPatrolSnapshot;
  const confirmReplacePatrol = () => !hasUnsavedPatrol() || window.confirm('当前巡逻任务有未保存的修改，确定放弃这些修改吗？');
  const freshPatrol = () => ({id: null, name: '新建巡逻任务', map_id: loadedMapId, repeat_count: 1, waypoints: []});
  const applyPatrol = (draft) => {
    patrolDraft = structuredClone(draft);
    selectedWaypointIndex = -1;
    activePoseKind = 'goal';
    patrolDialog.querySelector('[data-patrol-name]').value = patrolDraft.name;
    savedSelect.value = patrolDraft.id || '';
    patrolDialog.querySelector('[data-patrol-tool="delete"]').disabled = !patrolDraft.id;
    renderPatrolMarkers(); renderPatrolEditorList(); renderGoalPose(); updatePatrolSummary();
    savedPatrolSnapshot = draftSnapshot();
  };
  const refreshSavedPatrols = async () => {
    const mapId = loadedMapId;
    const items = mapId ? await (async () => {
      const response = await fetch(`/api/patrols?map_id=${encodeURIComponent(mapId)}`, {cache: 'no-store'});
      if (!response.ok) throw new Error('patrol_list_failed');
      return (await response.json()).items || [];
    })() : [];
    if (mapId !== loadedMapId) return [];
    items.sort((a, b) => (b.updated_at || '').localeCompare(a.updated_at || ''));
    savedSelect.replaceChildren(new Option('新建任务', ''));
    items.forEach((item) => savedSelect.add(new Option(`${item.name} · ${item.waypoints.length}点 · ${item.repeat_count}次`, item.id)));
    savedSelect.value = items.some((item) => item.id === patrolDraft.id) ? patrolDraft.id : '';
    patrolDialog.querySelector('[data-patrol-tool="delete"]').disabled = !savedSelect.value;
    return items;
  };
  const refreshTaskContext = async () => {
    let taskStatus = null;
    try {
      const [mapsResponse, tasksResponse] = await Promise.all([
        fetch('/api/maps', {cache: 'no-store'}), fetch('/api/tasks', {cache: 'no-store'}),
      ]);
      const maps = await mapsResponse.json();
      const tasks = await tasksResponse.json();
      const nextMapId = maps?.status?.load_id || null;
      if (patrolDraft.map_id && patrolDraft.map_id !== nextMapId) {
        patrolDraft = {id: null, name: '新建巡逻任务', map_id: nextMapId, repeat_count: 1, waypoints: []};
        savedPatrolSnapshot = null;
        selectedWaypointIndex = -1;
        activePoseKind = 'goal';
        patrolDialog.querySelector('[data-patrol-name]').value = patrolDraft.name;
        renderPatrolMarkers(); renderPatrolEditorList(); renderGoalPose();
        showMapFeedback('地图已切换，已新建该地图的巡逻草稿');
      }
      loadedMapId = nextMapId;
      taskStatus = tasks;
      taskMotionEnabled = Boolean(tasks?.motion_enabled);
    } catch (_error) {
      loadedMapId = null; taskMotionEnabled = false;
    }
    updateTaskPanel(taskStatus);
    const status = patrolDialog.querySelector('[data-patrol-map-status]');
    status.textContent = loadedMapId ? `已加载地图：${loadedMapId}` : '尚未加载已保存地图 · 可编辑，暂不能板端保存/执行';
    patrolDialog.querySelector('[data-patrol-tool="save"]').disabled = !loadedMapId || !patrolDraft.waypoints.length;
    poseEditor.querySelector('[data-goal-navigate]').disabled = !taskMotionEnabled || !loadedMapId || !targetPose;
    updatePatrolSummary();
  };
  const openPatrolEditor = async () => {
    root.querySelector('.side-patrol')?.classList.remove('disabled');
    patrolDialog.classList.add('open');
    patrolDialog.querySelector('[data-patrol-name]').value = patrolDraft.name;
    renderPatrolEditorList();
    await refreshTaskContext();
    try {
      const items = await refreshSavedPatrols();
      if (!patrolDraft.id && !patrolDraft.waypoints.length && items.length && !savedPatrolSnapshot) {
          applyPatrol(items[0]);
          showMapFeedback('已恢复该地图最近保存的巡逻任务');
      }
    } catch (_error) { showMapFeedback('已保存任务读取失败，可继续本地编辑', true); }
  };
  const closePatrolEditor = () => { setPatrolEditMode(false); patrolDialog.classList.remove('open'); };
  const addWaypoint = (pose) => {
    if (patrolDraft.waypoints.length >= 100) { showMapFeedback('航点最多100个', true); return; }
    patrolDraft.map_id = loadedMapId || patrolDraft.map_id || null;
    patrolDraft.waypoints.push({x: pose.x, y: pose.y, yaw: normalizeYaw(pose.yaw || 0)});
    selectPose(patrolDraft.waypoints.at(-1), 'waypoint', patrolDraft.waypoints.length - 1);
    renderPatrolEditorList(); updatePatrolSummary();
  };
  const savePatrolDraft = async () => {
    patrolDraft.name = patrolDialog.querySelector('[data-patrol-name]').value.trim();
    patrolDraft.repeat_count = Math.max(1, Math.min(99, Number(patrolDialog.querySelector('[data-patrol-repeat]').value) || 1));
    if (!loadedMapId || !patrolDraft.name || !patrolDraft.waypoints.length) { showMapFeedback('请先加载已保存地图，并填写任务名称和航点', true); return; }
    if (patrolDraft.map_id && patrolDraft.map_id !== loadedMapId) { showMapFeedback('该草稿属于另一张地图，不能跨地图覆盖', true); return; }
    const response = await fetch('/api/patrols/action', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({action: 'save', id: patrolDraft.id, name: patrolDraft.name, map_id: loadedMapId, repeat_count: patrolDraft.repeat_count, waypoints: patrolDraft.waypoints})});
    const payload = await response.json();
    if (!response.ok) throw new Error(payload.error || 'patrol_save_failed');
    patrolDraft = payload.item;
    savedPatrolSnapshot = draftSnapshot();
    await refreshSavedPatrols();
    updatePatrolSummary();
    showMapFeedback('巡逻任务已保存到地瓜派');
    announce('巡逻任务已保存到地瓜派', 'patrol-saved', 0);
  };

  root.querySelector('.side-patrol')?.classList.remove('disabled');
  root.querySelectorAll('[data-patrol-action="edit"],[data-patrol-action="save"],.repeat-stepper button,.repeat-stepper input').forEach((node) => { node.disabled = false; });
  root.querySelector('[data-patrol-action="edit"]')?.addEventListener('click', openPatrolEditor);
  root.querySelector('[data-task-open-patrol]')?.addEventListener('click', openPatrolEditor);
  root.querySelector('[data-patrol-action="save"]')?.addEventListener('click', async () => { await openPatrolEditor(); try { await savePatrolDraft(); } catch (error) { showMapFeedback(`保存失败：${error.message}`, true); } });
  root.querySelectorAll('[data-repeat-step]').forEach((button) => button.addEventListener('click', () => { patrolDraft.repeat_count = Math.max(1, Math.min(99, patrolDraft.repeat_count + Number(button.dataset.repeatStep))); updatePatrolSummary(); }));
  root.querySelector('.side-patrol [data-patrol-repeat]')?.addEventListener('input', (event) => { patrolDraft.repeat_count = Math.max(1, Math.min(99, Number(event.target.value) || 1)); updatePatrolSummary(); });
  patrolDialog.querySelector('[data-patrol-name]').addEventListener('input', (event) => { patrolDraft.name = event.target.value; updatePatrolSummary(); });
  patrolDialog.querySelector('[data-patrol-repeat]').addEventListener('input', (event) => { patrolDraft.repeat_count = Math.max(1, Math.min(99, Number(event.target.value) || 1)); updatePatrolSummary(); });
  patrolDialog.querySelector('[data-patrol-tool="map-add"]').addEventListener('click', () => setPatrolEditMode(!patrolEditMode));
  patrolDialog.querySelector('[data-patrol-tool="add-current"]').addEventListener('click', () => { if (targetPose) addWaypoint(targetPose); else showMapFeedback('请先在地图上选择目标点', true); });
  patrolDialog.querySelector('[data-patrol-tool="clear"]').addEventListener('click', () => { patrolDraft.waypoints = []; selectedWaypointIndex = -1; activePoseKind = 'goal'; renderPatrolMarkers(); renderPatrolEditorList(); renderGoalPose(); updatePatrolSummary(); });
  patrolDialog.querySelectorAll('[data-patrol-tool="close"],.patrol-editor-close').forEach((button) => button.addEventListener('click', closePatrolEditor));
  patrolDialog.querySelector('[data-patrol-tool="save"]').addEventListener('click', async () => { try { await savePatrolDraft(); } catch (error) { showMapFeedback(`保存失败：${error.message}`, true); } });
  savedSelect.addEventListener('change', async () => {
    const id = savedSelect.value;
    savedSelect.value = patrolDraft.id || '';
    if (!confirmReplacePatrol()) return;
    if (!id) { applyPatrol(freshPatrol()); return; }
    try {
      const items = await refreshSavedPatrols();
      const item = items.find((entry) => entry.id === id);
      if (!item) throw new Error('任务已被删除，请刷新列表');
      applyPatrol(item);
    } catch (error) { savedSelect.value = patrolDraft.id || ''; showMapFeedback(`读取失败：${error.message}`, true); }
  });
  patrolDialog.querySelector('[data-patrol-tool="new"]').addEventListener('click', () => { if (confirmReplacePatrol()) applyPatrol(freshPatrol()); });
  patrolDialog.querySelector('[data-patrol-tool="copy"]').addEventListener('click', () => {
    if (!patrolDraft.waypoints.length) { showMapFeedback('请先添加航点', true); return; }
    patrolDraft.id = null;
    patrolDraft.name = `${patrolDialog.querySelector('[data-patrol-name]').value.trim()} 副本`.slice(0, 80);
    patrolDialog.querySelector('[data-patrol-name]').value = patrolDraft.name;
    savedSelect.value = '';
    patrolDialog.querySelector('[data-patrol-tool="delete"]').disabled = true;
    savedPatrolSnapshot = null;
    updatePatrolSummary();
  });
  patrolDialog.querySelector('[data-patrol-tool="delete"]').addEventListener('click', async () => {
    const id = patrolDraft.id;
    if (!id || !window.confirm(`确定删除板端已保存的“${patrolDraft.name}”？此操作不可撤销。`)) return;
    try {
      const response = await fetch('/api/patrols/action', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({action: 'delete', id})});
      if (!response.ok) throw new Error((await response.json()).error || 'patrol_delete_failed');
      applyPatrol(freshPatrol());
      await refreshSavedPatrols();
      showMapFeedback('巡逻任务已从地瓜派删除');
    } catch (error) { showMapFeedback(`删除失败：${error.message}`, true); }
  });

  const runMotionTask = async (body) => {
    const response = await fetch('/api/tasks/action', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)});
    const payload = await response.json();
    if (!response.ok) throw new Error(payload.error || 'task_rejected');
    return payload;
  };
  root.querySelectorAll('[data-task-action]').forEach((button) => button.addEventListener('click', async () => {
    try {
      await runMotionTask({action: button.dataset.taskAction});
      await refreshTaskContext();
    } catch (_error) {
      showMapFeedback('实体底盘与Nav2尚未完成验收，任务控制保持安全锁定', true);
    }
  }));
  poseEditor.querySelector('[data-goal-navigate]').addEventListener('click', async () => {
    if (!targetPose || !loadedMapId) return;
    try { await runMotionTask({action: 'navigate', map_id: loadedMapId, ...targetPose}); }
    catch (_error) { showMapFeedback('实体底盘尚未验收，导航保持安全锁定', true); }
  });
  root.querySelector('[data-patrol-action="start"]')?.addEventListener('click', async () => {
    try { await runMotionTask({action: 'patrol_start', map_id: loadedMapId, repeat_count: patrolDraft.repeat_count, waypoints: patrolDraft.waypoints}); }
    catch (_error) { showMapFeedback('实体底盘尚未验收，巡逻保持安全锁定', true); }
  });

  const updateHeadingFromPointer = (event) => {
    const pose = activePose();
    const point = clientToMap(event);
    if (!pose || !point) return;
    pose.yaw = normalizeYaw(Math.atan2(point.y - pose.y, point.x - pose.x));
    renderGoalPose(); renderPatrolMarkers(); renderPatrolEditorList();
  };
  const clearHeadingDrag = () => {
    clearTimeout(headingDragTimer);
    headingDragTimer = null;
    headingDragPending = false;
    headingDragActive = false;
    headingDragPointerId = null;
    headingDragLastEvent = null;
    selectionArrowGroup?.classList.remove('dragging', 'hold-pending');
  };
  const finishHeadingDrag = (event, cancelled = false) => {
    if (event.pointerId !== headingDragPointerId) return;
    const wasActive = headingDragActive;
    if (wasActive && !cancelled) updateHeadingFromPointer(event);
    map.releasePointerCapture?.(event.pointerId);
    suppressMapClickUntil = performance.now() + 120;
    clearHeadingDrag();
    if (wasActive && !cancelled) showMapFeedback(`朝向已固定：${yawDegrees(activePose()?.yaw)}°`);
  };
  map.addEventListener('pointerdown', (event) => {
    if (event.button !== 0 || !event.target.closest('.live-goal-control')) return;
    event.preventDefault(); event.stopImmediatePropagation();
    clearHeadingDrag();
    headingDragPending = true;
    suppressMapClickUntil = performance.now() + 120;
    headingDragPointerId = event.pointerId;
    headingDragLastEvent = event;
    selectionArrowGroup?.classList.add('hold-pending');
    map.setPointerCapture?.(event.pointerId);
    headingDragTimer = setTimeout(() => {
      if (!headingDragPending || headingDragPointerId !== event.pointerId) return;
      headingDragPending = false;
      headingDragActive = true;
      selectionArrowGroup?.classList.remove('hold-pending');
      selectionArrowGroup?.classList.add('dragging');
      if (headingDragLastEvent) updateHeadingFromPointer(headingDragLastEvent);
    }, 180);
  }, true);
  map.addEventListener('pointermove', (event) => {
    if (event.pointerId !== headingDragPointerId) return;
    headingDragLastEvent = event;
    if (headingDragActive) updateHeadingFromPointer(event);
  }, true);
  map.addEventListener('pointerup', (event) => finishHeadingDrag(event), true);
  map.addEventListener('pointercancel', (event) => finishHeadingDrag(event, true), true);
  map.addEventListener('lostpointercapture', (event) => {
    if (event.pointerId === headingDragPointerId) clearHeadingDrag();
  }, true);
  occupancy.addEventListener('click', (event) => {
    const marker = event.target.closest('[data-waypoint-index]');
    if (!marker) return;
    const index = Number(marker.dataset.waypointIndex);
    selectPose(patrolDraft.waypoints[index], 'waypoint', index); renderPatrolEditorList();
    event.stopImmediatePropagation();
  }, true);

  map.addEventListener('click', (event) => {
    if (performance.now() < suppressMapClickUntil || headingDragActive || headingDragPending || event.target.closest('.layer-panel,.map-context,.map-head,.radar-minimize,.camera-view,.primary-view-switch,.goal-pose-editor,.live-goal-control')) return;
    const point = clientToMap(event);
    if (!point) return;
    event.stopImmediatePropagation();
    if (!isTraversable(point)) { showMapFeedback('该格为障碍或未知区域，不能设为目标/航点', true); return; }
    if (patrolEditMode) addWaypoint({...point, yaw: 0});
    else selectPose({...point, yaw: targetPose?.yaw || 0}, 'goal');
    if (mapReadout[1]) mapReadout[1].innerHTML = `坐标 <b>${point.x.toFixed(2)}, ${point.y.toFixed(2)}</b>`;
    if (mapReadout[2]) mapReadout[2].innerHTML = `格子 <b>${point.col}, ${point.row}</b>`;
    map.dataset.selectedMapX = point.x.toFixed(6);
    map.dataset.selectedMapY = point.y.toFixed(6);
    map.dataset.selectedMapCol = String(point.col);
    map.dataset.selectedMapRow = String(point.row);
    announce(`已选择地图坐标，横坐标${point.x.toFixed(2)}，纵坐标${point.y.toFixed(2)}`, 'map-point-selected', 700);
  }, true);

  const contextMenu = root.querySelector('.map-context');
  map.addEventListener('contextmenu', (event) => {
    if (event.target.closest('.layer-panel,.camera-view')) return;
    event.preventDefault(); event.stopImmediatePropagation();
    const point = clientToMap(event);
    if (!point) return;
    const traversable = isTraversable(point);
    if (traversable) selectPose({...point, yaw: targetPose?.yaw || 0}, 'goal');
    const rect = map.getBoundingClientRect();
    contextMenu.style.left = `${Math.max(8, Math.min(rect.width - 176, event.clientX - rect.left))}px`;
    contextMenu.style.top = `${Math.max(8, Math.min(rect.height - 110, event.clientY - rect.top))}px`;
    contextMenu.querySelector('.context-coordinate').textContent = `${point.x.toFixed(2)}, ${point.y.toFixed(2)}`;
    contextMenu.dataset.traversable = traversable ? '1' : '0';
    contextMenu.querySelectorAll('button').forEach((button) => { button.disabled = !traversable; });
    contextMenu.classList.add('open');
  }, true);
  contextMenu.addEventListener('click', (event) => {
    const button = event.target.closest('[data-map-action]');
    if (!button) return;
    event.preventDefault(); event.stopImmediatePropagation(); contextMenu.classList.remove('open');
    if (button.dataset.mapAction === 'mark') {
      if (patrolEditMode && targetPose) addWaypoint(targetPose);
      showMapFeedback(patrolEditMode ? '已添加巡逻航点' : '已标定目标点，可拖动箭头设置朝向');
    } else {
      activePoseKind = 'goal'; selectedWaypointIndex = -1; renderGoalPose();
      showMapFeedback(taskMotionEnabled ? '请确认目标方向后下发导航' : '目标已设置；实体底盘验收后才可下发导航');
    }
  }, true);
  refreshTaskContext();
  window.setInterval(() => { if (!document.hidden) refreshTaskContext(); }, 2000);
  updatePatrolSummary();

  let socket;
  let retryTimer;
  let watchdogTimer;
  let lastScanAt = 0;
  let connectedAt = 0;
  let radarDataOnline = false;
  let gatewayOnline = false;
  const markScanUnavailable = (message = '已连接网关，等待LD14合格帧…') => {
    setPill(2, 'LD14 等待数据', 'warning');
    stage.textContent = message;
    stage.className = 'live-stage-badge';
    setMetric(0, '雷达频率', '--', 'Hz', '等待 /scan');
    setMetric(1, '有效点数', '--', '点', '等待有效帧');
    setMetric(2, '最近障碍', '--', 'm', 'LD14量程下限0.15m');
    setMetric(3, '最远回波', '--', 'm', '当前帧有效回波');
    pointsGroup.innerHTML = '';
    radarHitPoints = [];
    hideRadarTooltip();
  };
  const armWatchdog = () => {
    clearInterval(watchdogTimer);
    watchdogTimer = setInterval(() => {
      const referenceTime = lastScanAt || connectedAt;
      if (socket?.readyState === WebSocket.OPEN && referenceTime && Date.now() - referenceTime > 1500) {
        lastScanAt = 0;
        if (radarDataOnline) announce('LD14雷达数据超时，正在等待恢复', 'radar-timeout', 0);
        radarDataOnline = false;
        markScanUnavailable('网关在线 · LD14数据超时，等待恢复…');
      }
    }, 500);
  };
  const connect = () => {
    clearTimeout(retryTimer);
    const url = `ws://${location.hostname}:8765`;
    socket = new WebSocket(url);
    socket.onopen = () => {
      connectedAt = Date.now();
      gatewayOnline = true;
      markScanUnavailable('网关已连接 · 等待LD14合格帧');
      announce('雷达网关已连接，正在等待LD14数据', 'gateway-connected', 0);
      armWatchdog();
    };
    socket.onmessage = (event) => {
      let data;
      try {
        data = JSON.parse(event.data);
      } catch (_error) {
        return;
      }
      if (data.type === 'map') {
        renderMap(data);
        return;
      }
      if (data.type === 'map_clear') {
        clearMap();
        return;
      }
      if (data.type === 'sim_layer') {
        renderSimLayer(data);
        return;
      }
      if (data.type !== 'scan') return;
      const radarWasOnline = radarDataOnline;
      radarDataOnline = true;
      lastScanAt = Date.now();
      pointsGroup.innerHTML = '';
      radarHitPoints = [];
      const maxVisualRange = 8;
      const radius = 245;
      data.points.forEach(([angle, range, intensity]) => {
        if (!Number.isFinite(range) || range < 0.15 || range > maxVisualRange) return;
        const r = Math.min(range / maxVisualRange, 1) * radius;
        const a = angle - Math.PI / 2;
        const point = document.createElementNS(ns, 'circle');
        point.setAttribute('class', 'live-radar-point');
        point.setAttribute('cx', 500 + Math.cos(a) * r);
        point.setAttribute('cy', 275 + Math.sin(a) * r);
        point.setAttribute('r', Math.max(1.2, Math.min(2.7, 1.2 + intensity / 255)));
        point.dataset.range = range.toFixed(3);
        point.dataset.angle = angle.toFixed(5);
        point.dataset.intensity = Number(intensity).toFixed(1);
        pointsGroup.appendChild(point);
        radarHitPoints.push({
          cx: 500 + Math.cos(a) * r,
          cy: 275 + Math.sin(a) * r,
          angle,
          range,
          intensity: Number(intensity) || 0,
        });
      });
      if (radarPointer.inside) {
        updateRadarHover(radarPointer.clientX, radarPointer.clientY, true);
      } else {
        hideRadarTooltip();
      }
      setPill(2, `LD14 · ${data.hz.toFixed(1)} Hz`, 'online');
      stage.textContent = `实时扫描 · ${data.valid_points}/${data.total_points}点 · 延迟 ${data.age_ms} ms`;
      stage.className = 'live-stage-badge online';
      setMetric(0, '雷达频率', data.hz.toFixed(1), 'Hz', 'ROS2 /scan 实时订阅');
      setMetric(1, '有效点数', data.valid_points, '点', `总点数 ${data.total_points}`);
      setMetric(2, '最近障碍', data.min_range.toFixed(2), 'm', '仅表示激光平面最近回波');
      setMetric(3, '最远回波', data.max_range.toFixed(2), 'm', '当前帧有效回波');
      if (panelMeta) panelMeta.textContent = `最后更新 ${new Date().toLocaleTimeString()} · 网关在线`;
      if (!radarWasOnline) announce('LD14雷达数据已恢复，实时扫描正常', 'radar-online', 0);
    };
    socket.onerror = () => socket.close();
    socket.onclose = () => {
      clearInterval(watchdogTimer);
      lastScanAt = 0;
      connectedAt = 0;
      const wasConnected = gatewayOnline;
      gatewayOnline = false;
      radarDataOnline = false;
      setPill(2, 'LD14 网关断开', 'warning');
      stage.textContent = '雷达网关断开，正在重连…';
      stage.className = 'live-stage-badge error';
      if (wasConnected) announce('雷达网关连接已断开，正在重连', 'gateway-disconnected', 0);
      retryTimer = setTimeout(connect, 2000);
    };
  };
  connect();

  root.addEventListener('click', (event) => {
    const button = event.target.closest('button');
    if (!button || button.disabled || button.matches('[data-voice-toggle],[data-power-action],[data-power-confirm],[data-power-cancel],[data-primary-view],.layer-toggle,.radar-minimize')) return;
    const label = button.textContent.trim() || button.getAttribute('aria-label');
    const voiceMessages = {
      '打开': '已选择打开定点巡逻',
      '暂停': '已请求暂停当前任务',
      '取消任务': '已请求取消当前任务',
      '编辑航点': '已打开航点编辑',
      '保存任务': '已请求保存巡逻任务',
      '开始巡逻': '已请求开始巡逻',
      '请求接管': '已请求人工接管',
      '标定此点': '已选择标定当前地图点',
      '导航到此点': '已请求导航到当前地图点',
      '前进': '人工控制，前进',
      '后退': '人工控制，后退',
      '左转': '人工控制，左转',
      '右转': '人工控制，右转',
      '停止': '人工控制，停止',
      '急停': '已触发车辆急停',
    };
    if (voiceMessages[label]) announce(voiceMessages[label], `operation-${label}`, label === '急停' ? 0 : 600);
  });
})();
