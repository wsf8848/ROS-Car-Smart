const {chromium} = require('playwright');

const assert = (condition, message) => { if (!condition) throw new Error(message); };
assert(process.env.BEE_TEST_PASSWORD, 'BEE_TEST_PASSWORD required');

(async () => {
  const browser = await chromium.launch({headless: true, executablePath: process.env.BEE_EDGE_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe'});
  const page = await browser.newPage({viewport: {width: 1440, height: 1000}});
  let mapId = null;
  let draftId = null;
  try {
    await page.goto('http://192.168.127.10:8080/login.html');
    await page.locator('#username').fill('FYX');
    await page.locator('#password').fill(process.env.BEE_TEST_PASSWORD);
    await page.locator('#login-submit').click();
    await page.waitForURL('http://192.168.127.10:8080/');
    const maps = await page.evaluate(async () => (await fetch('/api/maps')).json());
    assert(!maps.status.loaded, 'A map is already loaded; leave it untouched');
    mapId = maps.items.find((item) => item.complete)?.id;
    assert(mapId, 'No complete saved map available');
    const load = await page.evaluate(async (id) => {
      const response = await fetch('/api/maps/action', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({action: 'load', map_id: id})});
      return {status: response.status, body: await response.json()};
    }, mapId);
    assert(load.status === 200, `Map load failed: ${JSON.stringify(load)}`);
    await page.waitForSelector('.map.map-present .live-map-image', {timeout: 15000});
    await page.waitForTimeout(900);
    const free = await page.locator('.live-map-image').evaluate(async (image) => {
      const bitmap = new Image();
      bitmap.src = image.getAttribute('href');
      await bitmap.decode();
      const canvas = document.createElement('canvas'); canvas.width = bitmap.width; canvas.height = bitmap.height;
      const ctx = canvas.getContext('2d'); ctx.drawImage(bitmap, 0, 0);
      const pixels = ctx.getImageData(0, 0, bitmap.width, bitmap.height).data;
      const matrix = document.querySelector('.occupancy-grid').getScreenCTM();
      for (let row = 2; row < bitmap.height - 2; row += 1) {
        for (let col = 2; col < bitmap.width - 2; col += 1) {
          const offset = (row * bitmap.width + col) * 4;
          if (pixels[offset] !== 22 || pixels[offset + 1] !== 124 || pixels[offset + 2] !== 85) continue;
          const screen = new DOMPoint(col + .5, row + .5).matrixTransform(matrix);
          const map = document.querySelector('.map').getBoundingClientRect();
          if (screen.x > map.left + 30 && screen.x < map.right - 300 && screen.y > map.top + 80 && screen.y < map.bottom - 85) return {x: screen.x, y: screen.y};
        }
      }
      return null;
    });
    assert(free, 'Saved map has no unobstructed free cell');
    await page.mouse.click(free.x, free.y);
    assert(await page.locator('.goal-pose-editor').isVisible(), 'Saved map selection failed');
    await page.locator('[data-patrol-action="edit"]').click();
    await page.locator('[data-patrol-tool="add-current"]').click();
    assert(await page.locator('.patrol-waypoint-row').count() === 1, 'Add current goal failed');
    assert(await page.locator('[data-patrol-tool="save"]').isEnabled(), 'Save did not unlock after waypoint addition');
    await page.locator('[data-patrol-name]').fill('验收临时巡逻任务');
    await page.locator('.patrol-editor-dialog [data-patrol-repeat]').fill('3');
    await page.locator('.patrol-editor-dialog [data-patrol-repeat]').dispatchEvent('change');
    await page.locator('[data-patrol-tool="save"]').click();
    await page.waitForFunction(() => document.querySelector('.map-feedback')?.textContent?.includes('巡逻任务已保存到地瓜派'));
    const saved = await page.evaluate(async (id) => (await (await fetch(`/api/patrols?map_id=${encodeURIComponent(id)}`)).json()).items, mapId);
    const draft = saved.find((item) => item.name === '验收临时巡逻任务');
    assert(draft && draft.repeat_count === 3 && draft.waypoints.length === 1, 'Saved draft mismatch');
    draftId = draft.id;
    await page.reload();
    await page.waitForSelector('.map.map-present .live-map-image');
    await page.locator('[data-patrol-action="edit"]').click();
    await page.waitForFunction(() => document.querySelector('.patrol-waypoint-row') && document.querySelector('.patrol-editor-dialog [data-patrol-repeat]')?.value === '3');
    assert(await page.locator('[data-patrol-name]').inputValue() === '验收临时巡逻任务', 'Draft did not restore after refresh');
    assert(await page.locator('[data-patrol-action="start"]').isDisabled(), 'Patrol motion safety gate open');
    console.log(JSON.stringify({ok: true, map_id: mapId, repeat_count: 3, waypoints: 1, restored: true, motion_disabled: true}));
  } finally {
    if (draftId) {
      const deleted = await page.evaluate(async (id) => {
        const response = await fetch('/api/patrols/action', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({action: 'delete', id})});
        return response.ok;
      }, draftId).catch(() => false);
      if (!deleted) console.error(`Manual cleanup needed for test draft ${draftId}`);
    }
    if (mapId) {
      const unloaded = await page.evaluate(async () => {
        const response = await fetch('/api/maps/action', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({action: 'unload'})});
        return response.ok;
      }).catch(() => false);
      if (!unloaded) console.error(`Manual unload needed for map ${mapId}`);
    }
    await browser.close();
  }
})().catch((error) => { console.error(error.stack || error); process.exitCode = 1; });
