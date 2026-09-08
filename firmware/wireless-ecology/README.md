[简体中文](README.zh_CN.md)

# Wireless Ecology Observer

Standalone ESP32-C3 / 240 × 320 firmware built with ESP-IDF 5.5.3. References the Wi-Fi STA scan
example and reuses display/button BSP from https://github.com/folotoy/ai-passport at
`df3990726e3751fadaaaa703a480dbba6e13c61b`. License: `components/bsp/LICENSE`.

## Continuous observation

Boot automatically starts passive 2.4 GHz scans every 5 seconds. Scans never overlap; if a scan
takes longer, the next starts after completion. Previous results remain visible during scanning
and errors, with automatic retries. AUTO 5S, update number and scan status/countdown expose refresh activity.
Stamping, naming, the album and its NVS reads/writes have been removed. Old album data is not erased.

Channels 1–14 occupy fixed left-to-right columns. Each channel has one tree representing the strongest
smoothed RSSI among retained APs. Trees share shape, width, baseline and scale. Reference lines show
-30, -60 and -90 dBm; stronger signals grow taller. Heights map -95 through -30 dBm to 8–60 logical pixels,
clamped at the endpoints. Channels 1–4 are green, 5–9 amber and 10–14 blue, matching the ground strip.
The AP row shows retained AP counts per channel. A highlighted column and bottom detail show the selected
channel's count and PEAK dBm. Up to 24 APs are tracked; the top AP count is the full scan total, so channel
statistics represent a subset when the limit is exceeded.

BSSID hashes identify APs and subsequent RSSI uses 3:1 smoothing. APs survive one missed scan and disappear
after two successful scans omit them. A channel fades and loses its creature when all its retained APs
have missed one scan. Counts and peak values can include one scan of stale data. The visualization
cannot measure people or accurately measure channel congestion.

| Button | Action |
| --- | --- |
| UP / arrow up | Previous channel, wrapping |
| DOWN / arrow down | Next channel, wrapping |
| Hold DOWN about 2 seconds | Switch live / DEMO SYNTHETIC |
| OK / Enter | Request live scan within the 5-second rate limit; advance demo habitat immediately |

Demo mode automatically cycles three synthetic habitats every 5 seconds. Live and demo scenes are
independent; an in-flight live scan finishes when switching to demo. No Internet or AI service is needed.

Trees use a pixel pine silhouette with 2–4 tiers of spreading boughs, shaded foliage, brown trunks and flared roots. The tip still corresponds to signal height on the shared scale.

## Simulator and firmware

Run `npm start` at the repository root, visit http://127.0.0.1:4190 and load
`public/assets/firmware/wireless-ecology.bin`, a Full Flash merged image for offset 0x0.
The simulator provides virtual Wi-Fi rather than nearby physical APs. First leave buttons untouched
and check increasing update numbers; then hold DOWN and observe automatic habitat changes. Select
channels with UP/DOWN and inspect PEAK values.

## Build and checks

From the root (Docker may replace Podman):

```powershell
podman run --rm -v "${PWD}:/project" -w /project/firmware/wireless-ecology docker.io/espressif/idf:v5.5.3 idf.py build merge-bin
node tools/package-ecology.mjs
gcc -std=c11 -Wall -Wextra -Werror firmware/wireless-ecology/test_model.c -o .toolchains/ecology-test.exe
& .toolchains/ecology-test.exe
```

Alternatively activate local IDF and run `idf.py build merge-bin` in the firmware directory.
Packaged images and SHA-256 manifest are in `artifacts/wireless-ecology/`. Preserves the upstream
3 MiB app limit, cardid at 0x356000 and Recovery at 0x700000. No 8 MiB padding, old NVS erasure or factory writes.

Rendering uses a 38,400-byte framebuffer and 4,800-byte DMA stripe. Radio and UI communicate through queues;
button callbacks do not scan or draw. Host tests cover smoothing, missed scans, channel summaries, peak
selection, horizontal positioning and height limits. With a server on port 4193, run
`node tools/ecology-firmware-smoke.mjs` to check automatic live scans, demo updates and channel selection.
Requires Playwright in `.toolchains/browser` and installed Chrome; override URL using `ECO_TEST_URL`.
Physical scan coverage, movement experience, display readability and battery cost of 5-second scans remain unverified.
