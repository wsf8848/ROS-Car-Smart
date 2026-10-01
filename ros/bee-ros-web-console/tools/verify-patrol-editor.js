const { chromium } = require('playwright');
const path = require('path');

const assert = (condition, message) => { if (!condition) throw new Error(message); };
const outputDir = process.argv[2];
const screenshotPrefix = process.env.BEE_TEST_STAMP || '地图交互验收';
assert(outputDir && process.env.BEE_TEST_PASSWORD, 'Output directory and BEE_TEST_PASSWORD required');

(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.BEE_EDGE_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  });
  const results = [];
  try {
    for (const viewport of [{width: 1440, height: 1000}, {width: 1024, height: 900}, {width: 1536, height: 768}]) {
      const context = await browser.newContext({viewport});
      const page = await context.newPage();
      const errors = [];
      page.on('pageerror', (error) => errors.push(error.message));
      page.on('console', (message) => { if (message.type() === 'error') errors.push(message.text()); });
      try {
        await page.goto('http://192.168.127.10:8080/login.html');
        await page.locator('#username').fill('FYX');
        await page.locator('#password').fill(process.env.BEE_TEST_PASSWORD);
        await page.locator('#login-submit').click();
        await page.waitForURL('http://192.168.127.10:8080/');
        await page.waitForSelector('.map.map-present .live-map-image');
        const point = async (col, row) => page.locator('.occupancy-grid').evaluate((svg, coords) => {
          const matrix = svg.getScreenCTM();
          const screen = new DOMPoint(coords.col + 0.5, 30 - coords.row - 0.5).matrixTransform(matrix);
          return {x: screen.x, y: screen.y};
        }, {col, row});
        const green = await point(21, 10);
        const red = await point(16, 10);
        const unknown = await point(1, 1);
        await page.mouse.click(green.x, green.y);
        assert(await page.locator('.goal-pose-editor').isVisible(), 'Green cell did not select target');
        const initial = await page.locator('.map').getAttribute('data-selected-map-col');
        assert(initial === '21', `Unexpected selected cell: ${initial}`);
        await page.mouse.click(red.x, red.y);
        assert(await page.locator('.map').getAttribute('data-selected-map-col') === '21', 'Obstacle cell changed target');
        await page.mouse.click(unknown.x, unknown.y);
        assert(await page.locator('.map').getAttribute('data-selected-map-col') === '21', 'Unknown cell changed target');
        await page.locator('[data-goal-angle-number]').fill('90');
        await page.locator('[data-goal-angle-number]').dispatchEvent('change');
        assert(await page.locator('[data-goal-angle-range]').inputValue() === '90', 'Angle input did not update slider');
        const marker = await page.locator('.live-map-selection').boundingBox();
        const handle = await page.locator('.live-goal-handle').boundingBox();
        assert(marker && marker.width >= 16 && marker.height >= 16, `Goal marker is too small: ${JSON.stringify(marker)}`);
        assert(handle, 'Direction drag handle not visible');
        const markerCenter = {x: marker.x + marker.width / 2, y: marker.y + marker.height / 2};
        await page.mouse.move(markerCenter.x, markerCenter.y);
        await page.mouse.down();
        await page.waitForTimeout(80);
        await page.mouse.up();
        assert(await page.locator('[data-goal-angle-number]').inputValue() === '90', 'Short click unexpectedly changed heading');
        assert(await page.locator('.map').getAttribute('data-selected-map-col') === '21', 'Short click moved the selected target');
        await page.mouse.move(markerCenter.x, markerCenter.y);
        await page.mouse.down();
        await page.waitForTimeout(220);
        await page.mouse.move(green.x + 36, green.y, {steps: 8});
        await page.mouse.up();
        assert(Math.abs(Number(await page.locator('[data-goal-angle-number]').inputValue())) < 45, 'Drag did not change heading toward map X');
        assert(await page.locator('.map').getAttribute('data-selected-map-col') === '21', 'Direction drag moved the selected target');
        const frozenHeading = await page.locator('[data-goal-angle-number]').inputValue();
        await page.mouse.move(green.x - 40, green.y + 40, {steps: 4});
        assert(await page.locator('[data-goal-angle-number]').inputValue() === frozenHeading, 'Heading changed after pointer release');
        await page.locator('[data-goal-angle-number]').fill('90');
        await page.locator('[data-goal-angle-number]').dispatchEvent('change');
        const headingBeforeBlockedMenu = await page.locator('.live-goal-handle').locator('xpath=..').getAttribute('transform');
        await page.mouse.click(red.x, red.y, {button: 'right'});
        const headingAfterBlockedMenu = await page.locator('.live-goal-handle').locator('xpath=..').getAttribute('transform');
        assert(headingAfterBlockedMenu === headingBeforeBlockedMenu, `Obstacle context menu changed goal pose: before=${headingBeforeBlockedMenu}, after=${headingAfterBlockedMenu}, traversable=${await page.locator('.map-context').getAttribute('data-traversable')}`);
        assert(await page.locator('.map-context button').first().isDisabled(), 'Obstacle context menu allowed target action');
        await page.mouse.click(green.x, green.y);
        assert(await page.locator('[data-goal-navigate]').isDisabled(), 'Navigation safety gate unexpectedly open');

        await page.locator('[data-patrol-action="edit"]').click();
        await page.locator('[data-patrol-tool="map-add"]').click();
        const card = await page.locator('.patrol-editor-card').boundingBox();
        const side = await page.locator('.side-patrol').boundingBox();
        assert(card.x + card.width <= side.x + 2 || viewport.width <= 980, `Floating editor overlaps patrol sidebar: card=${JSON.stringify(card)}, side=${JSON.stringify(side)}`);
        const waypointOne = await point(30, 10);
        const waypointTwo = await point(35, 12);
        await page.mouse.click(waypointOne.x, waypointOne.y);
        await page.mouse.click(waypointTwo.x, waypointTwo.y);
        assert(await page.locator('.patrol-waypoint-row').count() === 2, 'Map clicks did not add two waypoints');
        await page.screenshot({path: path.join(outputDir, `${screenshotPrefix}-地图点选航点-${viewport.width}x${viewport.height}.png`), fullPage: true});
        const blocked = await point(44, 10);
        const unexplored = await point(46, 1);
        await page.mouse.click(blocked.x, blocked.y);
        await page.mouse.click(unexplored.x, unexplored.y);
        assert(await page.locator('.patrol-waypoint-row').count() === 2, 'Nontraversable cells added waypoint');
        await page.locator('[data-patrol-tool="map-add"]').click();
        await page.locator('.patrol-waypoint-row').first().locator('[data-waypoint-yaw]').fill('135');
        await page.locator('.patrol-waypoint-row').first().locator('[data-waypoint-yaw]').dispatchEvent('change');
        assert(await page.locator('[data-goal-angle-number]').inputValue() === '135', 'Waypoint heading did not synchronize');
        const firstCoordinate = await page.locator('.patrol-waypoint-row').first().locator('.patrol-waypoint-coord').innerText();
        await page.locator('.patrol-waypoint-row').first().locator('[data-waypoint-move="1"]').click();
        assert((await page.locator('.patrol-waypoint-row').nth(1).locator('.patrol-waypoint-coord').innerText()) === firstCoordinate, 'Waypoint reordering failed');
        await page.locator('.patrol-waypoint-row').first().locator('[data-waypoint-delete]').click();
        assert(await page.locator('.patrol-waypoint-row').count() === 1, 'Waypoint removal failed');
        await page.locator('.patrol-editor-dialog [data-patrol-repeat]').fill('100');
        await page.locator('.patrol-editor-dialog [data-patrol-repeat]').dispatchEvent('change');
        assert(await page.locator('.patrol-editor-dialog [data-patrol-repeat]').inputValue() === '99', 'Maximum patrol count failed');
        await page.locator('.patrol-editor-dialog [data-patrol-repeat]').fill('0');
        await page.locator('.patrol-editor-dialog [data-patrol-repeat]').dispatchEvent('change');
        assert(await page.locator('.patrol-editor-dialog [data-patrol-repeat]').inputValue() === '1', 'Minimum patrol count failed');
        assert(await page.locator('[data-patrol-action="start"]').isDisabled(), 'Patrol safety gate unexpectedly open');
        const audit = await page.evaluate(() => ({
          viewport: `${innerWidth}x${innerHeight}`,
          scrollWidth: document.documentElement.scrollWidth,
          clientWidth: document.documentElement.clientWidth,
          heading: document.querySelector('[data-goal-angle-number]').value,
          waypoints: document.querySelectorAll('.patrol-waypoint-row').length,
          repeat: document.querySelector('.patrol-editor-dialog [data-patrol-repeat]').value,
        }));
        assert(audit.scrollWidth <= audit.clientWidth + 1, 'Horizontal overflow');
        assert(errors.length === 0, `Browser errors: ${JSON.stringify(errors)}`);
        await page.screenshot({path: path.join(outputDir, `${screenshotPrefix}-巡逻编辑-${audit.viewport}.png`), fullPage: true});
        results.push(audit);
      } finally {
        await context.close();
      }
    }
    console.log(JSON.stringify({ok: true, results}, null, 2));
  } finally {
    await browser.close();
  }
})().catch((error) => { console.error(error.stack || error); process.exitCode = 1; });
