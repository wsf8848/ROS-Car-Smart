const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

(async () => {
  const outputDir = process.argv[2];
  if (!outputDir) throw new Error('Missing output directory.');
  fs.mkdirSync(outputDir, { recursive: true });
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.BEE_EDGE_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  });
  const results = [];
  for (const viewport of [
    {width: 1440, height: 900, name: '1440x900'},
    {width: 1024, height: 768, name: '1024x768'},
    {width: 624, height: 900, name: '624x900'},
  ]) {
    const context = await browser.newContext({viewport: {width: viewport.width, height: viewport.height}});
    const page = await context.newPage();
    const errors = [];
    page.on('console', (message) => { if (message.type() === 'error') errors.push(message.text()); });
    page.on('pageerror', (error) => errors.push(error.message));
    const response = await page.goto('http://192.168.127.10:8080/login.html', {waitUntil: 'networkidle'});
    if (!response || response.status() !== 200) throw new Error(`${viewport.name} login HTTP failure`);
    const audit = await page.evaluate(() => {
      const form = document.querySelector('#login-form');
      const formRect = form.getBoundingClientRect();
      const panelRect = document.querySelector('.login-panel').getBoundingClientRect();
      const scene = document.querySelector('.system-scene');
      return {
        scrollWidth: document.documentElement.scrollWidth,
        clientWidth: document.documentElement.clientWidth,
        scrollHeight: document.documentElement.scrollHeight,
        clientHeight: document.documentElement.clientHeight,
        formInsidePanel: formRect.left >= panelRect.left && formRect.right <= panelRect.right + 1,
        sceneVisible: getComputedStyle(scene).display !== 'none',
        requiredControls: ['username', 'password', 'remember', 'login-submit'].every((id) => document.getElementById(id)),
        authStatus: document.querySelector('#connection-text').textContent,
      };
    });
    if (audit.scrollWidth > audit.clientWidth + 1 || !audit.formInsidePanel || !audit.requiredControls) {
      throw new Error(`${viewport.name} layout audit failed: ${JSON.stringify(audit)}`);
    }
    if (viewport.width > 900 && !audit.sceneVisible) throw new Error(`${viewport.name} desktop scene hidden`);
    if (viewport.width <= 900 && audit.sceneVisible) throw new Error(`${viewport.name} compact scene not hidden`);
    if (errors.length) throw new Error(`${viewport.name} console errors: ${JSON.stringify(errors)}`);
    const screenshot = path.join(outputDir, `20260926-BeeROS-登录页-${viewport.name}.png`);
    await page.screenshot({path: screenshot, fullPage: true});
    results.push({viewport: viewport.name, screenshot, ...audit, errors});
    await context.close();
  }
  console.log(JSON.stringify(results, null, 2));
  await browser.close();
})().catch((error) => {
  console.error(error.stack || error);
  process.exit(1);
});
