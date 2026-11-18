# Publishing to M5Burner

How to share this firmware in [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/publish)
(*USER CUSTOM > Publish*, logged in with an M5Stack community account).

> **Do not use M5Burner's "Export" on your own StickC.** Export reads the whole flash from the device.
> Upload the clean `-full.bin` from the Releases page instead: it is built from source and its
> settings area (NVS) is blank.

## Fields

| Field | Value |
|---|---|
| Name | `StickC Motion Face` |
| Version | `v1.0.0` |
| Device Type | `STICKC` (original M5StickC, 80x160 screen) |
| Github | `https://github.com/ElectroIoT/M5-StickC-Motion-Face` |
| FirmWare | `M5-StickC-Motion-Face-v1.0.0-full.bin` (single 4 MB image, flashed at `0x0`) |
| Cover | [`docs/images/m5burner_cover.png`](images/m5burner_cover.png) (800 x 600) |

## Firmware description (markdown, paste as is)

```markdown
# StickC Motion Face

A little animated character lives on your **M5StickC** screen and reacts to how you move it.

## Characters
**Cat**, **Robo**, **Boo** (ghost) and **Cyclo** (cyclops)

## How it reacts
- **Tilt**: the eyes follow
- **Move**: happy, bounces
- **Shake**: dizzy, with spiral eyes and orbiting stars
- **Drop**: scared
- **Left still**: drowsy, then asleep with floating Zzz

## Buttons
- **A** (front): next character, hold for a new colour
- **B** (side): new colour

The screen also shows the character name, the mood and the battery level.
Remembers your character and colour after power off. No Wi-Fi, no setup.
For the original **M5StickC**.

Source and guide: https://github.com/ElectroIoT/M5-StickC-Motion-Face
```

## Version description (markdown, paste as is)

```markdown
## v1.0.0 - first release

- 4 animated characters: **Cat**, **Robo**, **Boo** (ghost), **Cyclo** (cyclops)
- Reacts to the motion sensor: tilt = eyes follow, move = happy, shake = dizzy, drop = scared, still = drowsy then asleep (Zzz)
- Button A = next character (pop animation), hold A or press B = new colour (6 themes)
- Info panel with character name, mood and battery level
- Remembers your character and colour after power off

Source and guide: https://github.com/ElectroIoT/M5-StickC-Motion-Face
```

## Making the image (maintainers)

```bash
pio run
python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 merge_bin \
  -o release/M5-StickC-Motion-Face-vX.Y.Z-full.bin --flash_mode keep --flash_freq keep --flash_size 4MB \
  0x1000 .pio/build/m5stickc/bootloader.bin \
  0x8000 .pio/build/m5stickc/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/m5stickc/firmware.bin
```

This firmware never touches Wi-Fi, so the image holds no personal data; the settings area (NVS) is blank.
