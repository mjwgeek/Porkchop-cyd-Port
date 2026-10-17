# Porkchop for Hosyond 4.0-inch CYD

This dedicated `hosyond-cyd40` branch targets the owner's E32R40T-style
ESP32-WROOM-32E display. It does not replace the original 2.8-inch port.

**Firmware web flasher:** https://mjwgeek.github.io/Porkchop-cyd-Port/

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
- Diagnostic display of last ESP32 reset reason and raw battery-sense ADC mV
  (GPIO34). The ADC mV are NOT calibrated battery-pack voltage and do not
  imply charger-state detection.
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
No accurate charge-status icon is possible without a charger-status IC.
SD formatting is intentionally not exposed as a one-tap destructive action.
