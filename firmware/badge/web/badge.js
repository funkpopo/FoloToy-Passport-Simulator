import { defaults, nextPage, renderPages, loadImage, makeBundle } from './badge-render.js';
const $ = s => document.querySelector(s);
const form = $('#profile-form');
let key = new URLSearchParams(location.hash.slice(1)).get('key') || sessionStorage.getItem('badge-key') || '';
let saved = structuredClone(defaults), draft = structuredClone(defaults), pages = [], page = 0;
let revision = '', dirty = false, busy = false, polling = false, renderId = 0, connected = false;
const setStatus = (text, error = false) => { $('#status').textContent = text; $('#status').classList.toggle('error', error); };
async function api(url, options = {}) {
  const response = await fetch(`/api/badge/${url}`, { ...options, headers: { 'X-Badge-Key': key, ...options.headers }, signal: AbortSignal.timeout(45000) });
  if (!response.ok) { let reason; try { reason = (await response.json()).error; } catch {} throw new Error(reason || (response.status === 401 ? '配置密码不正确，请重新连接' : `请求失败 (${response.status})`)); }
  return response;
}
function draw() {
  if (pages[page]) $('#screen').getContext('2d').drawImage(pages[page], 0, 0);
  document.querySelectorAll('[data-page]').forEach(el => { const active = Number(el.dataset.page) === page; el.classList.toggle('active', active); el.setAttribute('aria-pressed', String(active)); });
}
async function render() { const id = ++renderId; const rendered = await renderPages(draft); if (id === renderId) { pages = rendered; draw(); } }
function markDirty(value) { dirty = value; $('#dirty').textContent = value ? '未保存的修改' : '已同步'; }
function fill() { for (const name of ['name', 'role', 'company', 'wechat', 'bio']) form.elements[name].value = draft[name] || ''; }
async function sync(force = false) {
  const response = await api('state'); const state = await response.json();
  if (busy && !force) return;
  if (!dirty || force) {
    if (revision !== state.revision || force) { saved = { ...defaults, ...state.profile }; draft = structuredClone(saved); fill(); await render(); }
    revision = state.revision;
  } else if (revision !== state.revision) setStatus('手机或另一页面更新了工牌。请先撤销本地修改，加载最新资料。', true);
  connected = true; $('#connection').textContent = '● 已连接';
  $('#mode').textContent = 'Wi-Fi · 工牌配置';
}
async function connect() {
  try {
    if (!key) throw new Error('请长按工牌确认键，并输入屏幕上的配置密码');
    await sync(true); sessionStorage.setItem('badge-key', key); history.replaceState(null, '', location.pathname); markDirty(false);
  } catch (error) { $('#connection').textContent = '○ 未连接'; $('.phone').open = true; setStatus(error.message, true); }
}
form.addEventListener('input', event => {
  if (!['name', 'role', 'company', 'wechat', 'bio'].includes(event.target.name)) return;
  draft[event.target.name] = event.target.value; markDirty(true); render().catch(e => setStatus(e.message, true));
});
async function imageData(file, type) {
  if (!['image/png', 'image/jpeg', 'image/webp'].includes(file.type) || file.size > 8 * 1024 * 1024) throw new Error('请选择 8 MB 以内的 PNG、JPEG 或 WebP 图片');
  const url = await new Promise((resolve, reject) => { const r = new FileReader(); r.onload = () => resolve(r.result); r.onerror = reject; r.readAsDataURL(file); });
  const image = await loadImage(url);
  if (image.width * image.height > 24000000) throw new Error('图片分辨率过大，请先缩小至 2400 × 2400 以内');
  const c = document.createElement('canvas'); c.width = c.height = type === 'qr' ? 400 : 240;
  const ctx = c.getContext('2d'); ctx.fillStyle = '#fff'; ctx.fillRect(0, 0, c.width, c.height);
  const scale = (type === 'qr' ? Math.min : Math.max)(c.width / image.width, c.height / image.height);
  ctx.imageSmoothingEnabled = type !== 'qr'; ctx.drawImage(image, (c.width - image.width * scale) / 2, (c.height - image.height * scale) / 2, image.width * scale, image.height * scale);
  return c.toDataURL(type === 'qr' ? 'image/png' : 'image/jpeg', .85);
}
for (const type of ['avatar', 'qr']) {
  $(`#${type}`).addEventListener('change', async event => {
    const file = event.target.files[0]; if (!file) return;
    busy = true; $('#save').disabled = true;
    try { draft[type] = await imageData(file, type); markDirty(true); page = type === 'avatar' ? 1 : 2; await render(); setStatus('图片已载入，保存后同步到工牌。'); }
    catch (error) { setStatus(error.message, true); }
    finally { busy = false; $('#save').disabled = false; event.target.value = ''; }
  });
  $(`[data-clear="${type}"]`).onclick = () => { draft[type] = ''; markDirty(true); render().catch(e => setStatus(e.message, true)); };
}
form.onsubmit = async event => {
  event.preventDefault(); if (busy) return;
  busy = true; const controls = [...form.querySelectorAll('input,button')]; controls.forEach(el => el.disabled = true);
  try {
    if (!connected) throw new Error('请先连接工牌');
    setStatus('正在生成屏幕并保存，请保持连接…');
    const snapshot = structuredClone(draft); const rendered = await renderPages(snapshot);
    const response = await api('bundle', { method: 'PUT', headers: { 'Content-Type': 'application/octet-stream', 'If-Match': revision }, body: makeBundle(rendered, snapshot) });
    revision = (await response.json()).revision; saved = snapshot; markDirty(false); setStatus('已保存，工牌资料已更新。');
  } catch (error) { setStatus(error.message, true); }
  finally { busy = false; controls.forEach(el => el.disabled = false); }
};
$('#restore').onclick = async () => { if (busy) return; try { await sync(true); markDirty(false); setStatus('已加载工牌上的资料。'); } catch (e) { setStatus(e.message, true); } };
$('#pair').onclick = () => { key = $('#pair-key').value.trim(); connect(); };
const change = action => { page = nextPage(page, action); draw(); };
$('#prev').onclick = () => change('up'); $('#next').onclick = () => change('down'); $('#ok').onclick = () => change('ok');
document.querySelectorAll('[data-page]').forEach(el => el.onclick = () => { page = Number(el.dataset.page); draw(); });
document.addEventListener('keydown', event => {
  if (event.target.closest('input,textarea,button,summary') || event.altKey || event.ctrlKey || event.metaKey) return;
  const action = { ArrowUp: 'up', ArrowDown: 'down', Enter: 'ok' }[event.key];
  if (action) { event.preventDefault(); change(action); }
});
window.addEventListener('beforeunload', event => { if (dirty) { event.preventDefault(); event.returnValue = ''; } });
fill(); await render(); await connect();
setInterval(async () => {
  if (!key || busy || polling || document.hidden) return;
  polling = true;
  try { await sync(); } catch { connected = false; $('#connection').textContent = '○ 连接中断'; }
  finally { polling = false; }
}, 3000);
