[简体中文](README.zh_CN.md)

# Wireless Ecology Observer

Standalone ESP32-C3 / 240 × 320 firmware built with ESP-IDF 5.5.3. References the Wi-Fi STA scan
example and reuses display/button BSP from https://github.com/folotoy/ai-passport at
`df3990726e3751fadaaaa703a480dbba6e13c61b`. License: `components/bsp/LICENSE`.

## Forest view

The UI is now a Chinese “Wireless Little Forest”, with plain-language signal descriptions instead of channel
numbers, AP counts, dBm axes, peak values, update numbers and countdowns. The explanatory hint row is removed: its 32 pixels now extend the forest from 136 to 168 pixels high, with proportionally taller trees. Tree height still represents signal
strength; channel order and colors remain internal mappings. A small arrow marks the selected tree.
Descriptions say that its signal is strong, moderate, weak or fading. Below the strength description, black text shows the selected tree's Wi-Fi name. The green environment summary and automatic-scenery message are removed. Empty scenes display
one message with a blank name row. Names follow the strongest smoothed AP in that channel, with stable
identity-based tie breaking. Supports ASCII and basic CJK, scrolls long names, labels hidden networks and
replaces unsupported or malformed characters with a question mark. Demo remains explicitly labeled and uses
synthetic names. Displaying a name does not mean the observer connects to that network.

Live passive scans run every 5 seconds without overlapping. Previous results remain visible on failures and
retry automatically. UP/DOWN select existing trees, skipping empty positions. Hold DOWN about two seconds to
switch live/demo. Demo cycles three synthetic environments every 5 seconds. OK requests a rate-limited live
scan or advances the demo immediately. There is no album, naming, stamping, Internet or AI dependency.

Each channel tree represents the strongest retained smoothed AP. Up to 24 APs are tracked. RSSI uses 3:1 smoothing,
with removal after two missed successful scans. Internal strong/moderate/weak thresholds are -55/-75 dBm;
 These details stay in documentation rather than the UI.
Physical scan coverage, movement experience and battery cost remain unverified.

## Simulator and firmware

Run `npm start`, visit http://127.0.0.1:4190 and load `public/assets/firmware/wireless-ecology.bin`, a merged
Full Flash image for offset 0x0. The simulator provides virtual Wi-Fi, not nearby physical APs. Hold DOWN to
watch the automatic demo, then select trees to read the plain-language descriptions.

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
`node tools/ecology-firmware-smoke.mjs` to check Chinese labels, automatic demo updates and tree selection.
Requires Playwright in `.toolchains/browser` and installed Chrome; override URL using `ECO_TEST_URL`.
Physical scan coverage, movement experience, display readability and battery cost of 5-second scans remain unverified.

Chinese labels are embedded. Regenerate modified copy with `python tools/generate-ecology-labels.py`, requiring Pillow and Microsoft YaHei on Windows.

SSID glyphs are embedded from `main/name_font.bin`. Regenerate with `python tools/generate-ecology-name-font.py` (Pillow and Microsoft YaHei). Tests cover peak-name association, renames, UTF-8 decoding and simulated Chinese/ASCII names.
