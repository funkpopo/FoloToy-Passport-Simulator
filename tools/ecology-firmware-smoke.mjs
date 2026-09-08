// Requires Playwright installed under .toolchains/browser and a running local server.
import { chromium } from '../.toolchains/browser/node_modules/playwright/index.mjs';
import { readFile, mkdir } from 'node:fs/promises';
const font = [...(await readFile('firmware/wireless-ecology/main/badge_font.h','utf8')).matchAll(/\{([\d,]+)\}/g)].map(m=>m[1].split(',').map(Number));
const browser = await chromium.launch({ executablePath: process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless:true });
try {
  const page=await browser.newPage({viewport:{width:1440,height:1000}});
  await page.addInitScript(()=>localStorage.setItem('folotoy.simulator-notice.shown','1'));
  await page.goto(process.env.ECO_TEST_URL || 'http://127.0.0.1:4193');
  await page.waitForFunction(()=>!document.querySelector('#firmware-file').disabled);
  await page.locator('#firmware-file').setInputFiles('public/assets/firmware/wireless-ecology.bin');
  async function expectText(x,y,text,color) {
    await page.waitForFunction(({x,y,text,color,font})=>{
      const canvas=document.querySelector('#qemu-display');
      const data=canvas.getContext('2d').getImageData(0,0,240,320).data;
      let count=0, match=0;
      for(let a=0;a<text.length;a++) for(let row=0;row<14;row++) for(let b=0;b<8;b++) {
        if(!(font[text.charCodeAt(a)-32][row] & (1<<b))) continue;
        const p=((y+row)*240+x+a*8+b)*4; count++;
        if(color.every((c,i)=>Math.abs(data[p+i]-c)<9)) match++;
      }
      return count>0 && match/count>0.99;
    },{x,y,text,color,font},{timeout:60000,polling:300});
    console.log('PASS rendered text:',text);
  }
  async function key(name,long=false) {
    const target=page.locator(`[data-key="${name}"]`);
    await target.hover(); await page.mouse.down(); await page.waitForTimeout(long?3000:180); await page.mouse.up();
    await page.waitForTimeout(900);
  }
  await mkdir('artifacts/wireless-ecology',{recursive:true});
  await expectText(12,8,'WIRELESS ECOLOGY',[237,231,207]);
  await expectText(12,30,'FIELD  LIVE / 2.4 GHZ',[152,206,187]);
  await key('DOWN',true);
  await expectText(12,30,'FIELD  DEMO / SYNTHETIC',[152,206,187]);
  await page.locator('#qemu-display').screenshot({path:'artifacts/wireless-ecology/demo.png'});
  await key('DOWN');
  await expectText(8,298,'DEMO HABITAT 2 / 3',[19,36,44]);
  await key('OK');
  await expectText(20,110,'NAME YOUR STAMP',[237,231,207]);
  await key('DOWN');
  await expectText(20,135,'AMBER GROVE',[247,218,151]);
  await key('OK');
  await expectText(8,298,'STAMP SAVED!',[19,36,44]);
  await key('UP');
  await expectText(12,30,'ALBUM  DEMO / SYNTHETIC',[152,206,187]);
  await expectText(8,258,'STAMP 1/1 AMBER GROVE',[19,36,44]);
  await page.locator('#qemu-display').screenshot({path:'artifacts/wireless-ecology/album.png'});
  await key('OK'); await key('DOWN'); await key('OK');
  await expectText(8,258,'STAMP 1/1 TIDAL FOREST',[19,36,44]);
  await key('UP'); await key('DOWN',true);
  await expectText(12,30,'FIELD  LIVE / 2.4 GHZ',[152,206,187]);
  console.log('Simulator PASS: boot, live/demo, habitat cycling, naming, NVS save, album, rename, return to live.');
} finally { await browser.close(); }
