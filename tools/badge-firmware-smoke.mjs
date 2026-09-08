// npm install --prefix .toolchains/browser playwright
import { chromium } from '../.toolchains/browser/node_modules/playwright/index.mjs';
import { readFile, mkdir } from 'node:fs/promises';
import assert from 'node:assert/strict';
const sample = await readFile(new URL('../firmware/badge/main/sample.rgb', import.meta.url));
function hash(bytes) { let h = 2166136261; for (const b of bytes) h = Math.imul(h ^ b, 16777619); return h >>> 0; }
const expected = [0, 1, 2].map(i => hash(sample.subarray(i * 153600, (i + 1) * 153600)));
const browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless: true });
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  await page.addInitScript(() => localStorage.setItem('folotoy.simulator-notice.shown', '1'));
  const base = process.env.BADGE_TEST_URL || 'http://127.0.0.1:4192';
  assert.equal((await page.request.get(base + '/badge.html')).status(), 404);
  await page.goto(base + '/');
  await page.waitForFunction(() => !document.querySelector('#firmware-file').disabled);
  await page.locator('#firmware-file').setInputFiles('public/assets/firmware/identity-badge.bin');
  await mkdir('artifacts', { recursive: true });
  async function screen(index, name) {
    await page.waitForFunction(target => {
      const rgba = document.querySelector('#qemu-display').getContext('2d').getImageData(0, 0, 240, 320).data;
      let h = 2166136261;
      for (let i = 0; i < rgba.length; i += 4) {
        const p = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
        h = Math.imul(h ^ (p >> 8), 16777619); h = Math.imul(h ^ (p & 255), 16777619);
      }
      return (h >>> 0) === target;
    }, expected[index], { timeout: 60000, polling: 400 });
    await page.screenshot({ path: `artifacts/badge-firmware-${name}.png`, fullPage: true });
    console.log(`PASS ${name}: every rendered RGB565 pixel matches the embedded frame`);
  }
  await screen(0, 'profile');
  await page.locator('[data-key="DOWN"]').click(); await screen(1, 'avatar');
  await page.locator('[data-key="OK"]').click(); await screen(2, 'qr');
  await page.locator('[data-key="OK"]').click(); await screen(1, 'avatar-return');
  await page.locator('[data-key="UP"]').click(); await screen(0, 'profile-return');
  const ok = page.locator('[data-key="OK"]');
  await ok.hover(); await page.mouse.down(); await page.waitForTimeout(3500); await page.mouse.up();
  await page.waitForFunction(() => {
    const p = document.querySelector('#qemu-display').getContext('2d').getImageData(0, 0, 1, 1).data;
    return p[0] === 16 && p[1] === 61 && p[2] === 49;
  }, null, { timeout: 30000 });
  console.log('PASS long OK: Wi-Fi configuration screen');
  await ok.hover(); await page.mouse.down(); await page.waitForTimeout(3500); await page.mouse.up();
  await screen(0, 'after-config');
  await page.locator('#restart-runtime').click(); await page.waitForTimeout(1000); await screen(0, 'restart');
  console.log('Firmware simulator PASS: offline boot, three pages, physical button switching, long OK, restart, removed standalone page');
} finally { await browser.close(); }
