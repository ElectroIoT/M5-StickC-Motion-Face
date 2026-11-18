# Flashing & installing

Target: the original **M5StickC** (ESP32-PICO-D4, 4 MB flash, 80x160 screen). Tested on that model.

| File (Releases page) | Flash at | Use it for |
|---|---|---|
| `M5-StickC-Motion-Face-v1.0.0-full.bin` | `0x0` | **first install** / M5Burner - bootloader + partitions + app in one file |
| `M5-StickC-Motion-Face-v1.0.0-app.bin` | `0x10000` | **updates** - app only, keeps your saved character and colour |
| `SHA256SUMS.txt` | - | checksums |

## Option 1 - in the browser (easiest)

Open the **[web installer](https://electroiot.github.io/M5-StickC-Motion-Face/flasher/)** in Chrome or
Edge on a computer, connect the StickC, press **Install**, pick the serial port (USB Serial / FTDI).
Uses [ESP Web Tools](https://esphome.github.io/esp-web-tools/); nothing to install.

## Option 2 - M5Burner

Search "StickC Motion Face" in M5Burner (device type *STICKC*), download and burn.
(Maintainers: [M5BURNER.md](M5BURNER.md).)

## Option 3 - esptool (any OS)

```bash
pip install esptool
esptool.py --chip esp32 --port COM18 --baud 115200 write_flash 0x0 M5-StickC-Motion-Face-v1.0.0-full.bin
```

Update and keep settings:

```bash
esptool.py --chip esp32 --port COM18 --baud 115200 write_flash 0x10000 M5-StickC-Motion-Face-v1.0.0-app.bin
```

(`COM18` on Windows, `/dev/cu.usbserial-*` on macOS, `/dev/ttyUSB0` on Linux.)

## Option 4 - PlatformIO (build from source)

```bash
git clone https://github.com/ElectroIoT/M5-StickC-Motion-Face
cd M5-StickC-Motion-Face
pio run -t upload
```

## Blank screen?

A StickC with no firmware (or erased flash) shows nothing. Flash the firmware above; the screen and
backlight are switched on by the firmware through the power chip.
