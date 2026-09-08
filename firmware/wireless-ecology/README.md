[简体中文](README.zh_CN.md)

# Wireless Ecology Observer

Standalone ESP32-C3 firmware for the 240 × 320 FoloToy AI Passport, built with ESP-IDF 5.5.3.
Implements concept 4 in the root ADVICES.md. References the Wi-Fi STA scan example and
reuses display/button BSP from https://github.com/folotoy/ai-passport at
`df3990726e3751fadaaaa703a480dbba6e13c61b`; license: `components/bsp/LICENSE`.
RGB565 pixel graphics require no Internet, AI, LVGL or remote assets.

## Simulator testing

Run `npm start` at the repository root, visit http://127.0.0.1:4190 and load
`public/assets/firmware/wireless-ecology.bin`. This is a merged Full Flash image for offset 0x0,
not the application-only binary. Boot starts in `LIVE / 2.4 GHZ`.
The simulator exposes virtual Wi-Fi, not nearby physical access points.
**Hold DOWN for about two seconds to enter `DEMO / SYNTHETIC`** with three artificial habitats.

| Screen | Button | Action |
| --- | --- | --- |
| Field | UP / arrow up | Open album |
| Live field | DOWN / arrow down | Scan again, minimum interval 5 seconds |
| Demo field | DOWN | Cycle three synthetic habitats |
| Field | Hold DOWN | Switch live/demo |
| Field | OK / Enter | Freeze a snapshot and choose its name |
| Naming | UP / DOWN | Choose one of eight names |
| Naming | OK | Save stamp to Flash |
| Naming | Hold OK | Cancel |
| Album | DOWN | Next stamp |
| Album | OK | Rename stamp |
| Album | UP | Return to field |

Suggested walkthrough: boot, hold DOWN, cycle habitats, name and save a stamp, open the album,
browse or rename, return to field, switch back to live. Six snapshots are retained;
the seventh replaces the oldest slot. Snapshots preserve trees, terrain, name and live/demo provenance.
Hardware stores the album in NVS. Persistence across simulator restarts depends on whether
the simulator retains mutated Flash; uploading the original binary resets the album.

## Mapping and limits

BSSID hashes stabilize location, species and creatures across scan ordering. Stronger RSSI
produces taller trees (8–38 logical pixels), with 3:1 smoothing on later scans. Channels 1–4
map to green terrain, 5–9 to amber and 10–14 to blue, with an on-screen legend.
Each tree has a pixel creature. One missed successful scan fades a tree; two remove it.
Failed scans preserve the previous landscape. The +/- counters show the latest additions/removals.
Total AP count is displayed, with at most 24 trees including those retained after one missed scan.

Live mode performs passive scans every 30 seconds. Demo, naming and album screens pause automatic
scans; an in-flight scan may finish. Live and synthetic scenes are independent.
This is an artistic mapping, not a congestion meter or people counter. ESP32-C3 supports 2.4 GHz only.
Naming currently offers eight English presets, not free text. Power consumption, scan coverage and
the experience of moving between locations require physical hardware testing.

## Build and validation

From the repository root (Docker may replace Podman):

```powershell
podman run --rm -v "${PWD}:/project" -w /project/firmware/wireless-ecology docker.io/espressif/idf:v5.5.3 idf.py build merge-bin
node tools/package-ecology.mjs
gcc -std=c11 -Wall -Wextra -Werror firmware/wireless-ecology/test_model.c -o .toolchains/ecology-test.exe
& .toolchains/ecology-test.exe
```

Alternatively activate ESP-IDF 5.5.3 and run `idf.py build merge-bin` in this directory.
The packaging script checks partition MD5, binary contents and protected offsets, writes images
and a SHA-256 manifest to `artifacts/wireless-ecology/`, and copies the loadable image to
`public/assets/firmware/`. Preserves the upstream 3 MiB application limit, cardid at 0x356000
and Recovery at 0x700000. The image is not padded to 8 MiB and does not write factory partitions.
NVS initialization failure never triggers automatic erasure of existing data.

Rendering uses a 38,400-byte logical framebuffer and a 4,800-byte DMA stripe, reused only after
SPI completion. Button callbacks enqueue events, radio scans run in a worker, and the main task
serializes rendering and NVS writes. Host tests cover identity, smoothing, transient disappearance,
reappearance, capacity and RSSI mapping boundaries. Builds and simulator checks do not validate hardware.

Optional simulator check: start a local server on port 4193 and run `node tools/ecology-firmware-smoke.mjs`
(requires Playwright under `.toolchains/browser` and installed Chrome; override the URL with `ECO_TEST_URL`).
It checks rendered firmware text and buttons for boot, demo habitats, naming, NVS save, album and rename.
