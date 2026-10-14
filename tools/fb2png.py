#!/usr/bin/env python3
"""Convert a raw RGB565 framebuffer dump to PNG (standard library only).

Typical use, after the firmware logged ``LCD PATTERN DRAWN``:

    STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -u 0xC0000000 0x3FC00 fb.bin
    python tools/fb2png.py fb.bin fb.png --expected

``--expected`` regenerates the M1 test screen (App/Src/lcd_test.c): the
test pattern plus the five touch-target crosshairs that the firmware draws
on top when touch init succeeded (``TOUCH TARGET`` log lines). It compares
that pixel by pixel with the dump and prints the 32-bit sum of the bare
pattern, which must match ``sum=`` in the ``LCD PATTERN DRAWN`` log line
(the firmware takes it before drawing the targets). ``--no-targets``
compares against the bare pattern, for a run where touch init failed.
"""

import argparse
import struct
import sys
import zlib


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def expected_pattern(w, h):
    """Mirror of pattern_pixel() in App/Src/lcd_test.c."""
    white = rgb565(255, 255, 255)
    bars = [rgb565(*c) for c in (
        (255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
        (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0))]
    marker = 16
    px = []
    for y in range(h):
        for x in range(w):
            if x == 0 or y == 0 or x == w - 1 or y == h - 1:
                p = white
            elif x < marker and y < marker:
                p = rgb565(255, 0, 0)
            elif x >= w - marker and y >= h - marker:
                p = rgb565(0, 0, 255)
            elif y < (h * 3) // 4:
                p = bars[(x * 8) // w]
            else:
                level = (((x * 32) // w) * 255) // 31
                p = rgb565(level, level, level)
            px.append(p)
    return px


TARGET_INSET = 20
TARGET_ARM = 10
TARGET_COUNT = 5


def touch_target(i, w, h):
    """Mirror of LcdTest_TouchTarget() in App/Src/lcd_test.c."""
    right, bottom = w - 1 - TARGET_INSET, h - 1 - TARGET_INSET
    corners = [(TARGET_INSET, TARGET_INSET), (right, TARGET_INSET),
               (TARGET_INSET, bottom), (right, bottom)]
    return corners[i] if i < len(corners) else (w // 2, h // 2)


def draw_touch_targets(px, w, h):
    """Mirror of LcdTest_DrawTouchTargets() in App/Src/lcd_test.c."""
    def fill(x0, y0, x1, y1, color):
        for y in range(max(y0, 0), min(y1, h - 1) + 1):
            for x in range(max(x0, 0), min(x1, w - 1) + 1):
                px[y * w + x] = color

    white, black, arm = rgb565(255, 255, 255), rgb565(0, 0, 0), TARGET_ARM
    for i in range(TARGET_COUNT):
        cx, cy = touch_target(i, w, h)
        fill(cx - arm, cy - 1, cx + arm, cy + 1, white)
        fill(cx - 1, cy - arm, cx + 1, cy + arm, white)
        fill(cx - arm, cy, cx + arm, cy, black)
        fill(cx, cy - arm, cx, cy + arm, black)


def write_png(path, pixels, w, h):
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter: none
        for p in pixels[y * w:(y + 1) * w]:
            r = (p >> 11) & 0x1F
            g = (p >> 5) & 0x3F
            b = p & 0x1F
            raw += bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('dump', help='raw little-endian RGB565 dump')
    ap.add_argument('png', help='output PNG path')
    ap.add_argument('--width', type=int, default=480)
    ap.add_argument('--height', type=int, default=272)
    ap.add_argument('--expected', action='store_true',
                    help='compare against the M1 test screen (pattern + touch targets)')
    ap.add_argument('--no-targets', action='store_true',
                    help='with --expected: compare against the bare pattern '
                         '(touch init failed, no TOUCH TARGET log lines)')
    args = ap.parse_args()

    w, h = args.width, args.height
    with open(args.dump, 'rb') as f:
        data = f.read()
    if len(data) < w * h * 2:
        sys.exit(f'dump too short: {len(data)} bytes, need {w * h * 2}')
    pixels = list(struct.unpack(f'<{w * h}H', data[:w * h * 2]))

    write_png(args.png, pixels, w, h)
    print(f'PNG {args.png} w={w} h={h} sum=0x{sum(pixels) & 0xFFFFFFFF:08X}')

    if args.expected:
        exp = expected_pattern(w, h)
        print(f'PATTERN sum=0x{sum(exp) & 0xFFFFFFFF:08X} (= LCD PATTERN DRAWN sum=)')
        if not args.no_targets:
            draw_touch_targets(exp, w, h)
        bad = [i for i in range(w * h) if pixels[i] != exp[i]]
        if bad:
            i = bad[0]
            print(f'COMPARE FAIL mismatches={len(bad)} first=x{i % w},y{i // w} '
                  f'exp=0x{exp[i]:04X} got=0x{pixels[i]:04X}')
            return 1
        print('COMPARE PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
