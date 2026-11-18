# Troubleshooting

| Problem | What to do |
|---|---|
| Screen is blank | The device has no firmware or is switched off. Flash the full image; hold the power button (left side) 2 s to switch on. |
| No serial port appears | Use a USB **data** cable. Install the FTDI VCP driver if Windows does not show a COM port. |
| Eyes look the wrong way when tilting | Flip `LOOK_X_SIGN` / `LOOK_Y_SIGN` in `platformio.ini`, rebuild. |
| Gets dizzy too easily / never | Lower or raise `SHAKE_ENERGY` (default 8) in `src/main.cpp`. |
| Always "happy" on a vibrating table | Raise `MOVE_ENERGY` (default 2.5). |
| Falls asleep too fast | Raise `SLEEP_AFTER_MS` (default 25 s). |
| Face is upside down | Change `setRotation(0)` to `setRotation(2)` (or 3) in `setup()`. |
| Battery shows 0% | Normal on USB-only power with no battery connected. |

Serial console (115200): `ver`, `char 0-3`, `theme 0-5`, `mood 0-4|auto`, `shot`.
