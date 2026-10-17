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

## October 17 screen, touch and audio corrections

Hosyond's XPT2046 shares the LCD SPI bus. The firmware now reads raw touch
pressure/coordinates through TFT_eSPI, without starting a second SPI peripheral
on the same pins. Raw readings are mapped through the native portrait frame
and the current rotation (1 or 3). Touch calibration/orientation require
verification on real hardware.

The firmware calls `invertDisplay(false)` at the panel's runtime setup and
after rotation: on the tested screen `invertDisplay(true)` produced white
backgrounds with pink text rather than the expected dark UI.

Sound now **defaults OFF even for existing preview installations** using a
one-time NVS migration. Owners may turn it back on in Settings; it then uses
low-duty PWM (not the original loud 25-50% output), defaults to level 24/255,
and gates the NS4168 amplifier once per sound sequence instead of pulsing
its enable pin for every short beep. These changes aim to reduce screen
flashes caused by amp startup and current draw. They do not prove that an
underpowered USB supply or hardware power-rail sag cannot still cause flashes.
