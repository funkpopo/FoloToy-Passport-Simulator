[简体中文](README.zh_CN.md)

# FoloToy identity badge

ESP32-C3 firmware with an embedded mobile editor and offline examples for a 240 × 320 personal badge.
The display and ADC button drivers are copied, without functional changes, from
https://github.com/folotoy/ai-passport at commit
`df3990726e3751fadaaaa703a480dbba6e13c61b` (MIT; license in `components/bsp/LICENSE`).
Only the display/button BSP sources are built. The app draws RGB565 stripes directly;
it does not start LVGL, Bluetooth, audio, or a font engine on the device.

## Use the hardware

1. Flash the three files using the commands below. First boot formats only the new
   `badge` data partition. Existing `cardid` and Recovery addresses are preserved.
2. First boot shows an offline example. Hold OK to show connection details, then join `FoloToy-Badge-XXXX` using its random password.
   Stay connected when the phone warns that the network has no Internet access.
3. Open **http://192.168.4.1** in the phone browser. Enter the same configuration
   password under the phone connection panel and press Connect.
4. Edit name, role, company, WeChat ID, bio, avatar, and an actual saved WeChat QR
   image. Save. The browser renders Chinese text and three RGB565 frames; the
   device stores and displays those frames. WeChat ID alone is not an add-friend QR.
5. UP/DOWN cycle profile → avatar → QR. Short OK toggles avatar/QR. Hold OK to
   show/hide Wi-Fi connection instructions. POWER retains its hardware behavior.

The hotspot stays on while the badge is powered. This version does not implement
automatic sleep, Bluetooth provisioning, WeChat login, or automatic profile import.
QR images are contained, not cropped. For best scanning, upload just the full QR
and its white margin. Verify scanning from the physical LCD using another phone.

## Verify the firmware in the browser simulator

Run `npm start` at the repository root and open http://127.0.0.1:4190/.
Load `public/assets/firmware/identity-badge.bin` using the firmware upload control.
Without a saved profile, the device shows three clearly marked offline examples.
UP/DOWN cycle pages; OK toggles avatar/QR; hold OK to show connection instructions,
then hold again to return. The test QR encodes `https://example.com`, not a WeChat
friend code. Saving a real profile replaces the examples.

The standalone `/badge.html` page and Node.js profile API have been removed, and
the old port 4191 demo service has been stopped. Display output now comes from the
actual firmware through emulated SPI. The virtual hotspot still cannot accept a
physical phone; mobile provisioning requires a real board.

## Build and flash

Use **ESP-IDF 5.5.3** (ESP32-C3). From this directory in an activated IDF terminal:

```text
idf.py build
idf.py merge-bin
idf.py -p COM5 flash
```

Replace COM5 with the actual device port. The default IDF flash command writes only
bootloader at `0x0`, partition table at `0x8000`, and app at `0x10000`. It does not
erase the full chip or write cardid/Recovery. A prebuilt merged image is available
at `../../public/assets/firmware/identity-badge.bin`, with the equivalent command:

```text
python -m esptool --chip esp32c3 --port COM5 write_flash 0x0 identity-badge.bin
```

The merged image ends below `0x310000`; it must **not** be padded to 8 MB when
flashing a real product, as that would overwrite factory-owned regions. Back up
any old custom app data before changing the partition table. The badge uses the
previously unallocated region `0x35a000..0x700000` for SPIFFS. Do not erase Flash.

Container build from repository root (Docker can replace Podman):

```powershell
podman run --rm -v "${PWD}:/project" -w /project/firmware/badge docker.io/espressif/idf:v5.5.3 idf.py build merge-bin
node tools/package-badge.mjs
```

`main/CMakeLists.txt` embeds the phone editor from `web/` and three sample frames
from `main/sample.rgb`. Regenerate examples with `node tools/generate-badge-sample.mjs`
(separately installed Playwright, qrcode, and Chrome are required).

## Storage and protocol

- `PUT /api/badge/bundle`: 3 × 153600 bytes of RGB565 in SPI big-endian byte order,
  followed by UTF-8 JSON (maximum 220000 bytes). All seven profile fields are strings.
- `GET /api/badge/state`: `{mode, revision, profile}`. Both requests need
  `X-Badge-Key`; uploads additionally require `If-Match` with the last revision.
- Firmware streams uploads into the inactive SPIFFS slot, validates JSON without
  allocating image strings, closes the file, then commits the generation in its
  own `id_badge` NVS namespace. Interrupted uploads leave the previous slot selected.
- LCD uses a 4800-byte DMA buffer with completion synchronization. Button callbacks
  only enqueue events. Storage and SPI operations run outside button callbacks.
- Offline examples are only shown without valid saved data; they never overwrite user storage.
- The editor accepts PNG/JPEG/WebP up to 8 MB; it rasterizes images before upload.
  There is no cloud service or external image request.

## Validation

From the root: `npm test`, `npm run build`, `npm run verify:release`.
The firmware's host tests can run with any C11 compiler:

```text
cc -std=c11 -Wall -Wextra -Werror test_protocol.c -o badge-test
./badge-test
```

`tools/badge-firmware-smoke.mjs` uses separately installed Playwright and headless
Chrome. It loads the actual firmware, presses simulator hardware buttons, compares
every rendered pixel with the embedded frames, and tests long OK and restart.
Screenshots go to `artifacts/badge-firmware-*.png`. These tests do not establish physical device
validation: LCD color/orientation, ADC gestures, hotspot behavior on iOS/Android,
QR scan reliability, and power-loss recovery still need a real board.
