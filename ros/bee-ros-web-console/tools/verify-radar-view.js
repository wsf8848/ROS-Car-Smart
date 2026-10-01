const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

(async () => {
  const outputDir = process.argv[2];
  if (!outputDir) throw new Error('Missing screenshot output directory.');
  fs.mkdirSync(outputDir, { recursive: true });

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

  await page.goto('http://192.168.127.10:8080/', { waitUntil: 'networkidle' });
  await page.waitForSelector('.map.map-present .live-radar-grid');
  await page.waitForFunction(() => document.querySelectorAll('.live-radar-point').length > 20);

  const mapInitial = await page.locator('.map').getAttribute('class');
  const initialRadarBox = await page.locator('.live-radar-grid').boundingBox();
  const initialMapBox = await page.locator('.map').boundingBox();
  if (!mapInitial.includes('map-present') || mapInitial.includes('radar-expanded')) {
    throw new Error(`Unexpected initial map state: ${mapInitial}`);
  }
  if (!(initialRadarBox.width < initialMapBox.width / 2 && initialRadarBox.height < initialMapBox.height / 2)) {
    throw new Error('LD14 did not start as a minimized radar window.');
  }

  await page.locator('.layer-toggle[data-layer="scan"]').click();
  await page.waitForSelector('.map.radar-expanded');
  const expandedRadarBox = await page.locator('.live-radar-grid').boundingBox();
  const expandedMapBox = await page.locator('.map').boundingBox();
  const minimizeVisible = await page.locator('.radar-minimize').isVisible();
  if (!minimizeVisible || Math.abs(expandedRadarBox.width - expandedMapBox.width) > 2 || Math.abs(expandedRadarBox.height - expandedMapBox.height) > 2) {
    throw new Error('LD14 expanded view did not fill the map viewport.');
  }

  const target = await page.locator('.live-radar-point').evaluateAll((points) => {
    const radar = document.querySelector('.live-radar-grid');
    const matrix = radar.getScreenCTM();
    if (!matrix || !points.length) return null;
    const candidates = points.map((point) => {
      const svgPoint = new DOMPoint(Number(point.getAttribute('cx')), Number(point.getAttribute('cy'))).matrixTransform(matrix);
      return { x: svgPoint.x, y: svgPoint.y, range: Number(point.dataset.range) };
    }).filter((point) => Number.isFinite(point.x) && Number.isFinite(point.y) && point.range > 0.35);
    candidates.sort((a, b) => a.range - b.range);
    return candidates[Math.floor(candidates.length / 3)] || candidates[0] || null;
  });
  if (!target) throw new Error('No radar point available for hover verification.');
  await page.mouse.move(target.x, target.y);

  const hoverSamples = [];
  for (let index = 0; index < 14; index += 1) {
    await page.waitForTimeout(250);
    hoverSamples.push(await page.locator('.live-radar-tooltip').isVisible());
  }
  if (!hoverSamples.every(Boolean)) {
    throw new Error(`Radar tooltip did not remain visible across scan frames: ${JSON.stringify(hoverSamples)}`);
  }
  const tooltipText = await page.locator('.live-radar-tooltip').innerText();
  await page.screenshot({ path: path.join(outputDir, '20260925-BeeROSWebConsole-LD14大画面悬停保持-1440x1000.png'), fullPage: true });

  await page.locator('.radar-minimize').click();
  await page.waitForFunction(() => !document.querySelector('.map').classList.contains('radar-expanded'));
  const minimizedRadarBox = await page.locator('.live-radar-grid').boundingBox();
  if (!(minimizedRadarBox.width < expandedRadarBox.width / 2 && minimizedRadarBox.height < expandedRadarBox.height / 2)) {
    throw new Error('Radar minimize button did not restore the small window.');
  }
  await page.screenshot({ path: path.join(outputDir, '20260925-BeeROSWebConsole-地图主视图与LD14小窗-1440x1000.png'), fullPage: true });

  await page.locator('.layer-toggle[data-layer="scan"]').click();
  await page.locator('.layer-toggle[data-layer="grid"]').click();
  await page.waitForFunction(() => !document.querySelector('.map').classList.contains('radar-expanded'));

  await page.setViewportSize({ width: 1024, height: 900 });
  await page.waitForTimeout(300);
  const overflow1024 = await page.evaluate(() => document.documentElement.scrollWidth > document.documentElement.clientWidth);

  const result = {
    initialRadar: initialRadarBox,
    expandedRadar: expandedRadarBox,
    minimizedRadar: minimizedRadarBox,
    hoverDurationMs: hoverSamples.length * 250,
    hoverSamples,
    tooltipText,
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
