// Requires Playwright installed under .toolchains/browser and a running local server.
import { chromium } from '../.toolchains/browser/node_modules/playwright/index.mjs';
import { readFile, mkdir } from 'node:fs/promises';
const labels = JSON.parse(await readFile('firmware/wireless-ecology/test_labels.json','utf8'));
const nameFont=await readFile('firmware/wireless-ecology/main/name_font.bin');
const asciiFont=[...(await readFile('firmware/wireless-ecology/main/badge_font.h','utf8')).matchAll(/\{([\d,]+)\}/g)].map(m=>m[1].split(',').map(Number));
const browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless:true });
try {
  const page=await browser.newPage({viewport:{width:1440,height:1000}});
  await page.addInitScript(()=>localStorage.setItem('folotoy.simulator-notice.shown','1'));
  await page.goto(process.env.ECO_TEST_URL || 'http://127.0.0.1:4193');
  await page.waitForFunction(()=>!document.querySelector('#firmware-file').disabled);
  await page.locator('#firmware-file').setInputFiles('public/assets/firmware/wireless-ecology.bin');
  async function expectLabel(x,y,id,color) {
    const label=labels.find(l=>l.id===id);
    await page.waitForFunction(({x,y,label,color})=>{
      const data=document.querySelector('#qemu-display').getContext('2d').getImageData(0,0,240,320).data;
      let count=0, match=0;
      for(let row=0;row<22;row++) for(let col=0;col<label.width;col++) {
        const bit=row*label.width+col;
        if(!(label.bits[bit>>3] & (1<<(7-bit%8)))) continue;
        const p=((y+row)*240+x+col)*4; count++;
        if(color.every((c,i)=>Math.abs(data[p+i]-c)<9)) match++;
      }
      return count>0 && match/count>0.99;
    },{x,y,label,color},{timeout:60000,polling:300}).catch(async e=>{await page.screenshot({path:'artifacts/wireless-ecology/check-failure.png'});throw e;});
    console.log('PASS rendered label:',id);
  }
  async function expectName(name) {
    const points=[]; let x=16;
    for(const ch of name) {
      const cp=ch.codePointAt(0), glyph=cp>=0x4e00 && cp<=0x9fff?64+cp-0x4e00:-1;
      const w=glyph>=0?16:8;
      for(let row=0;row<20;row++) for(let col=0;col<w;col++) {
        const lit=glyph>=0?(nameFont[glyph*40+row*2+(col>>3)]&(1<<(7-col%8))):row>=3 && row<17 && (asciiFont[cp-32][row-3]&(1<<col));
        if(lit) points.push([x+col,263+row]);
      }
      x+=w;
    }
    await page.waitForFunction(points=>{
      const d=document.querySelector('#qemu-display').getContext('2d').getImageData(0,0,240,320).data;
      return points.every(([x,y])=>[19,36,44].every((c,i)=>Math.abs(d[(y*240+x)*4+i]-c)<9));
    },points,{timeout:30000});
    console.log('PASS selected Wi-Fi name:',name);
  }
  async function key(name,long=false) {
    const target=page.locator(`[data-key="${name}"]`);
    await target.hover(); await page.mouse.down(); await page.waitForTimeout(long?4500:180); await page.mouse.up();
    await page.waitForTimeout(900);
  }
  await mkdir('artifacts/wireless-ecology',{recursive:true});
  await expectLabel(16,3,'TITLE',[237,231,207]);
  await expectLabel(16,29,'LIVE',[152,206,187]);
  await page.waitForFunction(()=>{
    const d=document.querySelector('#qemu-display').getContext('2d').getImageData(0,56,240,8).data;
    for(let i=0;i<d.length;i+=4) if(![204,220,205].every((c,j)=>Math.abs(d[i+j]-c)<9)) return false;
    return true;
  },null,{timeout:30000});
  console.log('PASS former hint area is now forest sky');
  await key('DOWN',true);
  await expectLabel(16,29,'DEMO',[152,206,187]);
  await expectLabel(16,236,'STRONG',[19,36,44]);
  await expectName('森林小屋');
  const treePixels=()=>{
    const d=document.querySelector('#qemu-display').getContext('2d').getImageData(14,64,14,152).data;
    let count=0;
    for(let i=0;i<d.length;i+=4) if(d[i]<160 && d[i+1]>70 && d[i+1]<190 && d[i+2]<130) count++;
    return count;
  };
  const initial=await page.evaluate(treePixels);
  await page.waitForFunction(initial=>{
    const d=document.querySelector('#qemu-display').getContext('2d').getImageData(14,64,14,152).data;
    let count=0;
    for(let i=0;i<d.length;i+=4) if(d[i]<160 && d[i+1]>70 && d[i+1]<190 && d[i+2]<130) count++;
    return count!==initial;
  },initial,{timeout:30000});
  console.log('PASS tree changes automatically without input');
  await key('DOWN'); await expectLabel(16,236,'WEAK',[19,36,44]); await expectName('Garden-Guest');
  await key('UP'); await expectLabel(16,236,'STRONG',[19,36,44]);
  await expectLabel(16,291,'HELP',[19,36,44]);
  const data=await page.locator('#qemu-display').evaluate(c=>c.toDataURL().split(',')[1]);
  await (await import('node:fs/promises')).writeFile('artifacts/wireless-ecology/expanded-forest-view.png',Buffer.from(data,'base64'));
  await key('DOWN',true);
  await expectLabel(16,29,'LIVE',[152,206,187]);
  console.log('Simulator PASS: Chinese and ASCII Wi-Fi names, automatic landscape, selected-name association and live/demo switching.');
} finally { await browser.close(); }
