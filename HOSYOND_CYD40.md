# Porkchop for Hosyond 4.0-inch CYD

This dedicated `hosyond-cyd40` branch targets the owner's E32R40T-style
ESP32-WROOM-32E display. It does not replace the original 2.8-inch port.

**Firmware web flasher:** https://mjwgeek.github.io/Porkchop-cyd-Port/

**Firmware Releases:** https://github.com/mjwgeek/Porkchop-cyd-Port/releases

Release downloads deliberately contain just two `.bin` files; the browser's
three lower-level flash parts live separately under `flash/` in GitHub Pages.

The `Build & Publish Porkchop Hosyond 4.0` GitHub Actions workflow
compiles actual ESP32 binaries, uploads build artifacts and publishes
a Web Serial installer to the `gh-pages` branch. If the URL returns 404,
set GitHub Settings > Pages > Deploy from a branch > gh-pages / (root).

- ST7796S TFT native 320x480, presented as landscape 480x320
- Screen SPI CS15/DC2/SCLK14/MOSI13/MISO12, BL27
- XPT2046 touch CS33/IRQ36 shares screen SPI wires
- Separate SD SPI host: CLK18/MISO19/MOSI23/CS5
- RGB LED GPIO22/16/17; GPIO4 controls speaker amplifier (active low)
- Speaker PWM GPIO26; amplifier muted when silent
- No integrated GPS presumed, so UART0 is retained for USB serial
- In-settings on-screen credentials keyboard (WiGLE username/token, WPA-SEC)
- Same battery calculations as SquachWatch CYD40: eight averaged GPIO34 ADC
  readings through the 2:1 battery divider, interpolated LiPo percentage,
  no-cell detection below 2.5V, and 2-second sampling cache.
- Same SquachWatch-style animated lava-lamp battery graphic on the idle screen,
  plus compact percentage in the header and voltage/percentage in Diagnostics.
- Apparent charging is an **estimate** from rising/falling voltage, not a
  charger IC signal, and may be misleading under changing load.
- Low-memory direct display drawing to leave more heap for Wi-Fi capture

## Flash images

The browser installer uses **all three**:
`bootloader.bin` at 0x1000,
`partitions.bin` at 0x8000,
`firmware.bin` at 0x10000.

`Porkchop-Hosyond-4.0-Launcher.bin` is the **app-only** image; loading
it from an SD-card firmware launcher requires its existing partition layout
to have an app slot large enough for the build. Do not flash the **Full**
merged image through an app-only Launcher installer.

`Porkchop-Hosyond-4.0-Full.bin` is a complete merged ESP32 image for
compatible serial flash tools starting at 0x0, and can overwrite saved NVS
data. Save settings/captures first.

## Current limits

Hardware still needs hands-on validation (rotation, touch mapping, SD mounting,
audio, network operations). A compile pass is not hardware proof.
PIGGY BLUES remains stubbed; forcing the BLE stack into this Wi-Fi-heavy
build would be premature without memory and Wi-Fi coexistence tests.
The lava lamp's charging animation is based on the same voltage-trend
heuristic as SquachWatch; it is not a confirmed charger-status indication.
SD formatting is intentionally not exposed as a one-tap destructive action.

## Display startup recovery: diagnostic release

This firmware intentionally restores the exact board TFT configuration and
original (pre-shared-bus-touch) app implementation from known visibly working
commit `656048a`. The later shared-bus touchscreen implementation and
inversion changes have temporarily been backed out to isolate the solid
black/white-screen regression. It is NOT yet a touchscreen fix.

A visible startup checkpoint draws **three red/green/blue stripes** and
`PORKCHOP LCD CHECK` before SD, Wi-Fi, NVS or touch initialize; a series
of `[BOOT-PANEL]` markers at **115200 baud** identifies the point of failure.
If the checkpoint appears but the UI goes blank afterward, the follow-on
bus/driver initialization is the suspect. If it never appears, investigate
panel setup, flash image, and power before changing display inversion again.

Sound defaults OFF and its migration now correctly opens NVS writable.
On an existing preview installation the new `silent_boot_v3` key ensures
sound remains muted. Optional sound output uses much lower PWM drive.

Do NOT treat this diagnostic release as hardware-verified. It aims to restore
known-visible panel initialization first, before any further touch work.


## Experimental display recovery and OTA (test branch only)

The `hosyond-cyd40-render-test` branch **never replaces** the normal
rolling preview release or GitHub Pages. It publishes separate downloadable
experimental `.bin` files at
https://github.com/mjwgeek/Porkchop-cyd-Port/releases/tag/hosyond-cyd40-display-ota-test
and build artifacts in GitHub Actions. Merge only after
owner testing verifies red/green/blue polarity, dark background, smooth static
menus, touch, boot and network capture.

### Wi-Fi OTA from this fork

All OTA URLs are hardcoded to **mjwgeek/Porkchop-cyd-Port**:
- Manifest: https://mjwgeek.github.io/Porkchop-cyd-Port/ota.json
- App image: https://mjwgeek.github.io/Porkchop-cyd-Port/flash/firmware.bin

The GitHub workflow generates build number, image size and SHA-256 from the
**same compiled firmware** that it publishes. A manual update requires owner
confirmation on the device (Settings > check OTA > install OTA). It checks the
image size against the second OTA slot and verifies the flashed partition's
SHA-256 before reboot. A failed download does not intentionally change the
running slot. There is **no automatic installation**.

In Settings, enter Wi-Fi SSID and password with the on-screen keyboard first.
When checking for updates the app stops background Wi-Fi channel hopping,
joins that network temporarily, then disconnects and resumes reconnaissance.
The OTA button shows the local rN build number. A test-branch build will not
find its own version in the live OTA manifest until this branch is merged
and its workflow has successfully published to GitHub Pages.

### One-time full flash required

The previous `huge_app` partition table had only one application slot.
A launcher/app-only BIN **cannot** change that partition table and therefore
cannot enable OTA by itself. The first OTA-capable installation requires
flashing the FULL merged image at flash address 0x0 or the three components
(bootloader at 0x1000, partition table at 0x8000 and app at 0x10000).
**Back up saved credentials and captures first.** Changing partitions can
erase or invalidate NVS and SPIFFS contents. Do not use the Full.bin image
in an app-only SD-card launcher.

The new 4MB partition layout is:
- OTA0: 0x10000, 0x1B0000 bytes
- OTA1: 0x1C0000, 0x1B0000 bytes
- SPIFFS: 0x370000, 0x90000 bytes

Each firmware build fails CI if it exceeds the OTA slot's 0x1B0000-byte
capacity. For installations still using an old single-slot layout, the
Settings updater refuses to run and asks for the one-time full flash.

Security note: the experimental OTA client validates image size and SHA-256
from the fork's HTTPS manifest, but currently disables TLS CA verification
for compatibility with github.io. A malicious network capable of altering
both the manifest and download could bypass this integrity check.
Production use should add certificate validation or a signed update manifest.
