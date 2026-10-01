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
    { width: 1440, height: 1000, scale: 1, name: '1440x1000' },
    { width: 1536, height: 768, scale: 1.25, name: '1920x960-windows125pct' },
    { width: 1024, height: 900, scale: 1, name: '1024x900' },
    { width: 1152, height: 800, scale: 1.25, name: '1440x1000-125pct' },
    { width: 960, height: 667, scale: 1.5, name: '1440x1000-150pct' },
  ]) {
    const context = await browser.newContext({
      viewport: { width: viewport.width, height: viewport.height },
      deviceScaleFactor: viewport.scale,
    });
    const page = await context.newPage();
    const errors = [];
    page.on('console', (message) => { if (message.type() === 'error') errors.push(message.text()); });
    page.on('pageerror', (error) => errors.push(error.message));
    await page.goto('http://192.168.127.10:8080/', { waitUntil: 'networkidle' });
    await page.waitForFunction(() => document.querySelectorAll('.live-radar-point').length > 20);
    const audit = await page.evaluate(() => {
      const root = document.querySelector('#bee-console');
      const vehicle = root.querySelector('.vehicle-strip');
      const side = root.querySelector('.side');
      const manual = side.querySelector('.side-control:not(.side-patrol)');
      const patrol = side.querySelector('.side-patrol');
      const disabled = [...root.querySelectorAll('.danger,.takeover,.joy,.side-patrol button')]
        .every((button) => button.disabled);
      const vehicleRect = vehicle.getBoundingClientRect();
      const mapRect = root.querySelector('.map-panel').getBoundingClientRect();
      const manualRect = manual.getBoundingClientRect();
      const patrolRect = patrol.getBoundingClientRect();
      return {
        scrollWidth: document.documentElement.scrollWidth,
        clientWidth: document.documentElement.clientWidth,
        pageHeight: document.documentElement.scrollHeight,
        viewportHeight: window.innerHeight,
        pageFitsViewport: document.documentElement.scrollHeight <= window.innerHeight + 1,
        vehicleAboveMap: vehicleRect.bottom <= mapRect.top + 1,
        manualInRight: manualRect.left >= mapRect.right - 2 || window.innerWidth <= 980,
        patrolInRight: patrolRect.left >= mapRect.right - 2 || window.innerWidth <= 980,
        manualVisible: manualRect.bottom <= window.innerHeight + 1 || window.innerWidth <= 980,
        patrolVisible: patrolRect.bottom <= window.innerHeight + 1 || window.innerWidth <= 980,
        bottomCount: root.querySelectorAll('.bottom').length,
        metricCount: vehicle.querySelectorAll('.metric').length,
        disabled,
        radarPoints: root.querySelectorAll('.live-radar-point').length,
        mapClass: root.querySelector('.map').className,
      };
    });
    if (audit.scrollWidth > audit.clientWidth + 1) throw new Error(`${viewport.name} has horizontal overflow`);
    if (!audit.vehicleAboveMap || !audit.manualInRight || !audit.patrolInRight) {
      throw new Error(`${viewport.name} layout placement failed`);
    }
    if (viewport.width > 980 && (!audit.pageFitsViewport || !audit.manualVisible || !audit.patrolVisible)) {
      throw new Error(`${viewport.name} desktop controls do not fit the first viewport`);
    }
    if (audit.bottomCount !== 0 || audit.metricCount !== 6 || !audit.disabled) {
      throw new Error(`${viewport.name} structure or safety state failed`);
    }
    if (errors.length) throw new Error(`${viewport.name} console errors: ${JSON.stringify(errors)}`);
    if (viewport.name === '1440x1000') {
      await page.locator('[data-primary-view="radar"]').click();
      await page.waitForSelector('.map.radar-expanded');
      const expanded = await page.locator('.live-radar-grid').boundingBox();
      const mapBox = await page.locator('.map').boundingBox();
      if (!await page.locator('.radar-minimize').isVisible()
          || Math.abs(expanded.width - mapBox.width) > 2
          || Math.abs(expanded.height - mapBox.height) > 2) {
        throw new Error('LD14 expanded view regressed after layout change');
      }
      await page.locator('.radar-minimize').click();
      await page.waitForFunction(() => !document.querySelector('.map').classList.contains('radar-expanded'));
    }
    const screenshot = path.join(outputDir, `20260926-BeeROSWebConsole-紧凑右侧布局-${viewport.name}.png`);
    await page.screenshot({ path: screenshot, fullPage: true });
    results.push({ viewport: viewport.name, screenshot, ...audit, errors });
    await context.close();
  }
  console.log(JSON.stringify(results, null, 2));
  await browser.close();
})().catch((error) => {
  console.error(error.stack || error);
  process.exit(1);
});
