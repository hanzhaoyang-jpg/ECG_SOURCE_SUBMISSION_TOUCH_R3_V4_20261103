/* ECG 工作站前端
 * - 演示模式：本地合成心电，用于离线演示与彩排
 * - 实时模式：接收本地服务推送的板卡 UDP 数据（v1 当前固件 / v2 方案协议）
 * 一旦收到真实数据就切换为实时，且不会自动退回演示，避免真假数据混在一起。
 */
(function () {
  'use strict';

  // ---------------------------------------------------------------- 常量
  var FS = 360;
  var NBUF = 40 * FS;
  var MM_ACROSS = 125;
  var REFRACTORY = Math.round(0.36 * FS);

  var $ = function (id) { return document.getElementById(id); };
  var clamp = function (v, a, b) { return Math.max(a, Math.min(b, v)); };
  var mod = function (n) { return ((n % NBUF) + NBUF) % NBUF; };

  // ---------------------------------------------------------------- 状态
  var S = {
    mode: 'demo',            // demo | live
    frozen: false,
    recording: false,
    recStart: 0,
    paper: 25,
    amp: 10,
    notch: true,
    gain: 960,
    lp: 40,
    hr: 72,
    rr: 833,
    sqi: 86,
    lastClass: 'N',
    lastConf: 92,
    latestClass: 255,
    latestFlags: 0,
    pkts: 0,
    lost: 0,
    lastPacketAt: 0,
    udpPort: 5005
  };

  var counts = { F: 0, N: 0, S: 0, V: 0 };
  var CLASS_NAME = { 0: 'F', 1: 'N', 2: 'S', 3: 'V' };
  var CLASS_TEXT = { F: ['融合波', '室性融合 · 需复核'], N: ['正常', '窦性心律 · 未见异常'], S: ['室上性', '提前出现 · 窄 QRS'], V: ['室性早搏', '宽 QRS · 提前出现'] };

  // ---------------------------------------------------------------- 缓冲
  var buf = new Float32Array(NBUF);
  buf.fill(NaN);
  var head = 0;          // 下一个待写 index
  var gaps = 0;
  var markers = [];      // {idx, cls, conf}
  var freezeAt = 0;

  function pushAt(idx, v) {
    if (idx <= head - NBUF) return;      // 太旧，丢弃
    if (idx > head) {
      var gap = Math.min(idx - head, NBUF);
      if (idx - head > 1) gaps++;
      for (var k = 0; k < gap; k++) buf[mod(head + k)] = NaN;
      head = idx;
    }
    buf[mod(idx)] = v;
    if (idx >= head) head = idx + 1;
  }

  function resetBuffer(at) {
    buf.fill(NaN);
    head = at;
    markers = [];
    gaps = 0;
  }

  function addMarker(idx, cls, conf) {
    markers.push({ idx: idx, cls: cls, conf: conf });
    if (markers.length > 400) markers.shift();
  }

  // ---------------------------------------------------------------- 演示信号
  var demoIdx = 0;
  var demoAcc = 0;
  var beats = [];
  var nextBeatT = 0.36;
  var rrIdx = 0;

  function g(d, sigma, amp) {
    return amp * Math.exp(-0.5 * (d / sigma) * (d / sigma));
  }

  function scheduleBeats(horizon) {
    while (nextBeatT < horizon) {
      var pvc = Math.random() < 0.05;
      var rr = 0.833 + 0.022 * Math.sin(rrIdx / 1.6) + 0.008 * Math.sin(rrIdx / 0.7);
      beats.push({ t: nextBeatT, cls: pvc ? 'V' : 'N', conf: pvc ? 88 + Math.random() * 9 : 90 + Math.random() * 8, fired: false });
      nextBeatT += pvc ? 0.80 : rr;
      rrIdx++;
    }
    if (beats.length > 80) beats = beats.filter(function (b) { return !b.fired; });
  }

  function ecgAt(t) {
    var v = 0.045 * Math.sin(2 * Math.PI * 0.05 * t + 0.7) + 0.012 * Math.sin(2 * Math.PI * 0.27 * t);
    for (var k = 0; k < beats.length; k++) {
      var b = beats[k];
      var d = t - b.t;
      if (d < -0.6 || d > 0.75) continue;
      if (b.cls === 'V') {
        v += g(d + 0.02, 0.032, 1.05) + g(d - 0.055, 0.038, -0.42) + g(d - 0.30, 0.075, -0.38);
      } else {
        v += g(d + 0.165, 0.028, 0.13) + g(d + 0.022, 0.011, -0.13) + g(d, 0.0095, 1.0) +
             g(d - 0.028, 0.014, -0.27) + g(d - 0.30, 0.055, 0.33);
      }
    }
    v += (Math.random() - 0.5) * 0.012;
    v += (S.notch ? 0.004 : 0.02) * Math.sin(2 * Math.PI * 50 * t);
    return v;
  }

  function stepDemo(dt) {
    scheduleBeats(demoIdx / FS + 2);
    demoAcc += dt * FS;
    var n = Math.floor(demoAcc);
    if (n <= 0) return;
    demoAcc -= n;
    if (n > 720) n = 720;             // 卡顿/切后台后不要瞬移
    for (var k = 0; k < n; k++) {
      var t = demoIdx / FS;
      pushAt(demoIdx, ecgAt(t));
      demoIdx++;
    }
    // 触发心搏标记
    for (var i = 0; i < beats.length; i++) {
      var b = beats[i];
      if (b.fired) continue;
      var bi = Math.round(b.t * FS);
      if (bi <= demoIdx) {
        b.fired = true;
        addMarker(bi, b.cls, b.conf);
        counts[b.cls] = (counts[b.cls] || 0) + 1;
        S.lastClass = b.cls;
        S.lastConf = b.conf;
      }
    }
  }

  // ---------------------------------------------------------------- 实时检测
  // 实时模式下 R 峰由前端做一次轻量峰值检测（固件暂不下发 R 峰时间）。
  var detPrev = 0, detPeak = 0.2, lastRIdx = -1e9;

  function detectBeat(idx, v) {
    var a = Math.abs(v);
    detPeak = Math.max(a, detPeak * 0.9993);
    var thr = Math.max(0.2, detPeak * 0.45);
    if (detPrev > thr && v <= detPrev && idx - lastRIdx > REFRACTORY) {
      lastRIdx = idx;
      var cls = CLASS_NAME[S.latestClass] || 'N';
      addMarker(idx, cls, null);
      counts[cls] = (counts[cls] || 0) + 1;
      var prev = markers[markers.length - 2];
      if (prev && prev.idx > 0) {
        var rr = (idx - prev.idx) * 1000 / FS;
        if (rr > 300 && rr < 2000) {
          S.rr = rr;
          S.hr = S.hr * 0.65 + (60000 / rr) * 0.35;
        }
      }
    }
    detPrev = v;
  }

  // ---------------------------------------------------------------- 实时数据
  function enterLive(at) {
    if (S.mode !== 'live') {
      S.mode = 'live';
      resetBuffer(at);
      setChip();
    }
  }

  function handleMessage(m) {
    if (m.t === 'hello') {
      S.udpPort = m.udp || 5005;
      $('aboutText').textContent = 'ECG 工作站 v' + (m.version || '1.1') + ' · UDP ' + S.udpPort;
      if (m.ips && m.ips.length) {
        $('localIpText').textContent = m.ips.join('  /  ') + '   ← 监听 UDP ' + S.udpPort;
        $('localIpText').style.color = 'var(--text)';
      } else {
        $('localIpText').textContent = '未检测到网卡地址（请连接以太网）';
      }
      return;
    }
    if (m.t === 'snap') {
      enterLive(m.i0);
      applySamples(m.i0, m.v);
      return;
    }
    if (m.t === 'w') {
      enterLive(m.i0);
      applySamples(m.i0, m.v);
      if (typeof m.c === 'number') S.latestClass = m.c;
      if (typeof m.f === 'number') S.latestFlags = m.f;
      return;
    }
    if (m.t === 'st') {
      S.pkts = m.pkts || 0;
      S.lost = m.lost || 0;
      S.live = !!m.live;
      $('pktValue').textContent = String(S.pkts);
      var pct = S.pkts ? (S.lost / (S.pkts + S.lost) * 100) : 0;
      $('lossValue').textContent = pct.toFixed(2);
      return;
    }
  }

  var liveApplied = false;
  function applySamples(i0, v) {
    if (!v || !v.length) return;
    for (var k = 0; k < v.length; k++) {
      var mv = v[k] / 1000;
      pushAt(i0 + k, mv);
      S.lastPacketAt = Date.now();
    }
    if (!liveApplied) { liveApplied = true; }
  }

  function connectStream() {
    if (typeof EventSource === 'undefined') return;
    var es = new EventSource('/api/stream');
    es.onmessage = function (e) {
      try { handleMessage(JSON.parse(e.data)); } catch (err) { /* ignore */ }
    };
    es.onerror = function () {
      // EventSource 会自动重连，这里只更新提示
      if (S.mode === 'live') setPill('warn', '重连中…');
    };
  }

  // ---------------------------------------------------------------- 画布
  function fit(canvas) {
    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    var rect = canvas.getBoundingClientRect();
    var w = Math.max(1, Math.round(rect.width));
    var h = Math.max(1, Math.round(rect.height));
    if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
      canvas.width = w * dpr;
      canvas.height = h * dpr;
    }
    var ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return { ctx: ctx, w: w, h: h };
  }

  var gridCache = { key: '', canvas: null };

  function drawGrid(w, h, pxPerMm) {
    var key = w + 'x' + h + ':' + pxPerMm.toFixed(2);
    if (gridCache.key === key) return gridCache.canvas;

    var dpr = Math.min(window.devicePixelRatio || 1, 2);
    var c = document.createElement('canvas');
    c.width = w * dpr; c.height = h * dpr;
    var g2 = c.getContext('2d');
    g2.setTransform(dpr, 0, 0, dpr, 0, 0);
    g2.fillStyle = '#0c1422';
    g2.fillRect(0, 0, w, h);

    var i, x, y;
    g2.lineWidth = 1;
    g2.strokeStyle = 'rgba(120,170,255,0.055)';
    g2.beginPath();
    for (i = 0, x = 0; x <= w; i++, x = i * pxPerMm) {
      if (i % 5 === 0) continue;
      g2.moveTo(Math.round(x) + 0.5, 0); g2.lineTo(Math.round(x) + 0.5, h);
    }
    for (i = 0, y = 0; y <= h; i++, y = i * pxPerMm) {
      if (i % 5 === 0) continue;
      g2.moveTo(0, Math.round(y) + 0.5); g2.lineTo(w, Math.round(y) + 0.5);
    }
    g2.stroke();

    g2.strokeStyle = 'rgba(120,170,255,0.13)';
    g2.beginPath();
    for (i = 0, x = 0; x <= w; i++, x = i * pxPerMm) {
      if (i % 5 !== 0) continue;
      g2.moveTo(Math.round(x) + 0.5, 0); g2.lineTo(Math.round(x) + 0.5, h);
    }
    for (i = 0, y = 0; y <= h; i++, y = i * pxPerMm) {
      if (i % 5 !== 0) continue;
      g2.moveTo(0, Math.round(y) + 0.5); g2.lineTo(w, Math.round(y) + 0.5);
    }
    g2.stroke();

    gridCache.key = key;
    gridCache.canvas = c;
    return c;
  }

  function windowSec() { return MM_ACROSS / S.paper; }

  var waveCanvas = $('waveCanvas');
  var eventCanvas = $('eventCanvas');
  var trendCanvas = $('trendCanvas');

  function drawWave() {
    var f = fit(waveCanvas);
    var ctx = f.ctx, w = f.w, h = f.h;
    var pxPerMm = w / MM_ACROSS;
    var win = windowSec();
    var pxPerSec = w / win;
    var pxPerMv = pxPerMm * S.amp;

    ctx.clearRect(0, 0, w, h);
    ctx.drawImage(drawGrid(w, h, pxPerMm), 0, 0, w, h);

    var mid = h / 2;
    var end = S.frozen ? freezeAt : head - 1;
    var start = end - win * FS;

    // 基线
    ctx.strokeStyle = 'rgba(56,189,248,0.16)';
    ctx.setLineDash([4, 8]);
    ctx.beginPath(); ctx.moveTo(0, mid + 0.5); ctx.lineTo(w, mid + 0.5); ctx.stroke();
    ctx.setLineDash([]);

    var path = new Path2D();
    var open = false;
    var lastX = -1e9;
    for (var idx = Math.ceil(start); idx <= end; idx++) {
      var v = buf[mod(idx)];
      if (Number.isNaN(v)) { open = false; continue; }
      var x = w - (end - idx) * pxPerSec / FS;
      if (x < -4) continue;
      var y = mid - v * pxPerMv;
      if (!open) { path.moveTo(x, y); open = true; } else { path.lineTo(x, y); }
      lastX = x;
    }
    if (lastX > -1e9) {
      ctx.lineJoin = 'round';
      ctx.lineCap = 'round';
      ctx.strokeStyle = 'rgba(62,224,143,0.18)';
      ctx.lineWidth = 4;
      ctx.stroke(path);
      ctx.strokeStyle = '#3ee08f';
      ctx.lineWidth = 1.8;
      ctx.stroke(path);
    }

    // R 峰标注
    ctx.font = '600 10px -apple-system,"PingFang SC",sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    for (var i = 0; i < markers.length; i++) {
      var m = markers[i];
      if (m.idx < start - 2 || m.idx > end + 2) continue;
      var mx = w - (end - m.idx) * pxPerSec / FS;
      var col = m.cls === 'V' ? '#f87171' : '#34d399';

      ctx.strokeStyle = 'rgba(52,211,153,0.16)';
      ctx.setLineDash([2, 6]);
      ctx.beginPath(); ctx.moveTo(mx, 12); ctx.lineTo(mx, h - 24); ctx.stroke();
      ctx.setLineDash([]);

      ctx.fillStyle = col;
      ctx.beginPath();
      ctx.moveTo(mx, 10); ctx.lineTo(mx - 4.5, 3); ctx.lineTo(mx + 4.5, 3);
      ctx.closePath(); ctx.fill();

      var cw = m.cls === 'V' ? 22 : 20;
      ctx.fillStyle = m.cls === 'V' ? 'rgba(248,113,113,0.18)' : 'rgba(52,211,153,0.16)';
      roundRect(ctx, mx - cw / 2, h - 26, cw, 16, 4);
      ctx.fill();
      ctx.fillStyle = col;
      ctx.fillText(m.cls, mx, h - 18);
    }

    ctx.textAlign = 'right';
    ctx.textBaseline = 'alphabetic';
    ctx.fillStyle = 'rgba(99,119,146,0.9)';
    ctx.font = '10px -apple-system,"PingFang SC",sans-serif';
    ctx.fillText(win.toFixed(1).replace('.0', '') + ' s', w - 8, h - 8);
  }

  function roundRect(ctx, x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
  }

  function drawEvent() {
    var f = fit(eventCanvas);
    var ctx = f.ctx, w = f.w, h = f.h;
    ctx.clearRect(0, 0, w, h);

    var end = S.frozen ? freezeAt : head - 1;
    var span = 60 * FS;
    var x0 = 20, x1 = w - 20;
    var ty = h * 0.52;

    var wx = x1 - (windowSec() * FS / span) * (x1 - x0);
    ctx.fillStyle = 'rgba(56,189,248,0.07)';
    ctx.fillRect(wx, ty - 18, x1 - wx, 36);
    ctx.strokeStyle = 'rgba(56,189,248,0.35)';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(wx + 0.5, ty - 18); ctx.lineTo(wx + 0.5, ty + 18); ctx.stroke();

    ctx.strokeStyle = 'rgba(32,46,69,1)';
    ctx.beginPath(); ctx.moveTo(x0, ty + 0.5); ctx.lineTo(x1, ty + 0.5); ctx.stroke();

    for (var i = 0; i < markers.length; i++) {
      var m = markers[i];
      if (m.idx < end - span || m.idx > end) continue;
      var x = x0 + (m.idx - (end - span)) / span * (x1 - x0);
      if (m.cls === 'V') {
        ctx.strokeStyle = '#f87171';
        ctx.lineWidth = 2.5;
        ctx.beginPath(); ctx.moveTo(x, ty - 20); ctx.lineTo(x, ty + 20); ctx.stroke();
        ctx.fillStyle = '#f87171';
        ctx.beginPath(); ctx.arc(x, ty - 24, 3, 0, Math.PI * 2); ctx.fill();
      } else {
        ctx.strokeStyle = 'rgba(52,211,153,0.5)';
        ctx.lineWidth = 1.2;
        ctx.beginPath(); ctx.moveTo(x, ty - 7); ctx.lineTo(x, ty + 7); ctx.stroke();
      }
    }

    ctx.fillStyle = 'rgba(99,119,146,0.9)';
    ctx.font = '10px ui-monospace,Menlo,monospace';
    ctx.textBaseline = 'top';
    var labels = [[x0, '−60 s', 'left'], [x0 + (x1 - x0) / 3, '−40 s', 'center'],
                  [x0 + 2 * (x1 - x0) / 3, '−20 s', 'center'], [x1, '现在', 'right']];
    for (var j = 0; j < labels.length; j++) {
      ctx.textAlign = labels[j][2];
      ctx.fillText(labels[j][1], labels[j][0], ty + 16);
    }
  }

  var trend = [];   // HR 历史，1 点/秒

  function drawTrend() {
    var f = fit(trendCanvas);
    var ctx = f.ctx, w = f.w, h = f.h;
    ctx.clearRect(0, 0, w, h);

    var x0 = 20, x1 = w - 20, y0 = 10, y1 = h - 10;
    var yFor = function (v) { return y1 - (v - 55) / 30 * (y1 - y0); };

    var levels = [60, 70, 80];
    ctx.font = '9px ui-monospace,Menlo,monospace';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    for (var i = 0; i < levels.length; i++) {
      var y = yFor(levels[i]);
      ctx.strokeStyle = 'rgba(140,162,189,0.10)';
      ctx.beginPath(); ctx.moveTo(x0, y + 0.5); ctx.lineTo(x1, y + 0.5); ctx.stroke();
      ctx.fillStyle = 'rgba(99,119,146,0.75)';
      ctx.fillText(String(levels[i]), 2, y);
    }

    if (trend.length < 2) return;

    var pts = [];
    for (var k = 0; k < trend.length; k++) {
      pts.push([x0 + k / (trend.length - 1) * (x1 - x0), yFor(trend[k])]);
    }

    ctx.beginPath();
    ctx.moveTo(pts[0][0], y1);
    for (var a = 0; a < pts.length; a++) ctx.lineTo(pts[a][0], pts[a][1]);
    ctx.lineTo(pts[pts.length - 1][0], y1);
    ctx.closePath();
    ctx.fillStyle = 'rgba(56,189,248,0.10)';
    ctx.fill();

    ctx.beginPath();
    ctx.moveTo(pts[0][0], pts[0][1]);
    for (var b2 = 1; b2 < pts.length; b2++) ctx.lineTo(pts[b2][0], pts[b2][1]);
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 1.6;
    ctx.lineJoin = 'round';
    ctx.stroke();
  }

  // ---------------------------------------------------------------- 面板
  var lastPanel = 0;

  function updatePanels(now) {
    if (now - lastPanel < 120) return;
    lastPanel = now;

    var live = S.mode === 'live';
    var hr = Math.round(S.hr);
    $('hrValue').textContent = String(hr);
    $('rrValue').textContent = String(Math.round(S.rr));

    var cls = live ? (CLASS_NAME[S.latestClass] || null) : S.lastClass;
    var box = $('classBox');
    if (!cls) {
      box.textContent = '—';
      box.className = 'class-box unknown';
      $('className').textContent = '等待数据';
      $('classDesc').textContent = '板卡未给出分类结果';
      $('confValue').textContent = '—';
    } else {
      box.textContent = cls;
      box.className = 'class-box' + (cls === 'V' ? ' v' : '');
      $('className').textContent = CLASS_TEXT[cls][0];
      $('classDesc').textContent = CLASS_TEXT[cls][1];
      $('classDesc').style.color = cls === 'V' ? 'var(--red)' : '';
      if (live) {
        $('confValue').textContent = '—';
        $('confValue').style.color = 'var(--dim)';
      } else {
        $('confValue').textContent = Math.round(S.lastConf) + ' %';
        $('confValue').style.color = cls === 'V' ? 'var(--red)' : 'var(--green)';
      }
    }

    $('nCount').textContent = String(counts.N);
    $('vCount').textContent = String(counts.V);
    $('cntV').className = counts.V > 0 ? 'v on' : 'v';

    if (live) {
      $('sqiValue').textContent = '—';
      $('sqiLabel').textContent = '待固件';
      $('sqiLabel').style.color = 'var(--dim)';
      $('sqiBar').style.width = '0%';
      $('sqiTag').textContent = 'v1 无字段';
      $('sqiChips').innerHTML = '<span>需要 v2 STATUS 帧</span><span>当前仅收波形</span>';
    } else {
      var sqi = Math.round(S.sqi);
      $('sqiValue').textContent = String(sqi);
      $('sqiBar').style.width = sqi + '%';
      var good = sqi >= 80, mid2 = sqi >= 60;
      $('sqiLabel').textContent = good ? '良好' : (mid2 ? '一般' : '较差');
      $('sqiLabel').style.color = good ? 'var(--green)' : (mid2 ? 'var(--amber)' : 'var(--red)');
      $('sqiBar').style.background = good ? 'var(--green)' : (mid2 ? 'var(--amber)' : 'var(--red)');
      $('sqiTag').textContent = '2 s 窗口';
      $('sqiChips').innerHTML = good
        ? '<span>余量充足</span><span>无削顶</span><span>无脱落</span>'
        : '<span style="color:var(--amber)">余量偏低</span><span>建议降低增益</span>';
    }

    if (live) {
      var lod = (S.latestFlags & 1) !== 0;
      var otr = (S.latestFlags & 2) !== 0;
      $('devGain').textContent = '—';
      $('devFilter').textContent = '—';
      $('devNotch').textContent = '—';
      setStatusCell($('devLod'), lod ? '脱落' : '正常', !lod);
      setStatusCell($('devOtr'), otr ? '过载' : '正常', !otr);
      $('devSd').textContent = '—';
    } else {
      $('devGain').textContent = S.gain + ' ×';
      $('devFilter').textContent = '0.05 / ' + S.lp + ' Hz';
      $('devNotch').textContent = S.notch ? '开' : '关';
      setStatusCell($('devLod'), '正常', true);
      setStatusCell($('devOtr'), '正常', true);
      $('devSd').textContent = '12.4 GB 可用';
    }

    $('eventMeta').textContent = '异常 ' + counts.V + ' 次';

    var arr = trend.slice(-300);
    if (arr.length) {
      var mn = Math.min.apply(null, arr), mx = Math.max.apply(null, arr);
      var avg = Math.round(arr.reduce(function (a, b) { return a + b; }, 0) / arr.length);
      $('trendMeta').textContent = '平均 ' + avg + ' bpm · 最低 ' + Math.round(mn) + ' · 最高 ' + Math.round(mx);
    }

    $('rhythmHint').textContent = counts.V > 0 ? '偶发室早' : '窦性 · 规整';
    $('rhythmHint').style.color = counts.V > 0 ? 'var(--amber)' : 'var(--green)';

    updatePill();
  }

  function setStatusCell(el, text, ok) {
    el.textContent = text;
    el.className = ok ? 'ok' : 'bad';
  }

  var pillState = '';
  function setPill(kind, text) {
    var el = $('connPill');
    el.className = 'pill ' + ({ ok: 'pill--ok', warn: 'pill--amber', red: 'pill--red', cyan: 'pill--cyan' }[kind] || 'pill--amber');
    $('connText').textContent = text;
    pillState = kind + '|' + text;
  }

  function updatePill() {
    var now = Date.now();
    var stale = now - S.lastPacketAt;
    if (S.mode === 'demo') {
      if (pillState !== 'warn|演示数据') setPill('warn', '演示数据');
      return;
    }
    if (stale > 5000) {
      if (pillState !== 'red|实时 · 断流') setPill('red', '实时 · 断流');
    } else if (stale > 1500) {
      if (pillState !== 'warn|实时 · 缓冲') setPill('warn', '实时 · 缓冲');
    } else {
      if (pillState !== 'ok|实时 · UDP') setPill('ok', '实时 · UDP');
    }
  }

  function setChip() {
    var chip = $('liveChip');
    chip.textContent = S.mode === 'live' ? '实时' : '演示';
    chip.className = 'chip' + (S.mode === 'live' ? '' : ' chip--warn');
  }

  function updateClock() {
    var d = new Date();
    var pad = function (v) { return String(v).padStart(2, '0'); };
    $('clock').textContent = pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
    $('date').textContent = d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
    if (S.recording) {
      var sec = Math.floor((Date.now() - S.recStart) / 1000);
      $('recText').textContent = 'REC · ' + pad(Math.floor(sec / 60)) + ':' + pad(sec % 60);
    }
  }

  // ---------------------------------------------------------------- 主循环
  var lastReal = performance.now();

  function loop(now) {
    var dt = Math.min((now - lastReal) / 1000, 0.25);
    lastReal = now;

    if (S.mode === 'demo' && !S.frozen) {
      stepDemo(dt);
    }

    drawWave();
    drawEvent();
    drawTrend();
    updatePanels(now);
    requestAnimationFrame(loop);
  }

  setInterval(function () {
    updateClock();
    if (S.mode === 'demo' && !S.frozen) {
      trend.push(S.hr);
      if (trend.length > 300) trend.shift();
      S.sqi += (86 - S.sqi) * 0.25 + (Math.random() - 0.5) * 3;
      S.sqi = clamp(S.sqi, 46, 98);
    }
    if (S.mode === 'live' && !S.frozen && S.lastPacketAt && Date.now() - S.lastPacketAt < 3000) {
      trend.push(S.hr);
      if (trend.length > 300) trend.shift();
    }
  }, 1000);

  // ---------------------------------------------------------------- 交互
  var toastTimer = 0;
  function toast(msg) {
    var el = $('toast');
    el.textContent = msg;
    el.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { el.classList.remove('show'); }, 3200);
  }

  function setFreeze(on) {
    S.frozen = on;
    if (on) freezeAt = head - 1;
    $('frozenBadge').classList.toggle('show', on);
    $('btnFreeze').querySelector('span').textContent = on ? '继续' : '冻结';
  }

  $('btnFreeze').addEventListener('click', function () {
    setFreeze(!S.frozen);
    toast(S.frozen ? '已冻结画面（数据仍在接收）' : '已恢复实时显示');
  });

  $('btnRecord').addEventListener('click', function () {
    if (S.mode === 'live') {
      var path = S.recording ? '/api/record/stop' : '/api/record/start';
      fetch(path, { method: 'POST' })
        .then(function (r) { return r.json(); })
        .then(function (j) {
          if (!j.ok) { toast('记录失败：' + (j.error || '未知错误')); return; }
          S.recording = !S.recording;
          if (S.recording) S.recStart = Date.now();
          syncRecordButton();
          toast(S.recording ? '已开始记录 → ' + shortPath(j.file) : '已停止，保存 ' + j.lines + ' 行 → ' + shortPath(j.file));
        })
        .catch(function () { toast('记录接口不可用'); });
      return;
    }
    // 演示模式：只演示交互，不落盘
    S.recording = !S.recording;
    if (S.recording) S.recStart = Date.now();
    syncRecordButton();
    toast(S.recording ? '演示模式：记录状态已开始（不落盘）' : '演示模式：记录已停止');
  });

  function syncRecordButton() {
    var btn = $('btnRecord');
    btn.classList.toggle('btn--rec', S.recording);
    btn.querySelector('span').textContent = S.recording ? '停止记录' : '开始记录';
    $('recBadge').classList.toggle('show', S.recording);
  }

  function shortPath(p) {
    if (!p) return '';
    var parts = p.split(/[\\/]/);
    return parts.slice(-2).join('/');
  }

  $('btnSnapshot').addEventListener('click', function () {
    var src = $('waveCanvas');
    var c = document.createElement('canvas');
    c.width = src.width; c.height = src.height;
    var g2 = c.getContext('2d');
    g2.fillStyle = '#0c1422'; g2.fillRect(0, 0, c.width, c.height);
    g2.drawImage(src, 0, 0);
    c.toBlob(function (blob) {
      var a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'ECG_snapshot_' + Date.now() + '.png';
      a.click();
      setTimeout(function () { URL.revokeObjectURL(a.href); }, 4000);
    });
    toast('已保存波形快照（PNG）');
  });

  $('btnReport').addEventListener('click', function () {
    toast('报告草稿：心率 ' + Math.round(S.hr) + ' bpm，RR ' + Math.round(S.rr) +
          ' ms，室早 ' + counts.V + ' 次（接入 tjbg_report 后可导出 DOCX）');
  });

  $('btnSettings').addEventListener('click', function () {
    $('drawer').classList.add('open');
    $('scrim').classList.add('show');
    $('drawer').setAttribute('aria-hidden', 'false');
  });

  function closeDrawer() {
    $('drawer').classList.remove('open');
    $('scrim').classList.remove('show');
    $('drawer').setAttribute('aria-hidden', 'true');
  }
  $('btnCloseDrawer').addEventListener('click', closeDrawer);
  $('scrim').addEventListener('click', closeDrawer);

  $('btnQuit').addEventListener('click', function () {
    fetch('/api/quit', { method: 'POST' }).catch(function () {});
    document.body.innerHTML = '<div style="display:grid;place-items:center;height:100vh;color:#8ca2bd;font:15px -apple-system,sans-serif">工作站已退出，可以关闭窗口。</div>';
  });

  Array.prototype.forEach.call(document.querySelectorAll('.seg[data-key]'), function (seg) {
    seg.addEventListener('click', function (e) {
      var btn = e.target.closest('button[data-v]');
      if (!btn) return;
      Array.prototype.forEach.call(seg.querySelectorAll('button'), function (b) { b.classList.remove('on'); });
      btn.classList.add('on');
      var key = seg.getAttribute('data-key');
      var val = btn.getAttribute('data-v');

      if (key === 'paper') S.paper = parseFloat(val);
      if (key === 'amp') S.amp = parseFloat(val);
      if (key === 'gain') {
        S.gain = parseInt(val, 10);
        toast(S.mode === 'live' ? '增益待固件 CMD_SET_AFE 支持' : '演示：前端增益设为 ' + S.gain + '×');
      }
      if (key === 'lp') {
        S.lp = parseInt(val, 10);
        toast(S.mode === 'live' ? '低通待固件 CMD_SET_AFE 支持' : '演示：低通设为 ' + S.lp + ' Hz');
      }
      if (key === 'notch') {
        S.notch = val === '1';
        toast('50 Hz 陷波已' + (S.notch ? '开启' : '关闭') + (S.mode === 'live' ? '（仅影响显示）' : ''));
      }
      updateWaveMeta();
    });
  });

  function updateWaveMeta() {
    $('waveMeta').textContent = [
      S.paper + ' mm/s',
      S.amp + ' mm/mV',
      '0.05–' + S.lp + ' Hz',
      S.notch ? '50 Hz 陷波' : '陷波关'
    ].join(' · ');
  }
  updateWaveMeta();

  $('btnInject').addEventListener('click', function () {
    if (S.mode === 'live') { toast('实时模式下不注入模拟事件'); return; }
    var last = beats.length ? beats[beats.length - 1].t : demoIdx / FS;
    var t = Math.max(demoIdx / FS + 0.6, last + 0.7);
    beats.push({ t: t, cls: 'V', conf: 93, fired: false });
    nextBeatT = Math.max(nextBeatT, t + 0.9);
    toast('已注入一次室早，观察时间轴与计数');
    closeDrawer();
  });

  document.addEventListener('keydown', function (e) {
    if (e.target && /input|select|textarea/i.test(e.target.tagName)) return;
    if (e.code === 'Space') { e.preventDefault(); setFreeze(!S.frozen); }
    if (e.key === 'r' || e.key === 'R') $('btnRecord').click();
    if (e.key === 's' || e.key === 'S') $('btnSnapshot').click();
    if (e.key === 'Escape') closeDrawer();
  });

  window.addEventListener('resize', function () { gridCache.key = ''; });

  // ---------------------------------------------------------------- 心跳
  function heartbeat() {
    fetch('/api/hb', { method: 'POST' }).catch(function () {});
  }
  setInterval(heartbeat, 5000);
  document.addEventListener('visibilitychange', function () {
    if (!document.hidden) heartbeat();
  });

  // ---------------------------------------------------------------- 启动
  if (location.port) $('portValue').textContent = location.port;
  setChip();
  updateClock();
  for (var p = 0; p < 12; p++) stepDemo(1);   // 预填充 12 s，打开即为满屏波形
  for (var i = 0; i < 300; i++) trend.push(71 + 3.2 * Math.sin(i / 26) + 1.4 * Math.sin(i / 7));
  connectStream();
  requestAnimationFrame(loop);

  window.ECG = { S: S, get head() { return head; }, get gaps() { return gaps; } };
})();
