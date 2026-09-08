// npm install --prefix .toolchains/browser playwright qrcode
// Generate offline pages with the same renderer used by the phone editor.
import { chromium } from '../.toolchains/browser/node_modules/playwright/index.mjs';
import QRCode from '../.toolchains/browser/node_modules/qrcode/lib/index.js';
import { readFile, writeFile } from 'node:fs/promises';
const browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless: true });
try {
  const page = await browser.newPage();
  const source = await readFile(new URL('../firmware/badge/web/badge-render.js', import.meta.url), 'utf8');
  await page.route('http://badge-build.test/**', route => route.fulfill({ contentType: route.request().url().endsWith('.js') ? 'application/javascript' : 'text/html', body: route.request().url().endsWith('.js') ? source : '<!doctype html><meta charset="utf-8">' }));
  await page.goto('http://badge-build.test/');
  const qr = await QRCode.toDataURL('https://example.com', { width: 200, margin: 4 });
  const bytes = await page.evaluate(async qr => {
    const { renderPages, rgb565 } = await import('/badge-render.js');
    const avatar = document.createElement('canvas'); avatar.width = avatar.height = 240;
    const a = avatar.getContext('2d'); a.fillStyle = '#b8d0a4'; a.fillRect(0, 0, 240, 240);
    a.fillStyle = '#264e3b'; a.beginPath(); a.arc(120, 83, 42, 0, Math.PI * 2); a.fill();
    a.beginPath(); a.ellipse(120, 221, 88, 75, 0, 0, Math.PI * 2); a.fill();
    const frames = await renderPages({ name: '林小满', role: '产品设计师', company: 'FoloToy · 示例工牌', wechat: '', bio: '上 / 下翻页，确认切换头像与二维码', avatar: avatar.toDataURL(), qr });
    return frames.flatMap((canvas, page) => {
      const c = canvas.getContext('2d'); c.fillStyle = '#153c34'; c.fillRect(0, 0, 240, 34); c.fillRect(0, 290, 240, 30);
      c.fillStyle = '#b8d0a4'; c.font = '11px "Microsoft YaHei",sans-serif'; c.fillText('FOLOTOY / 离线示例', 18, 24);
      c.fillText(`${page + 1}/3  长按确认：手机配置`, 18, 309);
      if (page === 2) { c.fillStyle = '#153c34'; c.fillRect(0, 260, 240, 29); c.fillStyle = '#fff'; c.font = '12px "Microsoft YaHei",sans-serif'; c.fillText('测试二维码 · 非微信好友码', 35, 280); }
      return Array.from(rgb565(c.getImageData(0, 0, 240, 320).data));
    });
  }, qr);
  await writeFile(new URL('../firmware/badge/main/sample.rgb', import.meta.url), new Uint8Array(bytes));
  console.log(`Generated ${bytes.length} bytes of offline sample frames`);
} finally { await browser.close(); }
