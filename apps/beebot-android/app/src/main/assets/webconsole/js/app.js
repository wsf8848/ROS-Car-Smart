/* =====================================================================
 * 阿克曼小车 Web 控制台 · 前端逻辑
 * 说明：本文件只依赖 rosbridge（WebSocket），不需要在浏览器装任何东西。
 * 想改话题名/端口，只改下面的 CONFIG 即可。
 * ===================================================================== */

var CONFIG = {
  // rosbridge 端口（rosbridge_websocket 默认 9090）。主机自动取当前网址的 IP。
  rosbridgePort: 9090,
  // web_video_server 端口（默认 8080），用于摄像头图传
  videoPort: 8080,
  // 后台一键功能服务端口（第二步的 Flask 后台，默认 5000）
  backendPort: 5000,

  // ---- 话题名（按你小车实际情况修改，可用 rostopic list 核对）----
  topic_cmd_vel: '/cmd_vel',            // 下发速度（geometry_msgs/Twist）
  topic_odom:    '/odom',               // 里程计
  topic_scan:    '/scan',               // 激光雷达
  topic_battery: '/PowerVoltage',       // 电量/电压（std_msgs/Float32，没有会显示 --）
  topic_image:   '/usb_cam/image_raw',  // 摄像头图像话题
  move_base:     '/move_base',          // 导航 action 服务名

  cmdRateHz: 10                          // 摇杆下发频率
};

// 掌机内 WebView 从 file:// 加载，没有 hostname；改从网址参数 ?host= 读取树莓派 IP。
// 例如 file:///android_asset/webconsole/index.html?host=192.168.0.100
function _qp(k){ try { return new URLSearchParams(window.location.search).get(k); } catch(e){ return null; } }
var host = _qp('host') || window.location.hostname || '192.168.0.100';
var ros, cmdVelPub, cmdTimer = null, curLin = 0, curAng = 0;
var maxLin = 0.5, maxAng = 0.8;

/* ---------------- 连接 rosbridge ---------------- */
function connect() {
  ros = new ROSLIB.Ros({ url: 'ws://' + host + ':' + CONFIG.rosbridgePort });

  ros.on('connection', function () { setConn(true); initAll(); });
  ros.on('error', function () { setConn(false); });
  ros.on('close', function () {
    setConn(false);
    setTimeout(connect, 2000); // 断线自动重连
  });
}

function setConn(ok) {
  var c = document.getElementById('conn');
  c.className = 'chip ' + (ok ? 'ok' : 'off');
  c.innerHTML = '<span class="dot"></span>' + (ok ? '已连接' : '未连接');
}

/* ---------------- 初始化所有功能 ---------------- */
function initAll() {
  // 注意：驱动小车改走「蓝牙直连」(见 initJoystick)，不再发布 /cmd_vel（避免树莓派重启）
  subOdom();
  subScan();
  subBattery();
  initMap();
  initCamera();
}

/* ---------------- 里程计：速度 / 位置 / 朝向 ---------------- */
function subOdom() {
  new ROSLIB.Topic({ ros: ros, name: CONFIG.topic_odom, messageType: 'nav_msgs/Odometry',
    throttle_rate: 100 }).subscribe(function (m) {
    var lin = m.twist.twist.linear.x, ang = m.twist.twist.angular.z;
    var x = m.pose.pose.position.x, y = m.pose.pose.position.y;
    var q = m.pose.pose.orientation;
    var yaw = Math.atan2(2*(q.w*q.z + q.x*q.y), 1 - 2*(q.y*q.y + q.z*q.z));
    set('m_lin', lin.toFixed(2));
    set('m_ang', ang.toFixed(2));
    set('m_xy', x.toFixed(1) + ' / ' + y.toFixed(1));
    set('m_yaw', Math.round(yaw * 180 / Math.PI));
  });
}

/* ---------------- 激光雷达：机器人视角雷达图 ---------------- */
function subScan() {
  var cv = document.getElementById('radar'), ctx = cv.getContext('2d');
  new ROSLIB.Topic({ ros: ros, name: CONFIG.topic_scan, messageType: 'sensor_msgs/LaserScan',
    throttle_rate: 150 }).subscribe(function (m) {
    var W = cv.width, H = cv.height, cx = W/2, cy = H/2;
    ctx.clearRect(0,0,W,H);
    // 网格圈
    ctx.strokeStyle = '#232a34';
    for (var r = 24; r < H; r += 24) { ctx.beginPath(); ctx.arc(cx, cy, r, 0, 6.283); ctx.stroke(); }
    // 有效点最大距离，动态缩放到画布
    var maxR = Math.min(m.range_max || 6, 6), scale = (Math.min(W,H)/2 - 6) / maxR;
    ctx.fillStyle = '#ef6b6b';
    for (var i = 0; i < m.ranges.length; i++) {
      var d = m.ranges[i];
      if (!d || d < m.range_min || d > maxR) continue;
      var a = m.angle_min + i * m.angle_increment;
      var px = cx + Math.cos(a) * d * scale;
      var py = cy - Math.sin(a) * d * scale;
      ctx.fillRect(px, py, 2, 2);
    }
    // 车体
    ctx.fillStyle = '#3b82f6';
    ctx.beginPath(); ctx.arc(cx, cy, 4, 0, 6.283); ctx.fill();
  });
}

/* ---------------- 电量 ---------------- */
function subBattery() {
  new ROSLIB.Topic({ ros: ros, name: CONFIG.topic_battery, messageType: 'std_msgs/Float32',
    throttle_rate: 1000 }).subscribe(function (m) {
    document.getElementById('batt').textContent = '电压 ' + Number(m.data).toFixed(1) + ' V';
  });
}

/* ---------------- 地图 + 一键导航（nav2djs） ---------------- */
function initMap() {
  try {
    var el = document.getElementById('map');
    var viewer = new ROS2D.Viewer({ divID: 'map', width: el.clientWidth, height: 360 });
    var tfClient = new ROSLIB.TFClient({ ros: ros, fixedFrame: 'map',
      angularThres: 0.01, transThres: 0.01, rate: 10 });
    NAV2D.OccupancyGridClientNav({
      ros: ros, rootObject: viewer.scene, viewer: viewer, tfClient: tfClient,
      topic: '/map', serverName: CONFIG.move_base,
      withOrientation: true, continuous: true
    });
    document.getElementById('mapinfo').textContent = '已加载';
  } catch (e) {
    document.getElementById('mapinfo').textContent = '（建图/导航启动后显示）';
  }
}

/* ---------------- 摄像头图传 ---------------- */
function initCamera() {
  var url = 'http://' + host + ':' + CONFIG.videoPort +
            '/stream?topic=' + CONFIG.topic_image + '&type=mjpeg';
  var img = document.getElementById('cam'), off = document.getElementById('camoff');
  img.onload = function () { img.style.display = 'block'; off.style.display = 'none'; };
  img.onerror = function () { img.style.display = 'none'; off.style.display = 'flex'; };
  img.src = url;
}

/* ---------------- 虚拟摇杆 → 蓝牙直连驱动小车（绕开树莓派，避免其重启） ---------------- */
function initJoystick() {
  var joy = nipplejs.create({
    zone: document.getElementById('joystick'),
    mode: 'static', position: { left: '50%', top: '50%' },
    color: '#3b82f6', size: 120
  });
  joy.on('move', function (evt, data) {
    var rad = data.angle.radian, dist = Math.min(data.distance, 60) / 60;
    var throttle = Math.sin(rad) * dist;   // 上=前进(+) / 下=后退(-)
    var steer = Math.cos(rad) * dist;      // 右=右转(+) / 左=左转(-)
    if (window.BtBridge && BtBridge.drive) BtBridge.drive(throttle, steer);
  });
  joy.on('end', function () { if (window.BtBridge && BtBridge.stop) BtBridge.stop(); });
  setInterval(pollNative, 400);
  pollNative();
}

// 轮询原生状态：蓝牙连接 + 全屏（决定摄像头是否铺满整屏）
function pollNative() {
  // 摄像头全屏：跟随原生「全屏」按钮状态
  var full = !!(window.BtBridge && BtBridge.fullscreen && BtBridge.fullscreen());
  document.body.classList.toggle('camfull', full);
  // 蓝牙连接状态
  var el = document.getElementById('btstat_v');
  if (!el) return;
  var ok = window.BtBridge && BtBridge.connected && BtBridge.connected();
  el.textContent = ok ? '已连接（可摇杆控车）' : '未连接（先到「阿克曼小车」页连蓝牙）';
  el.style.color = ok ? '#4ade80' : '#f59e0b';
}

/* ---------------- 急停（蓝牙停车回中） ---------------- */
function estop() {
  if (window.BtBridge && BtBridge.stop) BtBridge.stop();
  toast('已停车（蓝牙）');
}

/* ---------------- 摄像头全屏：native 点「全屏」时调用，画面铺满整屏、自适应 ---------------- */
window.setCamFull = function (on) {
  document.body.classList.toggle('camfull', !!on);
};

/* ---------------- 一键功能：调用第二步的后台 ---------------- */
function api(path, label) {
  var url = 'http://' + host + ':' + CONFIG.backendPort + '/api/' + path;
  toast(label + ' …');
  fetch(url, { method: 'POST' })
    .then(function (r) { return r.json(); })
    .then(function (d) { toast(label + '：' + (d.msg || '完成')); })
    .catch(function () { toast('后台脚本未部署（这是第二步的内容）'); });
}

function applyParams() { api('set_params?lin=' + maxLin + '&ang=' + maxAng, '应用参数'); }

/* ---------------- 参数滑条 ---------------- */
function bindParams() {
  bindRange('p_lin', 'p_lin_v', function (v) { maxLin = v; }, 2);
  bindRange('p_ang', 'p_ang_v', function (v) { maxAng = v; }, 2);
  bindRange('p_turn', 'p_turn_v', function () {}, 2);
}
function bindRange(id, out, cb, dp) {
  var el = document.getElementById(id);
  var upd = function () { document.getElementById(out).textContent = Number(el.value).toFixed(dp); cb(Number(el.value)); };
  el.addEventListener('input', upd); upd();
}

/* ---------------- 工具 ---------------- */
function set(id, v) { var e = document.getElementById(id); if (e) e.textContent = v; }
var toastTimer;
function toast(msg) {
  var t = document.getElementById('toast');
  t.textContent = msg; t.className = 'show';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { t.className = ''; }, 2500);
}

/* ---------------- 启动 ---------------- */
window.addEventListener('load', function () {
  bindParams();
  initJoystick();
  connect();
});
