#!/usr/bin/env python3
"""Take real screenshots from a connected M5StickC over USB (firmware `shot` command).

  python tools/shot.py COM9 one out.png            current frame
  python tools/shot.py COM9 gallery docs/screenshots   every character x mood
  python tools/shot.py COM9 tour docs/images/tour.gif  animated tour of all characters

Needs: pip install pyserial pillow
"""
import sys
import time

import serial
from PIL import Image

CHARS = ["cat", "robot", "ghost", "cyclops"]
MOODS = ["idle", "happy", "dizzy", "scared", "sleeping"]


def open_port(port):
    s = serial.Serial(port, 115200, timeout=2)
    s.dtr = False
    s.rts = False
    time.sleep(0.3)
    s.reset_input_buffer()
    return s


def cmd(s, text, wait=0.05):
    s.write((text + "\n").encode())
    time.sleep(wait)


def grab(s):
    s.reset_input_buffer()
    cmd(s, "shot", 0.0)
    head = b""
    t0 = time.time()
    while not head.endswith(b"\n") or not head.strip().startswith(b"SHOT"):
        c = s.read(1)
        if c:
            head += c
            if head.endswith(b"\n") and not head.strip().startswith(b"SHOT"):
                head = b""  # skip log lines
        if time.time() - t0 > 5:
            raise TimeoutError("no SHOT header (is the new firmware flashed?)")
    parts = head.split()
    W, H = int(parts[1]), int(parts[2])
    n = W * H * 2
    raw = b""
    while len(raw) < n:
        chunk = s.read(n - len(raw))
        if not chunk:
            raise TimeoutError("short frame")
        raw += chunk
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        v = (raw[2 * i] << 8) | raw[2 * i + 1]  # M5GFX sprites are big-endian RGB565
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        px[i % W, i // W] = ((r * 255) // 31, (g * 255) // 63, (b * 255) // 31)
    return img


def settle(s, mood, seconds):
    cmd(s, f"mood {mood}")
    time.sleep(seconds)


def gallery(s, outdir):
    for ci, cname in enumerate(CHARS):
        cmd(s, f"char {ci}")
        time.sleep(1.8)  # pop animation + name label
        for mi, mname in enumerate(MOODS):
            settle(s, mi, 1.4 if mi != 4 else 2.5)
            grab(s).save(f"{outdir}/{cname}_{mname}.png")
            print("saved", cname, mname)
    cmd(s, "mood auto")


def tour(s, out):
    frames = []
    for ci in range(len(CHARS)):
        cmd(s, f"char {ci}")
        for mi, n in ((0, 14), (1, 10), (2, 14), (3, 6), (4, 6)):
            cmd(s, f"mood {mi}")
            if mi == 4:
                time.sleep(2.2)
            for _ in range(n):
                frames.append(grab(s).resize((160, 320), Image.NEAREST))
                time.sleep(0.04)
    cmd(s, "mood auto")
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=110, loop=0, optimize=True)
    print("saved", out, len(frames), "frames")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    ser = open_port(sys.argv[1])
    mode, target = sys.argv[2], sys.argv[3]
    if mode == "one":
        grab(ser).resize((256, 256), Image.NEAREST).save(target)
    elif mode == "gallery":
        gallery(ser, target)
    elif mode == "tour":
        tour(ser, target)
    else:
        sys.exit(__doc__)
