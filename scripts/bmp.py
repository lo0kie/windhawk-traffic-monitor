#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BMP / PNG 小工具：看尺寸、裁切、放大、拼对比图、统计覆盖率。

调试渲染效果时反复用到的几件事都在这儿，不依赖 Pillow：

    python scripts/bmp.py info .temp/dump/bar.bmp
    python scripts/bmp.py png  .temp/dump/bar.bmp docs/taskbar.png
    python scripts/bmp.py crop .temp/dump/bar.bmp .temp/dump/x.bmp 1740 8 320 50
    python scripts/bmp.py crop .temp/dump/bar.bmp .temp/dump/zoom.bmp 0 6 70 22 --scale 8
    python scripts/bmp.py stack .temp/dump/ab.bmp 6 a.bmp b.bmp c.bmp
    python scripts/bmp.py ink  .temp/dump/render.bmp --bg 200

32 位 BMP 的 alpha 有两种来源，默认自动判断：整张图 alpha 全为 0 时（GDI BitBlt /
PrintWindow 的常见输出）当作不透明处理，否则按 alpha 合成到背景色上。
"""

from __future__ import print_function

import argparse
import os
import struct
import sys
import zlib

WHITE = (255, 255, 255)


# ---------------------------------------------------------------------------
# 读
# ---------------------------------------------------------------------------
def read_bmp(path):
    """返回 (width, height, bpp, rows)，rows 是自上而下的 RGBA bytearray 列表。"""
    data = open(path, 'rb').read()
    if data[:2] != b'BM':
        raise SystemExit('%s 不是 BMP' % path)
    offset = struct.unpack_from('<I', data, 10)[0]
    header_size = struct.unpack_from('<I', data, 14)[0]
    width = struct.unpack_from('<i', data, 18)[0]
    height_raw = struct.unpack_from('<i', data, 22)[0]
    bpp = struct.unpack_from('<H', data, 28)[0]
    compression = struct.unpack_from('<I', data, 30)[0]
    if header_size < 40 or compression not in (0, 3):
        raise SystemExit('%s：只支持未压缩的 BMP（BI_RGB / BI_BITFIELDS）' % path)

    height = abs(height_raw)
    top_down = height_raw < 0
    stride = ((width * bpp // 8) + 3) // 4 * 4

    palette = []
    if bpp == 8:
        palette_offset = 14 + header_size
        for i in range(256):
            b, g, r = data[palette_offset + i * 4:palette_offset + i * 4 + 3]
            palette.append((r, g, b))

    rows = []
    for row in range(height):
        src = row if top_down else height - 1 - row
        base = offset + src * stride
        line = bytearray(width * 4)
        if bpp == 32:
            line[:] = data[base:base + width * 4]
            # BGRA -> RGBA
            line[0::4], line[2::4] = line[2::4], line[0::4]
        elif bpp == 24:
            for x in range(width):
                b, g, r = data[base + x * 3:base + x * 3 + 3]
                line[x * 4:x * 4 + 4] = bytes((r, g, b, 255))
        elif bpp == 8:
            for x in range(width):
                r, g, b = palette[data[base + x]]
                line[x * 4:x * 4 + 4] = bytes((r, g, b, 255))
        else:
            raise SystemExit('%s：不支持 %d 位 BMP' % (path, bpp))
        rows.append(line)
    return width, height, bpp, rows


def has_alpha(rows):
    return any(px for row in rows for px in row[3::4])


def flatten(rows, bg=WHITE):
    """合成到背景色上，返回 RGB（每像素 3 字节）行列表。

    整张图 alpha 全为 0 时（GDI BitBlt 的输出就是这样）当作不透明，直接用原色。
    """
    use_alpha = has_alpha(rows)
    out = []
    for row in rows:
        line = bytearray(len(row) // 4 * 3)
        for i in range(0, len(row), 4):
            r, g, b, a = row[i], row[i + 1], row[i + 2], row[i + 3]
            if not use_alpha:
                a = 255
            if a == 0:
                r, g, b = bg
            elif a != 255:
                # 分层窗口的输出是预乘 alpha，先还原再叠背景
                r, g, b = r * 255 // a, g * 255 // a, b * 255 // a
                r = (r * a + bg[0] * (255 - a)) // 255
                g = (g * a + bg[1] * (255 - a)) // 255
                b = (b * a + bg[2] * (255 - a)) // 255
            line[i // 4 * 3:i // 4 * 3 + 3] = bytes((min(r, 255), min(g, 255),
                                                     min(b, 255)))
        out.append(line)
    return out


# ---------------------------------------------------------------------------
# 写
# ---------------------------------------------------------------------------
def write_bmp(path, width, height, rgb_rows, bpp=24):
    stride = ((width * bpp // 8) + 3) // 4 * 4
    payload = bytearray()
    for row in reversed(rgb_rows):          # BMP 默认自下而上
        line = bytearray()
        for x in range(width):
            r, g, b = row[x * 3:x * 3 + 3]
            line += bytes((b, g, r)) if bpp == 24 else bytes((b, g, r, 0))
        payload += line + b'\x00' * (stride - len(line))

    info = struct.pack('<IiiHHIIiiII', 40, width, height, 1, bpp, 0, len(payload),
                       0, 0, 0, 0)
    header = struct.pack('<2sIHHI', b'BM', 14 + 40 + len(payload), 0, 0, 54)
    open(path, 'wb').write(header + info + payload)


def write_png(path, width, height, rows, alpha=False):
    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload +
                struct.pack('>I', zlib.crc32(tag + payload) & 0xFFFFFFFF))

    if alpha:
        raw = b''.join(b'\x00' + bytes(row) for row in rows)
        color_type = 6
    else:
        raw = b''.join(b'\x00' + bytes(row) for row in rows)
        color_type = 2

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, color_type, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def save(path, width, height, rgb_rows, keep_alpha_rows=None):
    """按扩展名决定写 BMP 还是 PNG；有 RGBA 行且允许时写带 alpha 的 PNG。"""
    ext = os.path.splitext(path)[1].lower()
    if ext == '.png':
        if keep_alpha_rows is not None:
            write_png(path, width, height, keep_alpha_rows, alpha=True)
        else:
            write_png(path, width, height, rgb_rows)
    elif ext == '.bmp':
        write_bmp(path, width, height, rgb_rows)
    else:
        raise SystemExit('不认识的输出扩展名：%s（用 .bmp 或 .png）' % path)


# ---------------------------------------------------------------------------
# 变换
# ---------------------------------------------------------------------------
def crop(width, height, rows, box):
    x, y, w, h = box
    x = max(0, min(x, width))
    y = max(0, min(y, height))
    w = min(w, width - x)
    h = min(h, height - y)
    if w <= 0 or h <= 0:
        raise SystemExit('裁切区域为空：%s' % (box,))
    return [row[x * 4:(x + w) * 4] for row in rows[y:y + h]]


def scale_rows(width, height, rows, factor):
    """最近邻放大（整数倍），够看清像素边缘。"""
    if factor <= 1:
        return width, height, rows
    out = []
    for row in rows:
        line = bytearray()
        for x in range(width):
            px = row[x * 4:x * 4 + 4]
            line += px * factor
        for _ in range(factor):
            out.append(bytearray(line))
    return width * factor, height * factor, out


# ---------------------------------------------------------------------------
# 统计
# ---------------------------------------------------------------------------
def ink_stats(rows, bg=200):
    """把灰度图当覆盖率看：总墨量、实心像素、半透明像素。

    用来比较抗锯齿 / 锐化的效果——半透明像素相对实心像素越少，笔画越“实”。
    """
    ink = 0.0
    full = 0
    partial = 0
    levels = set()
    for row in rows:
        for i in range(0, len(row), 4):
            alpha = 255.0 - row[i] * 255.0 / bg
            alpha = max(0.0, min(255.0, alpha))
            ink += alpha / 255.0
            levels.add(row[i])
            if alpha > 250:
                full += 1
            elif alpha > 5:
                partial += 1
    return {
        'ink': ink,
        'full': full,
        'partial': partial,
        'levels': len(levels),
        'ratio': partial / full if full else 0.0,
    }


# ---------------------------------------------------------------------------
# 命令行
# ---------------------------------------------------------------------------
def _parse_color(text):
    text = text.strip().lstrip('#')
    if len(text) != 6:
        raise SystemExit('颜色要写成 RRGGBB：%s' % text)
    return tuple(int(text[i:i + 2], 16) for i in (0, 2, 4))


def _load_as_rgb(path, bg):
    width, height, bpp, rows = read_bmp(path)
    if bpp == 32 and has_alpha(rows):
        return width, height, flatten(rows, bg), rows
    return width, height, flatten(rows, bg), None


def cmd_info(args):
    for path in args.files:
        if path.lower().endswith('.png'):
            with open(path, 'rb') as f:
                head = f.read(26)
            if head[:8] != b'\x89PNG\r\n\x1a\n':
                raise SystemExit('%s 不是 PNG' % path)
            width, height = struct.unpack('>II', head[16:24])
            depth, color = head[24], head[25]
            channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color, 0)
            print('%s  %dx%d  %d bpp  %.1f KB' % (
                path, width, height, depth * channels, os.path.getsize(path) / 1024.0))
            continue
        width, height, bpp, rows = read_bmp(path)
        extra = '，带 alpha' if bpp == 32 and has_alpha(rows) else ''
        print('%s  %dx%d  %d bpp%s  %.1f KB' % (
            path, width, height, bpp, extra, os.path.getsize(path) / 1024.0))


def cmd_png(args):
    bg = _parse_color(args.bg)
    width, height, bpp, rows = read_bmp(args.src)
    if args.keep_alpha and bpp == 32 and has_alpha(rows):
        # 原样保留 alpha（颜色先反预乘，PNG 存的是非预乘值）
        out = []
        for row in rows:
            line = bytearray(row)
            for i in range(0, len(line), 4):
                a = line[i + 3]
                if 0 < a < 255:
                    for k in range(3):
                        line[i + k] = min(255, line[i + k] * 255 // a)
            out.append(line)
        save(args.dst, width, height, None, keep_alpha_rows=out)
    else:
        save(args.dst, width, height, flatten(rows, bg))
    print('saved %s (%dx%d)' % (args.dst, width, height))


def cmd_crop(args):
    bg = _parse_color(args.bg)
    width, height, rgb, _ = _load_as_rgb(args.src, bg)
    rgb = crop(width, height, rgb, (args.x, args.y, args.w, args.h))
    width, height = args.w, args.h
    if args.scale > 1:
        width, height, rgb = scale_rows(width, height, rgb, args.scale)
    save(args.dst, width, height, rgb)
    print('saved %s (%dx%d)' % (args.dst, width, height))


def cmd_stack(args):
    bg = _parse_color(args.bg)
    tiles = []
    target = args.width
    for path in args.files:
        width, height, rgb, _ = _load_as_rgb(path, bg)
        if args.crop:
            rgb = crop(width, height, rgb, tuple(args.crop))
            width, height = args.crop[2], args.crop[3]
        if args.scale > 1:
            width, height, rgb = scale_rows(width, height, rgb, args.scale)
        tiles.append((width, height, rgb))

    if target <= 0:
        target = max(t[0] for t in tiles)
    gap = bytes([0x40]) * (3 * target)

    out = []
    for index, (width, height, rgb) in enumerate(tiles):
        for row in rgb:
            line = bytearray(row)
            if width < target:                      # 右侧补背景
                line += bytes([0x40]) * (3 * (target - width))
            out.append(line)
        if index != len(tiles) - 1 and args.gap > 0:
            out.extend([bytearray(gap)] * args.gap)

    save(args.dst, target, len(out), out)
    print('saved %s (%dx%d，%d 张)' % (args.dst, target, len(out), len(tiles)))


def cmd_ink(args):
    # --bg 可以写两位灰度（C8）或六位颜色（RRGGBB），取第一个通道当灰度基准
    level = int(args.bg, 16) if len(args.bg.strip()) <= 2 else _parse_color(args.bg)[0]
    for path in args.files:
        width, height, bpp, rows = read_bmp(path)
        stats = ink_stats(flatten(rows), level)
        print('%-36s %dx%d  墨量 %7.1f  实心 %5d  半透明 %5d  半透明/实心 %.2f  '
              '灰阶 %d' % (os.path.basename(path), width, height, stats['ink'],
                          stats['full'], stats['partial'], stats['ratio'],
                          stats['levels']))


def main():
    parser = argparse.ArgumentParser(description='BMP / PNG 查看与处理')
    sub = parser.add_subparsers(dest='cmd')

    p = sub.add_parser('info', help='看尺寸和位深')
    p.add_argument('files', nargs='+')
    p.set_defaults(func=cmd_info)

    p = sub.add_parser('png', help='转成 PNG')
    p.add_argument('src')
    p.add_argument('dst')
    p.add_argument('--keep-alpha', action='store_true', help='保留 32 位 alpha')
    p.add_argument('--bg', default='FFFFFF', help='合成用的背景色，默认 FFFFFF')
    p.set_defaults(func=cmd_png)

    p = sub.add_parser('crop', help='裁切（可顺便整数倍放大）')
    p.add_argument('src')
    p.add_argument('dst')
    p.add_argument('x', type=int)
    p.add_argument('y', type=int)
    p.add_argument('w', type=int)
    p.add_argument('h', type=int)
    p.add_argument('--scale', type=int, default=1)
    p.add_argument('--bg', default='FFFFFF')
    p.set_defaults(func=cmd_crop)

    p = sub.add_parser('stack', help='多张图竖着拼成一张对比图')
    p.add_argument('dst')
    p.add_argument('files', nargs='+')
    p.add_argument('--scale', type=int, default=1)
    p.add_argument('--gap', type=int, default=8, help='图之间空几像素')
    p.add_argument('--crop', type=int, nargs=4, metavar=('X', 'Y', 'W', 'H'))
    p.add_argument('--width', type=int, default=0, help='目标宽度，默认取最宽的')
    p.add_argument('--bg', default='FFFFFF')
    p.set_defaults(func=cmd_stack)

    p = sub.add_parser('ink', help='统计墨量与半透明像素（比较锐化效果）')
    p.add_argument('files', nargs='+')
    p.add_argument('--bg', default='C8', help='渲染底色的灰度，默认 C8（200）')
    p.set_defaults(func=cmd_ink)

    args = parser.parse_args()
    if not getattr(args, 'func', None):
        parser.print_help()
        return 1
    args.func(args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
