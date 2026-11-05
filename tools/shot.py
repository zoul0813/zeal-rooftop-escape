#!/usr/bin/env python3
"""Screenshot a Rooftop Escape build without a window: boots it in zeal-native's console debugger,
lets it run (an AUTOPILOT build plays by itself), stops at the next sprite upload and rebuilds the
frame from RAM: the skyline ring (assets/legs/sky.tmx) at its parallax scroll, the world buffer at the
camera, and the sprite table. Not a VRAM capture (lightning flashes and palette changes outside the
sky are not shown), but the same tiles, scroll and sprites the game hands the Video Board.

Usage: shot.py <rooftop.bin> <out.png> [seconds after exec ...] [--enter] [--scale 2]
Several seconds give several shots (out-<s>.png). The .map and rooftop.zar must sit next to the .bin.
Needs Pillow.
"""
import argparse
import os
import re
import subprocess
from PIL import Image
from tmx_helpers import Map, load_maps, ROOT as MAP_ROOT

import shlex
from emulator import add_arguments, resolve

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ap = argparse.ArgumentParser()
ap.add_argument('bin')
ap.add_argument('out')
ap.add_argument('seconds', nargs='*', type=float, default=[8.0])
ap.add_argument('--enter', action='store_true', help='press Enter on the title (non-AUTOPILOT builds)')
ap.add_argument('--scale', type=int, default=2)
add_arguments(ap)
a = ap.parse_args()
resolve(a, ap)

binp = os.path.abspath(a.bin)
sym = {}
for l in open(binp[:-4] + '.map'):
    m = re.match(r'\s+([0-9A-F]{8})\s+(_\w+)\s', l)
    if m:
        sym[m.group(2)] = int(m.group(1), 16)
T = 10_000_000
RA = sym['_gfx_sprite_render_array']


def typed(text):
    keys = []
    for ch in text:
        keys += {' ': ['tap SPACE'], ':': ['press SHIFT', 'tap SEMICOLON', 'release SHIFT'], '/': ['tap SLASH'],
                 '.': ['tap PERIOD']}.get(ch, ['tap ' + ch.upper()])
        keys.append('run 200000')
    return keys + ['tap ENTER', 'run 2000000']


def grab(seconds):
    cmds = ['run 30000000'] + typed('cd h:/') + typed('exec h:/' + os.path.basename(binp))
    if a.enter:
        cmds += ['run %d' % (3 * T), 'press ENTER', 'run 1000000', 'release ENTER']
    cmds += ['run %d' % int(seconds * T), 'bp 0x%x' % RA, 'c', 'regs']
    cmds += ['rb 0x%x 256' % ad for ad in range(0x8000, 0x10000, 256)] + ['quit']
    p = subprocess.run([a.emu, '-r', a.rom, '-H', os.path.dirname(binp), '-o', '-g'] + shlex.split(a.args),
                       input='\n'.join(cmds) + '\n', capture_output=True, text=True, timeout=1200)
    ram = bytearray(0x10000)
    for ad, b in re.findall(r'\[DBG\] ([0-9a-f]{4}): ((?:[0-9a-f]{2} ?)+)', p.stdout):
        ad = int(ad, 16)
        for i, x in enumerate(b.split()):
            ram[ad + i] = int(x, 16)
    sp = int(re.findall(r'SP=([0-9a-f]{4})', p.stdout)[-1], 16)
    return ram, sp


def u16(ram, ad):
    return ram[ad] | ram[ad + 1] << 8


def s16(ram, ad):
    v = u16(ram, ad)
    return v - 0x10000 if v & 0x8000 else v


tsp = Image.open(os.path.join(ROOT, 'assets', 'tileset.gif'))
pal = tsp.getpalette()[:768]
tiles = [tsp.crop(((t % 16) * 16, (t // 16) * 16, (t % 16) * 16 + 16, (t // 16) * 16 + 16)).tobytes()
         for t in range(256)] if tsp.height >= 256 else None
if tiles is None:
    cols = tsp.width // 16
    tiles = []
    for t in range(256):
        tx, ty = (t % cols) * 16, (t // cols) * 16
        tiles.append(tsp.crop((tx, ty, tx + 16, ty + 16)).tobytes() if ty < tsp.height else bytes(256))
sky_ring = bytes(g - 1 if g else 0 for g in Map(MAP_ROOT/'assets/legs/sky.tmx', (80, 15)).gids)
sky_pals, moon_pals = [], []
for l in open(os.path.join(ROOT, 'src', 'sky.h')):
    m = re.findall(r'0x([0-9A-F]{4})', l)
    if len(m) in (2, 6):
        (sky_pals if len(m) == 6 else moon_pals).append([int(x, 16) for x in m])
MOON_COLOR = int(re.search(r'#define MOON_COLOR (\d+)', open(os.path.join(ROOT, 'src', 'sky.h')).read()).group(1))
legs_sky = [m.props['sky'] for m in load_maps('legs/leg[0-9].tmx', 'LEG_COUNT')]
SKY_C = int(re.search(r'#define SKY_C\s+(\d+)', open(os.path.join(ROOT, 'src', 'gfx.h')).read()).group(1))


def render(ram, sp):
    world = u16(ram, sym['_world'])
    camx = s16(ram, sym['_cam'])
    base = ram[sym['_course_base']]
    leg, state = ram[sym['_game'] + 2], ram[sym['_game'] + 6]
    p = list(pal)
    step = legs_sky[leg] if state not in (4,) else len(sky_pals) - 3
    for i, c in list(enumerate(sky_pals[step])) + [(MOON_COLOR + j, c) for j, c in enumerate(moon_pals[step])]:
        p[i * 3:i * 3 + 3] = [((c >> 11) & 31) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3]
    img = Image.new('P', (320, 240))
    img.putpalette(p)
    px = img.load()
    scroll = ((base * 16 + camx) >> 1) % 1280
    for sy in range(240):
        for sx in range(320):
            wx = sx + scroll
            ty, tx = sy // 16, (wx // 16) % 80
            t = sky_ring[ty * 80 + tx] if ty < 15 else SKY_C
            px[sx, sy] = tiles[t][(sy % 16) * 16 + wx % 16]
    for sy in range(240):
        for sx in range(320):
            wx = camx + sx
            tx, ty = wx // 16, sy // 16
            if 0 <= tx < 120 and ty < 15:
                v = tiles[ram[world + ty * 120 + tx]][(sy % 16) * 16 + wx % 16]
                if v:
                    px[sx, sy] = v
    # gfx_sprite_render_array(ctx, from, array, count): array and count on the stack
    arr, count = u16(ram, sp + 3), ram[sp + 5]
    for i in reversed(range(count)):   # lower slots draw on top
        o = arr + 8 * i
        y, x, t, fl, opt = u16(ram, o), u16(ram, o + 2), ram[o + 4], ram[o + 5], u16(ram, o + 6)
        if y == 0 or x == 0 or x >= 336 or y >= 256:
            continue
        parts = [(t, 0)] + ([(t + 1, 16)] if opt & 2 else [])
        for (tt, dy) in parts:
            data = tiles[tt & 255]
            for yy in range(16):
                for xx in range(16):
                    v = data[yy * 16 + (15 - xx if fl & 8 else xx)]
                    X, Y = x - 16 + xx, y - 16 + dy + yy
                    if v and 0 <= X < 320 and 0 <= Y < 240:
                        px[X, Y] = v
    return img.convert('RGB').resize((320 * a.scale, 240 * a.scale), Image.NEAREST)


for s in a.seconds:
    ram, sp = grab(s)
    out = a.out if len(a.seconds) == 1 else a.out.replace('.png', '-%g.png' % s)
    render(ram, sp).save(out)
    print(out)
