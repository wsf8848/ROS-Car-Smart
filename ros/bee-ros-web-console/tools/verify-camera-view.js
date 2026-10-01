const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
let browser;

(async () => {
  const outputDir = process.argv[2];
  if (!outputDir) throw new Error('Missing screenshot output directory.');
  fs.mkdirSync(outputDir, { recursive: true });

  browser = await chromium.launch({
    headless: true,
    executablePath: process.env.BEE_EDGE_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  });
  const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
  const page = await context.newPage();
  const errors = [];
  page.on('console', (message) => { if (message.type() === 'error') errors.push(message.text()); });
  page.on('pageerror', (error) => errors.push(error.message));

  await page.goto('http://192.168.127.10:8080/', { waitUntil: 'domcontentloaded' });
  if (page.url().includes('/login.html')) {
    const username = process.env.BEE_TEST_USERNAME;
    const password = process.env.BEE_TEST_PASSWORD;
    if (!username || !password) throw new Error('Authentication required; set BEE_TEST_USERNAME and BEE_TEST_PASSWORD.');
    await page.locator('input[name="username"]').fill(username);
    await page.locator('input[name="password"]').fill(password);
    await page.locator('button[type="submit"]').click();
    await page.waitForURL('http://192.168.127.10:8080/');
  }

  await page.locator('[data-primary-view="camera"]').click();
  await page.waitForSelector('.map.camera-expanded .camera-view');
  await page.waitForFunction(async () => {
    const response = await fetch('/api/camera/status', {cache: 'no-store'});
    const status = await response.json();
    return status.online && status.measured_fps >= 10;
  }, null, {timeout: 12000});
  await page.waitForFunction(() => {
    const image = document.querySelector('.camera-frame');
    return image && image.naturalWidth > 0 && image.naturalHeight > 0;
  }, null, {timeout: 12000});
  await page.locator('.camera-status.online').waitFor({state: 'visible', timeout: 5000});

  const audit = await page.evaluate(async () => {
    const map = document.querySelector('.map');
    const image = document.querySelector('.camera-frame');
    const response = await fetch('/api/camera/status', {cache: 'no-store'});
    const status = await response.json();
    return {
      mapClass: map.className,
      imageWidth: image.naturalWidth,
      imageHeight: image.naturalHeight,
      minimizeVisible: document.querySelector('.camera-minimize').offsetParent !== null,
      pageOverflow: document.documentElement.scrollWidth > document.documentElement.clientWidth,
      status,
    };
  });
  if (audit.imageWidth !== 960 || audit.imageHeight !== 540 || !audit.minimizeVisible || audit.pageOverflow) {
    throw new Error(`Camera view audit failed: ${JSON.stringify(audit)}`);
  }
  await page.screenshot({
    path: path.join(outputDir, '20260927-BeeROSWebConsole-C70实时画面-1440x1000.png'),
    fullPage: true,
  });

  await page.locator('.camera-minimize').click();
  await page.waitForFunction(() => !document.querySelector('.map').classList.contains('camera-expanded'));
  await page.waitForFunction(async () => {
    const response = await fetch('/api/camera/status', {cache: 'no-store'});
    const status = await response.json();
    return status.clients >= 1 && status.online;
  }, null, {timeout: 6000});
  const minimizedAudit = await page.evaluate(() => {
    const map = document.querySelector('.map');
    const view = document.querySelector('.camera-view');
    const rect = view.getBoundingClientRect();
    return {cameraVisible: map.classList.contains('camera-visible'), cameraMinimized: map.classList.contains('camera-minimized'), width: Math.round(rect.width), height: Math.round(rect.height), display: getComputedStyle(view).display};
  });
  if (!minimizedAudit.cameraVisible || !minimizedAudit.cameraMinimized || minimizedAudit.display === 'none' || minimizedAudit.width < 200 || minimizedAudit.height < 120) {
    throw new Error(`Camera minimize audit failed: ${JSON.stringify(minimizedAudit)}`);
  }
  await page.screenshot({
    path: path.join(outputDir, '20260927-BeeROSWebConsole-C70最小化右下角小窗-1440x1000.png'),
    fullPage: true,
  });
  await page.locator('[data-primary-view="radar"]').click();
  await page.waitForSelector('.map.radar-expanded');
  if (!await page.locator('.radar-minimize').isVisible()) throw new Error('LD14 main view regressed.');
  await page.locator('.radar-minimize').click();
  await page.waitForFunction(() => !document.querySelector('.map').classList.contains('radar-expanded'));

  await page.setViewportSize({width: 1024, height: 900});
  await page.locator('[data-primary-view="camera"]').click();
  await page.locator('.camera-status.online').waitFor({state: 'visible', timeout: 12000});
  await page.waitForFunction(() => {
    const image = document.querySelector('.camera-frame');
    return image && image.naturalWidth === 960 && image.naturalHeight === 540;
  }, null, {timeout: 12000});
  const narrowAudit = await page.evaluate(() => ({
    overflow: document.documentElement.scrollWidth > document.documentElement.clientWidth,
    imageWidth: document.querySelector('.camera-frame').naturalWidth,
    imageHeight: document.querySelector('.camera-frame').naturalHeight,
  }));
  if (narrowAudit.overflow || narrowAudit.imageWidth !== 960 || narrowAudit.imageHeight !== 540) {
    throw new Error(`1024x900 camera audit failed: ${JSON.stringify(narrowAudit)}`);
  }
  await page.screenshot({
    path: path.join(outputDir, '20260927-BeeROSWebConsole-C70实时画面-1024x900.png'),
    fullPage: true,
  });
  await page.locator('.camera-minimize').click();
  await page.waitForFunction(async () => {
    const response = await fetch('/api/camera/status', {cache: 'no-store'});
    const status = await response.json();
    return status.clients >= 1 && status.online;
  }, null, {timeout: 6000});

  await page.setViewportSize({width: 1536, height: 768});
  const desktopAudit = await page.evaluate(() => {
    const manual = document.querySelector('.side-control:not(.side-patrol)').getBoundingClientRect();
    const patrol = document.querySelector('.side-patrol').getBoundingClientRect();
    return {
      pageFits: document.documentElement.scrollHeight <= window.innerHeight + 1,
      manualVisible: manual.bottom <= window.innerHeight + 1,
      patrolVisible: patrol.bottom <= window.innerHeight + 1,
      overflow: document.documentElement.scrollWidth > document.documentElement.clientWidth,
    };
  });
  if (!desktopAudit.pageFits || !desktopAudit.manualVisible || !desktopAudit.patrolVisible || desktopAudit.overflow) {
    throw new Error(`1536x768 layout audit failed: ${JSON.stringify(desktopAudit)}`);
  }
  if (errors.length) throw new Error(`Browser console errors: ${JSON.stringify(errors)}`);
  console.log(JSON.stringify({...audit, minimizedAudit, narrowAudit, desktopAudit, consoleErrors: errors}, null, 2));
  await context.close();
  await browser.close();
  browser = null;
})().catch(async (error) => {
  if (browser) await browser.close().catch(() => {});
  console.error(error.stack || error);
  process.exit(1);
});
