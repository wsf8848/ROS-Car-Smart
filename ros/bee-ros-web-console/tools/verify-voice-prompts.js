const { chromium } = require('playwright');
const path = require('path');

(async () => {
  const outputDir = process.argv[2];
  if (!outputDir) throw new Error('Missing screenshot output directory.');
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.BEE_EDGE_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  });
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  const consoleErrors = [];
  page.on('console', (message) => {
    if (message.type() === 'error') consoleErrors.push(message.text());
  });
  page.on('pageerror', (error) => consoleErrors.push(error.message));

  await page.addInitScript(() => {
    window.__beeSpoken = [];
    window.__beeSpeechCancelled = 0;
    class FakeUtterance {
      constructor(text) { this.text = text; }
    }
    Object.defineProperty(window, 'SpeechSynthesisUtterance', { configurable: true, value: FakeUtterance });
    Object.defineProperty(window, 'speechSynthesis', {
      configurable: true,
      value: {
        getVoices: () => [{ lang: 'zh-CN', name: 'Microsoft Xiaoxiao' }],
        speak: (utterance) => window.__beeSpoken.push(utterance.text),
        cancel: () => { window.__beeSpeechCancelled += 1; },
      },
    });
    class FakeWebSocket {
      static OPEN = 1;
      constructor() {
        this.readyState = FakeWebSocket.OPEN;
        window.__beeWs = this;
        setTimeout(() => this.onopen?.(), 20);
      }
      emit(payload) { this.onmessage?.({ data: JSON.stringify(payload) }); }
      close() { this.readyState = 3; this.onclose?.(); }
    }
    Object.defineProperty(window, 'WebSocket', { configurable: true, value: FakeWebSocket });
    localStorage.setItem('beeRosVoiceEnabled', '0');
  });

  await page.goto(process.env.BEE_CONSOLE_URL || 'http://127.0.0.1:18080/', { waitUntil: 'networkidle' });
  const toggle = page.locator('[data-voice-toggle]');
  if ((await toggle.innerText()).trim() !== '语音关闭') throw new Error('Voice control did not default to off.');
  if ((await toggle.getAttribute('aria-pressed')) !== 'false') throw new Error('Voice control aria state is not off.');

  await toggle.click();
  if ((await toggle.innerText()).trim() !== '语音开启') throw new Error('Voice control did not switch on.');

  await page.evaluate(() => {
    window.__beeWs.emit({
      type: 'map', frame_id: 'bee_test_map', is_test: true, width: 4, height: 3,
      resolution: 0.2, origin: { x: 0, y: 0, yaw: 0 }, counts: { free: 8, occupied: 2, unknown: 2 },
      data: [100, 100, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0],
    });
    for (let index = 0; index < 20; index += 1) {
      window.__beeWs.emit({
        type: 'scan', hz: 6, valid_points: 2, total_points: 2, age_ms: 20,
        min_range: 0.8, max_range: 1.2, points: [[0, 0.8, 120], [1, 1.2, 160]],
      });
    }
  });
  await page.locator('.layer-toggle[data-layer="scan"]').click();
  await page.locator('.radar-minimize').click();

  const spokenBeforeOff = await page.evaluate(() => [...window.__beeSpoken]);
  const radarOnlineCount = spokenBeforeOff.filter((text) => text.includes('LD14雷达数据已恢复')).length;
  if (radarOnlineCount !== 1) throw new Error(`Radar online prompt repeated ${radarOnlineCount} times.`);
  const required = ['语音提示已开启', '测试地图已接收', 'LD14雷达数据已恢复', '已切换到LD14雷达主画面', '已切换到栅格地图主画面'];
  required.forEach((phrase) => {
    if (!spokenBeforeOff.some((text) => text.includes(phrase))) throw new Error(`Missing voice prompt: ${phrase}`);
  });

  await toggle.click();
  const countAfterOff = await page.evaluate(() => window.__beeSpoken.length);
  await page.locator('.layer-toggle[data-layer="scan"]').click();
  const countAfterSilentAction = await page.evaluate(() => window.__beeSpoken.length);
  if (countAfterSilentAction !== countAfterOff) throw new Error('A prompt played after voice was switched off.');
  if ((await toggle.innerText()).trim() !== '语音关闭') throw new Error('Voice control did not switch off.');

  await page.screenshot({ path: path.join(outputDir, '20260926-BeeROSWebConsole-Windows语音提示开关-1440x1000.png'), fullPage: true });
  await page.setViewportSize({ width: 1024, height: 900 });
  const overflow1024 = await page.evaluate(() => document.documentElement.scrollWidth > document.documentElement.clientWidth);
  const result = {
    spokenBeforeOff,
    countAfterOff,
    countAfterSilentAction,
    speechCancelCalls: await page.evaluate(() => window.__beeSpeechCancelled),
    storedPreference: await page.evaluate(() => localStorage.getItem('beeRosVoiceEnabled')),
    overflow1024,
    consoleErrors,
  };
  console.log(JSON.stringify(result, null, 2));
  if (overflow1024) throw new Error('Horizontal overflow detected at 1024x900.');
  if (consoleErrors.length) throw new Error(`Browser console errors: ${JSON.stringify(consoleErrors)}`);
  await browser.close();
})().catch((error) => {
  console.error(error.stack || error.message);
  process.exit(1);
});
