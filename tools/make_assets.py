#!/usr/bin/env python3
"""Generate the Rooftop Escape tileset (assets/tileset.gif), its tile index header (src/gfx.h)
and the storm sky palettes (src/sky.h).

All art is drawn here as text pixel art or small procedures so it stays diffable. One 256-tile,
8-bit tileset is shared by both layers and all sprites. Palette index 0 is transparent for layer 1
and sprites; on layer 0 it is the top band of the night sky.

Layout:
    0         empty
    1..31     rooftop tiles (layer 1)
    32..95    font at the ASCII codes, so ZGDK's default ASCII_MAP prints text as-is
    96..127   skyline tiles (layer 0)
    128..     more rooftop tiles, then sprites
Run: python3 tools/make_assets.py   (from the game folder, needs Pillow)
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---------------------------------------------------------------- palette
PAL = [
    ('0', (40, 30, 70)),     # 0 sky band A (top) / transparent
    ('1', (80, 50, 100)),    # sky band B
    ('2', (170, 90, 90)),    # sky band C (horizon glow)
    ('3', (40, 34, 70)),     # far building silhouette
    ('4', (70, 56, 96)),     # far building lit edge
    ('5', (200, 200, 255)),  # stars
    ('k', (0, 0, 0)),
    ('w', (255, 255, 255)),
    ('l', (176, 170, 190)),  # light grey (ledges, rope)
    ('m', (110, 106, 128)),  # mid grey
    ('d', (64, 60, 82)),     # dark grey
    ('M', (244, 238, 206)),  # moon
    ('n', (196, 190, 160)),  # moon shade
    ('Y', (255, 214, 110)),  # lit window
    ('y', (190, 140, 60)),   # lit window shade
    ('Q', (226, 170, 80)),   # far window warm
    ('q', (110, 150, 200)),  # far window cool
    # near building style A: brick
    ('A', (132, 70, 64)),
    ('a', (98, 50, 50)),
    ('Z', (60, 30, 38)),
    # near building style B: concrete
    ('B', (82, 96, 122)),
    ('b', (60, 70, 96)),
    ('X', (36, 42, 62)),
    ('r', (230, 60, 60)),    # red light
    ('g', (70, 230, 130)),   # green light
    ('c', (150, 220, 240)),  # glass
    ('C', (70, 130, 170)),   # glass shade
    # courier
    ('O', (255, 140, 40)),   # jacket
    ('o', (190, 80, 20)),
    ('S', (240, 184, 140)),  # skin
    ('H', (52, 34, 30)),     # hair
    ('N', (50, 70, 140)),    # pants
    ('v', (30, 42, 90)),     # pants shade
    ('W', (236, 236, 240)),  # shoes
    ('K', (176, 116, 60)),   # package
    ('j', (116, 72, 36)),    # package shade
    # teammate jacket
    ('T', (60, 210, 200)),
    ('t', (30, 120, 140)),
    # pigeon
    ('P', (156, 156, 176)),
    ('p', (104, 104, 126)),
    ('G', (90, 160, 130)),   # neck
    ('e', (236, 140, 120)),  # beak / feet
    ('R', (120, 40, 30)),    # rust
    ('u', (255, 240, 150)),  # bright gold
    # storm front
    ('E', (34, 36, 48)),     # cloud core
    ('F', (78, 84, 104)),    # cloud
    ('f', (156, 164, 186)),  # cloud lit edge
    ('V', (170, 190, 230)),  # rain
    ('L', (255, 250, 200)),  # lightning
    # billboards
    ('i', (255, 70, 170)),   # neon pink
    ('I', (150, 30, 100)),   # neon pink shade
    ('U', (40, 20, 48)),     # sign panel
]
CI = {ch: i for i, (ch, _) in enumerate(PAL)}
assert len(PAL) <= 256

tiles = [None] * 256
names = {}
flags = {}   # tile -> flag names, written to gfx.h as TILE_FLAGS_INIT


def grid(rows, h=16):
    out = []
    for r in rows:
        r = r.replace(' ', '')
        assert len(r) <= 16, (r, len(r))
        out.append([CI[ch] for ch in r.ljust(16, '.').replace('.', '0')])
    while len(out) < h:
        out.append([0] * 16)
    assert len(out) == h, len(out)
    return out


def blank():
    return [[0] * 16 for _ in range(16)]


def fill(c):
    return [[CI[c]] * 16 for _ in range(16)]


def put(index, name, px, fl=None):
    assert tiles[index] is None, (index, name)
    assert len(px) == 16 and all(len(r) == 16 for r in px), name
    tiles[index] = px
    if name:
        names[name] = index
    if fl:
        flags[index] = fl


def recolor(px, mapping):
    m = {CI[a]: CI[b] for a, b in mapping.items()}
    return [[m.get(v, v) for v in r] for r in px]


def over(base, top):
    """top drawn over base, index 0 in top is see-through"""
    return [[t if t else b for b, t in zip(rb, rt)] for rb, rt in zip(base, top)]


def split_tall(px32):
    return px32[:16], px32[16:]


# ---------------------------------------------------------------- rooftops (layer 1)
put(0, 'TILE_EMPTY', blank())


def brick(style):
    L, M, D = ('A', 'a', 'Z') if style == 'A' else ('B', 'b', 'X')
    px = fill(L)
    if style == 'A':
        for y in range(16):
            for x in range(16):
                row = y // 4
                if y % 4 == 3:
                    px[y][x] = CI[D]
                elif (x + (row % 2) * 4) % 8 == 7:
                    px[y][x] = CI[D]
                elif y % 4 == 2:
                    px[y][x] = CI[M]
    else:
        for y in range(16):
            for x in range(16):
                if y == 7 or y == 15:
                    px[y][x] = CI[D]
                elif x == 15:
                    px[y][x] = CI[M]
                elif (x * 7 + y * 3) % 23 == 0:
                    px[y][x] = CI[M]
    return px


def edge(px, side, style):
    D = 'Z' if style == 'A' else 'X'
    px = [r[:] for r in px]
    for y in range(16):
        if side == 'L':
            px[y][0] = CI['k']
            px[y][1] = CI[D]
        else:
            px[y][15] = CI['k']
            px[y][14] = CI[D]
    return px


def roof(px, style):
    px = [r[:] for r in px]
    for x in range(16):
        px[0][x] = CI['l']
        px[1][x] = CI['l'] if x % 5 else CI['m']
        px[2][x] = CI['m']
        px[3][x] = CI['k']
    return px


def window(style, lit):
    px = brick(style)
    G, g = ('Y', 'y') if lit else ('d', 'k')
    for y in range(3, 14):
        for x in range(3, 13):
            if y in (3, 13) or x in (3, 12):
                px[y][x] = CI['k']
            elif x == 8 or y == 8:
                px[y][x] = CI['k']
            else:
                px[y][x] = CI[G] if (x + y) % 9 else CI[g]
    for x in range(2, 14):
        px[14][x] = CI['l']
    return px


SOLID = ['TF_SOLID']
for si, style in enumerate('AB'):
    base = 1 + si * 12
    put(base + 0, 'TILE_ROOF_L_' + style, roof(edge(brick(style), 'L', style), style), SOLID)
    put(base + 1, 'TILE_ROOF_M_' + style, roof(brick(style), style), SOLID)
    put(base + 2, 'TILE_ROOF_R_' + style, roof(edge(brick(style), 'R', style), style), SOLID)
    put(base + 3, 'TILE_WALL_L_' + style, edge(brick(style), 'L', style), SOLID)
    put(base + 4, 'TILE_WALL_M_' + style, brick(style), SOLID)
    put(base + 5, 'TILE_WALL_R_' + style, edge(brick(style), 'R', style), SOLID)
    put(base + 6, 'TILE_WIN_' + style, window(style, True), SOLID)
    put(base + 7, 'TILE_WIN_DARK_' + style, window(style, False), SOLID)

# chimney: cap + body, brick
cap = grid([
    "kkkkkkkkkkkkkkkk",
    "klllllllllllllmk",
    "kmmmmmmmmmmmmmdk",
    "kkkkkkkkkkkkkkkk",
    ".kAAAaAAAAAaAAk.",
    ".kaaaaaaaaaaaak.",
    ".kZZZZZZZZZZZZk.",
    ".kAAAAAaAAAAAAk.",
    ".kaaaaaaaaaaaak.",
    ".kZZZZZZZZZZZZk.",
    ".kAAaAAAAAAaAAk.",
    ".kaaaaaaaaaaaak.",
    ".kZZZZZZZZZZZZk.",
    ".kAAAAAAaAAAAAk.",
    ".kaaaaaaaaaaaak.",
    ".kZZZZZZZZZZZZk.",
])
put(9, 'TILE_CHIMNEY', cap, SOLID)
# AC unit with a fan grille
ac = grid([
    "................",
    "................",
    ".kkkkkkkkkkkkkk.",
    ".kllllllllllllk.",
    ".klkkkkkkkmmmlk.",
    ".klkdmdmdkmlmlk.",
    ".klkmdmdmkmmmlk.",
    ".klkdmdmdkmlmlk.",
    ".klkmdmdmkmmmlk.",
    ".klkdmdmdkmlmlk.",
    ".klkkkkkkkmmmlk.",
    ".kllllllllllllk.",
    ".kmmmmmmmmmmmmk.",
    ".kkkkkkkkkkkkkk.",
    "..kdk......kdk..",
    "..kdk......kdk..",
])
put(10, 'TILE_AC', ac, SOLID)
# broken skylight glass: hurts, not solid (sits on the roof)
glass = grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "........w.......",
    ".......kck......",
    "..w....kcCk..w..",
    "..kk..kccCk.kck.",
    ".kcCk.kcCCkkccCk",
    ".kcCkkccCCkkcCCk",
    "kkccCkccCCCkccCk",
    "kcccCkcccCCkcccC",
    "kddddddddddddddd",
    "kmmmmmmmmmmmmmmm",
    "kkkkkkkkkkkkkkkk",
])
put(11, 'TILE_GLASS', glass, ['TF_TRIGGER'])
# scaffold plank: one way
plank = grid([
    "kkkkkkkkkkkkkkkk",
    "KKKKKjKKKKKKKjKK",
    "jjjjjjjjjjjjjjjj",
    "kkkkkkkkkkkkkkkk",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "...kmk....kmk...",
    "....kmk..kmk....",
    ".....kmkkmk.....",
    "......kmmk......",
    ".....kmkkmk.....",
    "....kmk..kmk....",
    "...kmk....kmk...",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "..kmk......kmk..",
])
put(12, 'TILE_PLANK', plank, ['TF_ONEWAY'])
# scaffold legs under a plank (decor)
legs = grid([
    "..kmk......kmk..",
    "..kmk......kmk..",
    "...kmk....kmk...",
    "....kmk..kmk....",
    ".....kmkkmk.....",
    "......kmmk......",
    ".....kmkkmk.....",
    "....kmk..kmk....",
    "...kmk....kmk...",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "..kmk......kmk..",
    "..kmk......kmk..",
])
put(25, 'TILE_SCAFFOLD', legs)

# water tower: 2x2 tank (top row is standable) on 2 tiles of legs
tank = [
    "......kkkkkkkkkkkkkkkkkkkk......",
    "....kkRRRRRRRRRRRRRRRRRRRRkk....",
    "..kkRRRRRRRRRRRRRRRRRRRRRRRRkk..",
    ".kRRRRRRRRRRRRRRRRRRRRRRRRRRRRk.",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kjKKKjKKKKjKKKKjKKKKjKKKKjKKKjjk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    ".kjjjjjjjjjjjjjjjjjjjjjjjjjjjjk.",
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk..",
    "...kmk..................kmk.....",
    "...kmk..................kmk.....",
    "...kmkk................kkmk.....",
    "...kmkmk..............kmkmk.....",
    "...kmk.kmk..........kmk.kmk.....",
    "...kmk...kmk......kmk...kmk.....",
]


def cut(rows32x, tx, ty):
    return grid([r[tx * 16:(tx + 1) * 16] for r in rows32x[ty * 16:(ty + 1) * 16]])


put(26, 'TILE_TANK_TL', cut(tank, 0, 0), ['TF_ONEWAY'])
put(27, 'TILE_TANK_TR', cut(tank, 1, 0), ['TF_ONEWAY'])
put(28, 'TILE_TANK_BL', cut(tank, 0, 1))
put(29, 'TILE_TANK_BR', cut(tank, 1, 1))
tower_legs = [
    "...kmk...kmk......kmk...kmk.....",
    "...kmk.....kmk..kmk.....kmk.....",
    "...kmk.......kmmk.......kmk.....",
    "...kmk......kmkkmk......kmk.....",
    "...kmk....kmk....kmk....kmk.....",
    "...kmk..kmk........kmk..kmk.....",
    "...kmkkmk............kmkkmk.....",
    "...kmmk................kmmk.....",
    "...kmkmk..............kmkmk.....",
    "...kmk.kmk..........kmk.kmk.....",
    "...kmk...kmk......kmk...kmk.....",
    "...kmk.....kmk..kmk.....kmk.....",
    "...kmk.......kmmk.......kmk.....",
    "...kmk......kmkkmk......kmk.....",
    "...kmk....kmk....kmk....kmk.....",
    "..kkmkk..kk........kk..kkmkk....",
]
put(30, 'TILE_TOWER_L', cut(tower_legs, 0, 0))
put(31, 'TILE_TOWER_R', cut(tower_legs, 1, 0))

# clothesline: slope 1/4 (4 px down per tile), 4 variants by start height, plus a pole
for k in range(4):
    px = blank()
    for x in range(16):
        y = 4 * k + x // 4
        px[y][x] = CI['l']
        if y + 1 < 16:
            px[y + 1][x] = CI['d']
    put(128 + k, 'TILE_LINE%d' % k, px)
pole = grid([
    ".......kk.......",
    "......kllk......",
    "......kmmk......",
    ".......kk.......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    ".......kmk......",
    "......kmmmk.....",
    ".....kkkkkkk....",
])
put(132, 'TILE_POLE_TOP', pole)
pole_mid = grid([".......kmk......"] * 16)
put(133, 'TILE_POLE', pole_mid)
# laundry hanging on a line tile (decor drawn under the line in some spots)
# antenna / satellite dish decor (non solid)
dish = grid([
    "................",
    "................",
    "...........k....",
    "..kkk.....kmk...",
    ".kllmk....k.....",
    "kllllmk..k......",
    "kllllmmkk.......",
    "kllllmmmk.......",
    ".kllmmmmk.......",
    "..kmmmmk........",
    "...kkkkmk.......",
    ".......kmk......",
    ".......kmk......",
    "......kmmmk.....",
    ".....kmmmmmk....",
    ".....kkkkkkk....",
])
put(134, 'TILE_DISH', dish)
vent = grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "....kkkkkkk.....",
    "...kllllllmk....",
    "...kmdmdmdmk....",
    "...kkkkkkkkk....",
    "....kmllmmk.....",
    "....kmllmmk.....",
    "....kmllmmk.....",
    "....kmllmmk.....",
    "....kmllmmk.....",
    "...kmmmmmmmk....",
    "...kkkkkkkkk....",
])
put(135, 'TILE_VENT', vent)

# ---------------------------------------------------------------- escape tiles (layer 1)
# the stairwell hatch at the end of a leg: a little brick hut with a lit open door (not solid)
hut = [
    "................................",
    "................................",
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk..",
    ".kllllllllllllllllllllllllllllk.",
    ".kmmmmmmmmmmmmmmmmmmmmmmmmmmmmk.",
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk..",
    "..kAAAAAAaAAAAAAAAAAAAAaAAAAAk..",
    "..kAkkkkkkkkkkkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAaAAAAAAAaAAAk..",
    "..kAkYYYYYYYYYkAAAAAkkkkkAAAAk..",
    "..kakYYYYYYYYYkaAAAAkggrkAAAAk..",
    "..kAkYYyYYYYYYkAAAAAkkkkkAaAAk..",
    "..kAkYYyYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYyyYYYYYkAAaAAAAAAAAAAAk..",
    "..kAkYYYyYYYYYkAAAAAAAAAaAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kakYYYYYYYYYkaAAAAAAaAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAaAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAaAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAaAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kakYYYYYYYYYkaAAAAAAAaAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAaAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kAkYYYYYYYYYkAAAAAAAAAAAAAAk..",
    "..kkkkkkkkkkkkkkkkkkkkkkkkkkkk..",
]


def cut32(rows, tx, ty):
    return [[CI[ch] for ch in r[tx * 16:tx * 16 + 16].replace('.', '0')] for r in rows[ty * 16:ty * 16 + 16]]


put(21, 'TILE_HATCH_TL', cut32(hut, 0, 0))
put(22, 'TILE_HATCH_TR', cut32(hut, 1, 0))
put(23, 'TILE_HATCH_BL', cut32(hut, 0, 1))
put(24, 'TILE_HATCH_BR', cut32(hut, 1, 1))


# billboards: a 3-row neon sign on posts, its bottom at head height: slide under it
def sign(part_x, part_y):
    px = fill('U')
    for y in range(16):
        for x in range(16):
            if part_y == 'T' and y < 2 or part_y == 'B' and y > 13:
                px[y][x] = CI['k']
            elif part_y == 'T' and y == 2 or part_y == 'B' and y == 13:
                px[y][x] = CI['m']
            if part_x == 'L' and x < 2 or part_x == 'R' and x > 13:
                px[y][x] = CI['k']
    if part_y == 'T':
        for x in range(2, 16, 4):
            px[0][x] = CI['Y']    # bulbs
    if part_y == 'M':
        # a neon zigzag runs across the middle row
        for x in range(16):
            y = 5 + abs((x % 8) - 4)
            px[y][x] = CI['i']
            px[y + 1][x] = CI['I']
    if part_y == 'T' and part_x != 'L':
        for x in range(1, 15):
            if x % 5:
                px[8][x] = CI['c']
    if part_y == 'B' and part_x != 'R':
        for x in range(1, 15):
            if x % 3:
                px[6][x] = CI['C']
    return px


SIGN_IDX = 118
for py, pname in (('T', 'T'), ('M', 'M'), ('B', 'B')):
    for pxn in ('L', 'M', 'R'):
        name = 'TILE_SIGN_%s%s' % (py, pxn)
        put(SIGN_IDX, name, sign(pxn, py), SOLID)
        SIGN_IDX += 1
post = blank()
for y in range(16):
    for x in (7, 8):
        px_c = 'm' if x == 7 else 'd'
        post[y][x] = CI[px_c]
    if y % 6 == 0:
        post[y][6] = CI['k']
        post[y][9] = CI['k']
put(127, 'TILE_SIGN_POST', post)
# ---------------------------------------------------------------- font (5x7 scaled x2)
FONT = {
    ' ': [""] * 7,
    '!': ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
    '-': [".....", ".....", ".....", "#####", ".....", ".....", "....."],
    '.': [".....", ".....", ".....", ".....", ".....", ".##..", ".##.."],
    ':': [".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."],
    '?': [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    "'": ["..#..", "..#..", ".#...", ".....", ".....", ".....", "....."],
    '+': [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."],
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    '1': ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    '2': [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    '3': ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    '4': ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    '5': ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    '6': ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    'D': ["###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    'J': ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
}
for ch, rows in FONT.items():
    px = blank()
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c != '#':
                continue
            for dy in range(2):
                for dx in range(2):
                    px[1 + y * 2 + dy][3 + x * 2 + dx] = CI['w']
    # outline so text reads over the moving skyline
    out = [r[:] for r in px]
    for y in range(16):
        for x in range(16):
            if px[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < 16 and 0 <= xx < 16 and px[yy][xx] == CI['w']:
                        out[y][x] = CI['k']
    put(ord(ch), None, out)
for i in range(32, 96):
    if tiles[i] is None:
        tiles[i] = blank()

# ---------------------------------------------------------------- skyline (layer 0, opaque)
# rows 0-4 band A (stars), 5 dither A/B, 6-8 band B, 9 dither B/C, 10-14 band C
SKY_ROWS = ['0'] * 5 + ['01'] + ['1'] * 3 + ['12'] + ['2'] * 5


def sky(band):
    if len(band) == 1:
        return fill(band)
    px = fill(band[0])
    for y in range(16):
        for x in range(16):
            # ordered dither, denser towards the bottom of the tile
            if ((x * 5 + y * 3) % 16) < y:
                px[y][x] = CI[band[1]]
    return px


put(96, 'SKY_A', sky('0'))
put(97, 'SKY_AB', sky('01'))
put(98, 'SKY_B', sky('1'))
put(99, 'SKY_BC', sky('12'))
put(100, 'SKY_C', sky('2'))
for i, pts in enumerate([[(3, 4), (11, 12)], [(8, 2)], [(13, 7), (2, 13), (6, 9)]]):
    px = fill('0')
    for (x, y) in pts:
        px[y][x] = CI['5']
    if i == 1:
        for (dx, dy) in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            px[2 + dy][8 + dx] = CI['4']
    put(101 + i, 'SKY_STAR%d' % i, px)
# moon 2x2
moon = []
for y in range(32):
    row = ''
    for x in range(32):
        dx, dy = x - 15.5, y - 15.5
        d = (dx * dx + dy * dy) ** 0.5
        if d < 11:
            # crescent shadow on the left
            sd = ((x - 10.5) ** 2 + (y - 13.5) ** 2) ** 0.5
            row += 'n' if sd < 7 or (x * 3 + y * 7) % 31 == 0 else 'M'
        elif d < 13 and (x + y) % 2 == 0:
            row += '4'
        else:
            row += '0'
    moon.append(row)
put(104, 'SKY_MOON_TL', cut(moon, 0, 0))
put(105, 'SKY_MOON_TR', cut(moon, 1, 0))
put(106, 'SKY_MOON_BL', cut(moon, 0, 1))
put(107, 'SKY_MOON_BR', cut(moon, 1, 1))


def far_body(variant):
    px = fill('3')
    for y in range(16):
        for x in range(16):
            if x == 0:
                px[y][x] = CI['4']
    if variant >= 0:
        for wy in (2, 6, 10, 14):
            for wx in (3, 7, 11):
                lit = ((wx * 7 + wy * 5 + variant * 11) % 5) < 2
                if lit:
                    c = 'Q' if (wx + wy + variant) % 3 else 'q'
                    px[wy][wx] = CI[c]
                    px[wy][wx + 1] = CI[c]
                    px[wy + 1][wx] = CI[c]
                    px[wy + 1][wx + 1] = CI[c]
    return px


def far_cap(band, kind):
    px = sky(band)
    body = far_body(kind % 3)
    top = {'flat': 6, 'step': 6, 'spire': 6}[['flat', 'step', 'spire'][kind // 3]] if False else 6
    for y in range(16):
        for x in range(16):
            if kind == 0:      # flat roof with a ledge
                if y >= 6:
                    px[y][x] = body[y][x]
                if y == 6:
                    px[y][x] = CI['4']
            elif kind == 1:    # stepped
                if (y >= 10) or (y >= 4 and 4 <= x < 12):
                    px[y][x] = body[y][x]
                if (y == 10 and not 4 <= x < 12) or (y == 4 and 4 <= x < 12):
                    px[y][x] = CI['4']
            else:              # spire with a red light
                if y >= 12 or (y >= 2 and 7 <= x <= 8):
                    px[y][x] = CI['3']
                if y == 12:
                    px[y][x] = CI['4']
                if y in (1, 2) and x in (7, 8):
                    px[y][x] = CI['r']
    return px


for b, band in enumerate(['1', '2']):
    for kind in range(3):
        put(108 + b * 3 + kind, 'FAR_CAP%d_%d' % (b, kind), far_cap(band, kind))
for v in range(3):
    put(114 + v, 'FAR_BODY%d' % v, far_body(v))
put(117, 'FAR_DARK', far_body(-1))

# ---------------------------------------------------------------- sprites
S = 136


def spr(name, px, fl=None):
    global S
    put(S, name, px, fl)
    S += 1


def tall(name, px32):
    top, bot = split_tall(px32)
    spr(name, top)
    spr(None, bot)


HEAD = [
    "................",
    "................",
    "......kkkkk.....",
    ".....kHHHHHk....",
    "....kHHHHHHHk...",
    "....kHHSSSSSk...",
    "....kHSSSSkSk...",
    "....kHSSSSSSk...",
    ".....kSSSSSk....",
    "......kkSSk.....",
]
TORSO_DOWN = [
    ".....kkOOOk.....",
    "...kkKkOOOOk....",
    "..kKKKkOOOOOk...",
    "..kKjKkOOoOOk...",
    "..kKjKkOOoOOk...",
    "..kKKKkOOoSk....",
    "...kkkkOOoSk....",
    "......kNNNNk....",
    "......kNNNNk....",
    ".....kNNNNNNk...",
]
TORSO_FWD = [
    ".....kkOOOk.....",
    "...kkKkOOOOk....",
    "..kKKKkOOOOOkk..",
    "..kKjKkOOoOOOOk.",
    "..kKjKkOOooOSSk.",
    "..kKKKkOOoOkkk..",
    "...kkkkOOoOk....",
    "......kNNNNk....",
    "......kNNNNk....",
    ".....kNNNNNNk...",
]
TORSO_BACK = [
    ".....kkOOOk.....",
    "...kkKkOOOOk....",
    "..kKKKOOOOOOk...",
    "..kKjOOoOOOOk...",
    "..kKSSkOoOOk....",
    "..kKKkkOoOOk....",
    "...kkkkOOoOk....",
    "......kNNNNk....",
    "......kNNNNk....",
    ".....kNNNNNNk...",
]
TORSO_UP = [
    "...kOk.kkOOOkOk.",
    "...kOkkKOOOOkOk.",
    "...kOkKKOOOOOOk.",
    "...kOOKjOOoOOk..",
    "...kkOKjOOoOk...",
    "..kKKKkOOoOk....",
    "...kkkkOOoOk....",
    "......kNNNNk....",
    "......kNNNNk....",
    ".....kNNNNNNk...",
]
ARMS_OVER = [   # zip hang: hands above the head
    "....kSk..kSk....",
    "....kOk..kOk....",
]
LEGS = {
    'stand': [
        ".....kNNkNNk....",
        ".....kNNkNNk....",
        ".....kNNkNNk....",
        ".....kNvkNvk....",
        ".....kNvkNvk....",
        ".....kNvkNvk....",
        ".....kNvkNvk....",
        ".....kNvkNvk....",
        ".....kNvkNvk....",
        "....kWWkWWWk....",
        "....kWWWkWWWk...",
        "....kkkkkkkkk...",
    ],
    'stride': [
        "......kNNNk.....",
        ".....kNNkNNk....",
        "....kNNk.kNNk...",
        "....kNNk..kNNk..",
        "...kNvk....kNvk.",
        "...kNvk.....kNk.",
        "..kNvk......kNk.",
        "..kNvk......kNk.",
        ".kNvk.......kNk.",
        ".kWWk......kWWWk",
        "kWWWk......kWWWk",
        "kkkk........kkkk",
    ],
    'pass': [
        "......kNNNk.....",
        "......kNNNk.....",
        "......kNNNNk....",
        "......kNNkNNk...",
        "......kNvkkNNk..",
        "......kNvk.kNk..",
        "......kNvk.kWWk.",
        "......kNvk.kkk..",
        "......kNvk......",
        ".....kWWWk......",
        ".....kWWWWk.....",
        ".....kkkkkk.....",
    ],
    'reach': [
        "......kNNNk.....",
        ".....kNNNNNk....",
        "....kNNkkNNNk...",
        "...kNNk..kNNNk..",
        "..kNvk....kNNk..",
        ".kNvk......kNNk.",
        ".kWWk.......kNk.",
        "..kk........kNk.",
        "............kWWk",
        "...........kWWWk",
        "...........kWWWk",
        "...........kkkkk",
    ],
    'tuck': [
        "......kNNNNk....",
        ".....kNNNNNNk...",
        ".....kNNkkNNNk..",
        "....kNNk..kNNk..",
        "....kNvk..kNvk..",
        "...kWWWk.kWWWk..",
        "...kkkk..kkkk...",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
    'spread': [
        "......kNNNk.....",
        ".....kNNkNNk....",
        ".....kNNkNNk....",
        "....kNNk.kNNk...",
        "....kNvk.kNvk...",
        "...kNvk...kNvk..",
        "...kNvk...kNvk..",
        "..kWWWk...kWWWk.",
        "..kkkk.....kkkk.",
        "................",
        "................",
        "................",
    ],
    'wall': [
        "......kNNNk.....",
        "......kNNNNk....",
        "......kNNkNNk...",
        ".....kNNk.kNNk..",
        ".....kNvk..kNvk.",
        "....kNvk...kWWWk",
        "....kNvk...kWWWk",
        "....kNvk....kkkk",
        "...kWWWk........",
        "...kWWWWk.......",
        "...kkkkkk.......",
        "................",
    ],
}


def hero(torso, legs, arms_over=False, shift=0):
    rows = HEAD + torso + LEGS[legs]
    assert len(rows) == 32, len(rows)
    if shift:
        rows = rows[-shift:] + rows[:-shift] if shift > 0 else rows
    px = grid(rows, 32)
    if arms_over:
        top = grid(ARMS_OVER + ["....kOk..kOk...."] * 8, 32)
        px = over(px, top)
    return px


def lowered(px, n):
    """drop the art n pixels so a crouched / airborne pose still sits on the feet line"""
    return [[0] * 16 for _ in range(n)] + px[:32 - n]


HERO_FRAMES = [
    ('SPR_HERO_STAND', hero(TORSO_DOWN, 'stand')),
    ('SPR_HERO_RUN1', hero(TORSO_FWD, 'stride')),
    ('SPR_HERO_RUN2', hero(TORSO_DOWN, 'pass')),
    ('SPR_HERO_RUN3', hero(TORSO_BACK, 'reach')),
    ('SPR_HERO_RUN4', hero(TORSO_DOWN, 'pass')),
    ('SPR_HERO_JUMP', lowered(hero(TORSO_UP, 'tuck'), 0)),
    ('SPR_HERO_FALL', hero(TORSO_FWD, 'spread')),
    ('SPR_HERO_WALL', hero(TORSO_FWD, 'wall')),
    ('SPR_HERO_ZIP', hero(TORSO_UP, 'spread', arms_over=True)),
    ('SPR_HERO_STUMBLE', lowered(hero(TORSO_BACK, 'spread'), 3)),
    ('SPR_HERO_SLIDE', grid([""] * 16 + [
        "................",
        "................",
        "................",
        "..kkkk..........",
        ".kHHHHk.........",
        ".kHHSSk.........",
        ".kHSSkSk........",
        "..kSSSk.........",
        "..kkkOOkkk......",
        ".kKkOOOOOOkk....",
        "kKjKOOooOOOSk...",
        "kKjKkOOOOkkk....",
        ".kkkkNNNNNNNkkk.",
        "....kNNvvvNNNNNk",
        "....kNNkkkkkWWWk",
        "....kWWk....kkkk",
    ], 32)),
]
for name, px in HERO_FRAMES:
    tall(name, px)
mate = recolor(hero(TORSO_DOWN, 'stand'), {'O': 'T', 'o': 't', 'K': '0', 'j': '0'})
tall('SPR_MATE_STAND', mate)
tall('SPR_MATE_WAVE', recolor(hero(TORSO_UP, 'stand', arms_over=True), {'O': 'T', 'o': 't', 'K': '0', 'j': '0'}))

PIGEON = {
    'SPR_PIGEON_SIT': [
        "", "", "", "", "",
        "........kkk.....",
        ".......kGGGk....",
        ".......kGwkek...",
        "..kkkkkkGGGkk...",
        ".kPPPPPPPPGk....",
        "kPPpPPPPPPPk....",
        ".kPPppppPPk.....",
        "..kkPPPPkk......",
        "....kekek.......",
        "....kk.kk.......",
    ],
    'SPR_PIGEON_PECK': [
        "", "", "", "", "", "", "",
        "..kkkkkkk.......",
        ".kPPPPPPPkkkk...",
        "kPPpPPPPPGGGGk..",
        ".kPPppppPGwkGek.",
        "..kkPPPPkkkkkk..",
        "....kekek.......",
        "....kk.kk.......",
    ],
    'SPR_PIGEON_FLY1': [
        "",
        "..kk............",
        ".kPpk.....kk....",
        ".kPPpk...kPpk...",
        "..kPPpk.kPPpk...",
        "...kPPpkPPpk.kk.",
        "....kPPPPPk.kGGk",
        "..kkPPPPPPkkGwek",
        ".kPPPPPPPPPGGkk.",
        "kpPPpppPPPPGk...",
        ".kkkkkkkkkkk....",
    ],
    'SPR_PIGEON_FLY2': [
        "", "", "", "", "",
        "..............kk",
        "...kkkkkkkkk.kGG",
        "..kPPPPPPPPPkGwe",
        ".kPPpppPPPPPGGkk",
        "kpPPPPPPPPPGk...",
        ".kPPpPPPpkkk....",
        "..kPPpkPPpk.....",
        "...kPpk.kPpk....",
        "....kk...kPk....",
        "..........kk....",
    ],
}
for name, rows in PIGEON.items():
    spr(name, grid(rows))

stamp = [
    "",
    "...k.k.k.k.k....",
    "..kuuuuuuuuuk...",
    "...uYYYYYYYu....",
    "..kuYrrrrrYuk...",
    "...uYrwwwrYu....",
    "..kuYrwKwrYuk...",
    "...uYrwwwrYu....",
    "..kuYrrrrrYuk...",
    "...uYYYYYYYu....",
    "..kuuuuuuuuuk...",
    "...k.k.k.k.k....",
]
spr('SPR_STAMP1', grid(stamp))
spr('SPR_STAMP2', recolor(grid(stamp), {'u': 'w', 'Y': 'u'}))

beacon = [
    "",
    "......kkkk......",
    ".....krrrrk.....",
    ".....krwrrk.....",
    "......kkkk......",
    ".......kk.......",
    "......kmmk......",
    ".......kk.......",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    "......kmlmk.....",
    ".......kmk......",
    ".....kmmlmmk....",
    "....kmmmmmmmk...",
    "...kkkkkkkkkkk..",
    "", "", "", "",
]
beacon = beacon[:32]
tall('SPR_BEACON_RED', grid(beacon, 32))
tall('SPR_BEACON_GREEN', recolor(grid(beacon, 32), {'r': 'g'}))

dust1 = grid(["", "", "", "", "", "", "", "", "", "",
              "......ll........",
              "...llmmll..ll...",
              "..lmmmmmml.lml..",
              "...llmmll...l...", ])
dust2 = grid(["", "", "", "", "", "", "", "", "",
              "..l.........l...",
              ".lml..m...m.lml.",
              "..l...........l.",
              "................",
              "....m......m....", ])
spr('SPR_DUST1', dust1)
spr('SPR_DUST2', dust2)
pip_off = grid(["", "", "", "",
                "...kkk..........",
                "...kdmk.........",
                "....kdmk........",
                ".....kdmk.......",
                ".....kdmk.......",
                "....kdmk........",
                "...kdmk.........",
                "...kkk.........."])
spr('SPR_PIP_OFF', pip_off)
spr('SPR_PIP_ON', recolor(pip_off, {'d': 'O', 'm': 'Y'}))
spr('SPR_PIP_MAX', recolor(pip_off, {'d': 'r', 'm': 'w'}))
clock = grid(["",
              ".....kkkkk......",
              "...kkYYYYYkk....",
              "..kYYYwkwYYYk...",
              "..kYYwwkwwYYk...",
              ".kYYwwwkwwwYYk..",
              ".kYYwwwkkkwYYk..",
              ".kYYwwwwwwwYYk..",
              "..kYYwwwwwYYk...",
              "..kYYYwwwYYYk...",
              "...kkYYYYYkk....",
              ".....kkkkk......"])
spr('SPR_LIFE', grid(["",
                      "..kkk...kkk.....",
                      ".kOOOk.kOOOk....",
                      "kOOwOOkOOOOOk...",
                      "kOwOOOOOOOOOk...",
                      "kOOOOOOOOOOOk...",
                      ".kOOOOOOOOOk....",
                      "..kOOOOOOOk.....",
                      "...kOOOOOk......",
                      "....kOOOk.......",
                      ".....kOk........",
                      "......k........."]))
spr('SPR_PACKAGE', grid(["", "", "",
                         "...kkkkkkkkkk...",
                         "..kKKKKrKKKKjk..",
                         "..kKKKKrKKKKjk..",
                         "..krrrrrrrrrjk..",
                         "..kKKKKrKKKKjk..",
                         "..kKKKKrKKKKjk..",
                         "..kjjjjjjjjjjk..",
                         "...kkkkkkkkkk..."]))



# the storm front: a cloud wall (left column) and its ragged, raining edge (three churn frames)
def storm_core():
    # churning cloud: swirls of lighter grey in the dark core
    px = fill('E')
    for y in range(16):
        for x in range(16):
            w = (x * 3 + y * 5) % 16
            if w < 3 or (x - 8) ** 2 + (y - 5) ** 2 < 9:
                px[y][x] = CI['F']
            if w == 1 and x % 3 == 0:
                px[y][x] = CI['f']
    return px


def storm_edge(phase):
    px = blank()
    for y in range(16):
        # the cloud's edge bulges in and out down the column
        w = 6 + int(3 * abs(((y + phase * 5) % 16) - 8) / 4)
        for x in range(w):
            px[y][x] = CI['E'] if x < w - 5 else CI['F'] if x < w - 2 else CI['f']
        # rain streaks blown ahead of it
        for i in range(3):
            rx = w + 1 + ((y + phase * 3 + i * 5) * 7) % (15 - w if w < 14 else 1)
            if (y + i + phase) % 4 == 0 and rx < 16:
                px[y][rx] = CI['V']
    return px


spr('SPR_STORM', storm_core())
for ph in range(3):
    spr('SPR_STORM_EDGE%d' % (ph + 1), storm_edge(ph))

# pipes on stilts at head height: slide under or jump over
pipe = blank()
for y in range(4, 15):
    for x in range(16):
        pipe[y][x] = CI['l'] if y == 5 else CI['m'] if y < 10 else CI['d'] if y < 14 else CI['k']
    pipe[y][0] = pipe[y][0]
for y in (4, 14):
    for x in range(16):
        pipe[y][x] = CI['k']
pipe_l = [r[:] for r in pipe]
pipe_r = [r[:] for r in pipe]
for y in range(3, 16):
    for x in range(4):
        pipe_l[y][x] = CI['R'] if 4 < y < 14 and 0 < x < 3 else CI['k']
        pipe_r[y][15 - x] = CI['R'] if 4 < y < 14 and 0 < x < 3 else CI['k']
pipe_m = [r[:] for r in pipe]
for y in range(5, 14):
    pipe_m[y][8] = CI['R']
spr('TILE_PIPE_L', pipe_l, SOLID)
spr('TILE_PIPE_M', pipe_m, SOLID)
spr('TILE_PIPE_R', pipe_r, SOLID)
stilt = blank()
for y in range(16):
    stilt[y][7] = CI['d']
    stilt[y][8] = CI['k']
spr('TILE_PIPE_LEG', stilt)
SPR_END = S
assert SPR_END <= 256, SPR_END
for i in range(256):
    if tiles[i] is None:
        tiles[i] = blank()

# ---------------------------------------------------------------- storm palettes for the sky bands
# indices 0..4 of the palette (sky A, B, C, silhouette, silhouette edge): dusk, the storm closing
# in leg by leg, then sunrise when it has passed; the last two are the lightning flash
# plus the stars (index 5); the moon (indices 11-12) fades behind the clouds in MOON
SKY = [
    [(40, 30, 70), (80, 50, 100), (170, 90, 90), (40, 34, 70), (70, 56, 96), (200, 200, 255)],
    [(30, 28, 60), (56, 44, 90), (120, 70, 100), (36, 32, 66), (56, 50, 90), (150, 150, 200)],
    [(22, 24, 44), (40, 42, 66), (80, 64, 90), (30, 30, 52), (48, 48, 74), (70, 72, 100)],
    [(18, 26, 30), (30, 44, 46), (60, 74, 70), (24, 32, 36), (40, 52, 56), (18, 26, 30)],
    [(14, 18, 24), (24, 30, 38), (44, 52, 60), (20, 24, 30), (34, 40, 48), (14, 18, 24)],
    [(10, 12, 16), (18, 22, 28), (32, 38, 46), (16, 18, 24), (28, 32, 40), (10, 12, 16)],
    [(96, 130, 190), (180, 150, 180), (255, 190, 120), (62, 52, 84), (120, 96, 120), (96, 130, 190)],
]
MOON = [
    [(244, 238, 206), (196, 190, 160)],
    [(200, 196, 176), (160, 156, 136)],
    [(84, 84, 100), (64, 64, 84)],
] + [[s[0], s[0]] for s in SKY[3:]]
FLASH = [
    [(200, 200, 230), (170, 170, 210), (140, 140, 190), (90, 90, 120), (120, 120, 150), (200, 200, 230)],
    [(90, 90, 120), (80, 80, 110), (70, 70, 100), (50, 50, 70), (70, 70, 90), (90, 90, 120)],
]
assert SKY[0] == [PAL[i][1] for i in range(6)]
assert MOON[0] == [PAL[CI['M']][1], PAL[CI['n']][1]] and CI['n'] == CI['M'] + 1


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


# ---------------------------------------------------------------- write gif + headers
COLS = 16
rows_used = (SPR_END + COLS - 1) // COLS
img = Image.new('P', (16 * COLS, 16 * rows_used))
flat = []
for (_, rgb) in PAL:
    flat += list(rgb)
flat += [0] * (768 - len(flat))
img.putpalette(flat)
for i in range(SPR_END):
    tx, ty = (i % COLS) * 16, (i // COLS) * 16
    for y in range(16):
        for x in range(16):
            img.putpixel((tx + x, ty + y), tiles[i][y][x])
os.makedirs(os.path.join(ROOT, 'assets'), exist_ok=True)
img.save(os.path.join(ROOT, 'assets', 'tileset.gif'))
os.makedirs(os.path.join(ROOT, 'screenshots', 'assets'), exist_ok=True)
img.convert('RGB').resize((img.width * 3, img.height * 3), Image.NEAREST).save(os.path.join(ROOT, 'screenshots', 'assets', 'tileset_preview.png'))

with open(os.path.join(ROOT, 'src', 'gfx.h'), 'w') as f:
    f.write('/* Generated by tools/make_assets.py, do not edit */\n#ifndef GFX_H\n#define GFX_H\n\n')
    for name, idx in sorted(names.items(), key=lambda kv: kv[1]):
        f.write('#define %-24s %3d\n' % (name, idx))
    f.write('\n#define TILE_COUNT %d\n\n' % SPR_END)
    f.write('/* tile flags, for a 256-entry table: TILE_FLAGS_INIT inside an array initializer */\n')
    f.write('#define TILE_FLAGS_INIT \\\n')
    f.write(' \\\n'.join('    [%d] = %s,' % (i, ' | '.join(fl)) for i, fl in sorted(flags.items())))
    f.write('\n\n#endif\n')

with open(os.path.join(ROOT, 'src', 'sky.h'), 'w') as f:
    f.write('/* Generated by tools/make_assets.py, do not edit. RGB565 colors 0-5 (sky, skyline, stars): storm steps,\n'
            ' * sunrise, lightning; the two moon colors per step. */\n')
    f.write('#ifndef SKY_H\n#define SKY_H\n#define SKY_COLORS 6\n#define SKY_STEPS %d\n' % len(SKY))
    f.write('#define MOON_COLOR %d\n' % CI['M'])
    f.write('#define SKY_FLASH %d\n#define SKY_FLASH_DIM %d\n' % (len(SKY), len(SKY) + 1))
    f.write('static const uint16_t SKY_PALETTES[SKY_STEPS + 2][SKY_COLORS] = {\n')
    for d in SKY + FLASH:
        f.write('    { %s },\n' % ', '.join('0x%04X' % rgb565(c) for c in d))
    f.write('};\nstatic const uint16_t MOON_PALETTES[SKY_STEPS][2] = {\n')
    for d in MOON:
        f.write('    { %s },\n' % ', '.join('0x%04X' % rgb565(c) for c in d))
    f.write('};\n#endif\n')
print('tiles used:', SPR_END)

# Shared SNES art, owned locally so the game remains standalone.
from snes_glyphs import install as install_snes_glyphs
install_snes_glyphs(ROOT, SPR_END)
