export const FRAME_BYTES = 240 * 320 * 2;
export const PIXEL_BYTES = FRAME_BYTES * 3;
export const MAX_PROFILE_BYTES = 220000;
export const defaults = { name: '你的名字', role: '产品设计师', company: 'FoloToy', wechat: '', bio: '很高兴认识你，一起做点有趣的事。', avatar: '', qr: '' };
export function nextPage(page, key) {
  return key === 'up' ? (page + 2) % 3 : key === 'ok' ? (page === 1 ? 2 : 1) : (page + 1) % 3;
}
export function rgb565(rgba) {
  const out = new Uint8Array(rgba.length / 2);
  for (let i = 0, j = 0; i < rgba.length; i += 4, j += 2) {
    const pixel = ((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 2) << 5) | (rgba[i + 2] >> 3);
    out[j] = pixel >> 8; out[j + 1] = pixel & 255; // ST7789 SPI wire byte order
  }
  return out;
}
export function loadImage(url) {
  return new Promise((resolve, reject) => {
    const img = new Image(); img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('图片无法读取，请使用 PNG、JPEG 或 WebP 图片'));
    img.src = url;
  });
}
function fitText(ctx, text, x, y, maxWidth, size, weight = 400) {
  ctx.font = `${weight} ${size}px "Microsoft YaHei", sans-serif`;
  while (ctx.measureText(text).width > maxWidth && size > 11) {
    size--; ctx.font = `${weight} ${size}px "Microsoft YaHei", sans-serif`;
  }
  while (ctx.measureText(text).width > maxWidth && text.length > 1) text = text.slice(0, -2) + '…';
  ctx.fillText(text, x, y);
}
function contain(ctx, img, x, y, w, h, crop = false) {
  const scale = (crop ? Math.max : Math.min)(w / img.width, h / img.height);
  ctx.save(); ctx.beginPath(); ctx.rect(x, y, w, h); ctx.clip();
  ctx.drawImage(img, x + (w - img.width * scale) / 2, y + (h - img.height * scale) / 2, img.width * scale, img.height * scale);
  ctx.restore();
}
export async function renderPages(profile) {
  const [avatar, qr] = await Promise.all([profile.avatar ? loadImage(profile.avatar) : null, profile.qr ? loadImage(profile.qr) : null]);
  return [0, 1, 2].map(page => {
    const canvas = document.createElement('canvas'); canvas.width = 240; canvas.height = 320;
    const ctx = canvas.getContext('2d'); ctx.fillStyle = '#153c34'; ctx.fillRect(0, 0, 240, 320);
    ctx.fillStyle = '#b8d0a4'; fitText(ctx, 'FOLOTOY / IDENTITY', 18, 25, 204, 10, 600);
    ctx.fillStyle = '#f2f6e8';
    if (page === 0) {
      if (avatar) contain(ctx, avatar, 18, 46, 68, 68, true);
      else { ctx.fillStyle = '#b8d0a4'; ctx.fillRect(18, 46, 68, 68); ctx.fillStyle = '#153c34'; fitText(ctx, profile.name.slice(0, 1) || '你', 34, 93, 45, 34, 600); }
      ctx.fillStyle = '#b8d0a4'; fitText(ctx, 'HELLO,', 103, 72, 115, 13, 600); fitText(ctx, 'NICE TO MEET YOU.', 103, 93, 115, 9);
      ctx.fillStyle = '#fff'; fitText(ctx, profile.name || '你的名字', 18, 159, 204, 30, 700);
      ctx.fillStyle = '#c9d9ba'; fitText(ctx, profile.role, 18, 188, 204, 15); fitText(ctx, profile.company, 18, 212, 204, 13);
      ctx.fillStyle = '#739087'; ctx.fillRect(18, 230, 204, 1);
      ctx.fillStyle = '#c9d9ba';
      const chars = Array.from(profile.bio); let line = '', row = 0;
      ctx.font = '12px "Microsoft YaHei", sans-serif';
      for (const char of chars) { if (ctx.measureText(line + char).width > 204) { ctx.fillText(line, 18, 254 + row * 18); line = ''; row++; } if (row < 2) line += char; }
      if (row < 2) ctx.fillText(line, 18, 254 + row * 18);
    } else if (page === 1) {
      ctx.fillStyle = '#ecf1e2'; ctx.fillRect(20, 47, 200, 200);
      if (avatar) contain(ctx, avatar, 20, 47, 200, 200, true);
      else { ctx.fillStyle = '#59704e'; fitText(ctx, '上传微信头像', 69, 153, 130, 16); }
      ctx.fillStyle = '#fff'; fitText(ctx, profile.name, 20, 277, 200, 20, 600);
    } else {
      ctx.fillStyle = '#fff'; ctx.fillRect(12, 43, 216, 216);
      if (qr) { ctx.imageSmoothingEnabled = false; contain(ctx, qr, 20, 51, 200, 200); }
      else { ctx.fillStyle = '#59704e'; fitText(ctx, '上传微信二维码', 61, 145, 160, 16); fitText(ctx, '请使用微信保存的真实图片', 41, 171, 170, 12); }
      ctx.fillStyle = '#fff'; fitText(ctx, profile.wechat ? `微信：${profile.wechat}` : '扫一扫，认识一下', 18, 282, 204, 13);
    }
    ctx.fillStyle = '#739b89'; fitText(ctx, ['01 / 个人资料', '02 / 微信头像', '03 / 微信二维码'][page], 18, 307, 170, 10);
    for (let dot = 0; dot < 3; dot++) { ctx.fillStyle = dot === page ? '#cee4a6' : '#507566'; ctx.beginPath(); ctx.arc(198 + dot * 10, 303, 2, 0, Math.PI * 2); ctx.fill(); }
    return canvas;
  });
}
export function makeBundle(pages, profile) {
  const json = new TextEncoder().encode(JSON.stringify(profile));
  if (json.length > MAX_PROFILE_BYTES) throw new Error('图片内容过大，请选择更简单的图片');
  const result = new Uint8Array(PIXEL_BYTES + json.length);
  pages.forEach((canvas, i) => result.set(rgb565(canvas.getContext('2d').getImageData(0, 0, 240, 320).data), i * FRAME_BYTES));
  result.set(json, PIXEL_BYTES); return result;
}
