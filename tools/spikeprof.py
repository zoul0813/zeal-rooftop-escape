#!/usr/bin/env python3
"""Spike profiler: where do the slow frames spend their time?
Runs the game headless in zeal-native, samples PC every STEP T-states, splits the samples into frames
at the gfx_wait_vblank loop, and profiles the work between two waits in the frames that ran over the
167,680 T frame, next to the profile of all frames.
Usage: spikeprof.py <game.bin> <seconds> --rom <rom> --tf <scratch.img>
       --eeprom <scratch.img> [--emu <zeal-native>] [--build <build-dir>]
       [--start] [--step T] [--pre 'cmd;cmd'] [--top N]
Samples approximate frame boundaries; use bpfps.py for exact late-frame counts."""
import os, re, subprocess, sys, bisect, collections, argparse, shutil, tempfile
import shlex
from emulator import add_arguments, resolve

ap = argparse.ArgumentParser()
ap.add_argument('bin'); ap.add_argument('secs', type=float)
ap.add_argument('--step', type=int, default=1500)
ap.add_argument('--pre', default='')
ap.add_argument('--top', type=int, default=25)
ap.add_argument('--frames', type=int, default=12)   # slow frames to list one by one
ap.add_argument('--tf', required=True, help='Scratch TF image')
ap.add_argument('--eeprom', required=True, help='Scratch EEPROM image')
ap.add_argument('--build', default=os.environ.get('BUILD_DIR'))
ap.add_argument('--start', action='store_true', help='Press Enter on titles that do not auto-start')
add_arguments(ap)
a = ap.parse_args()
resolve(a, ap)
binp = os.path.realpath(a.bin)
syms = []
for l in open(binp[:-4] + '.map'):
    m = re.match(r'\s+([0-9A-F]{8})\s+(\S+)\s*(\S*)', l)
    if m:
        ad = int(m.group(1), 16)
        if 0x4000 <= ad < 0x10000: syms.append((ad, m.group(2) + ' [' + (m.group(3) or 'lib') + ']'))
glob = {name.split(' ')[0]: ad for ad, name in syms}
WV, WE = glob['_gfx_wait_vblank'], glob['_gfx_wait_end_vblank']
build = a.build
if build:   # static functions from the per-object .sym files (as in bubble-keep/tools/hprof.py)
    for root, _, files in os.walk(build):
        for f in files:
            if not f.endswith('.c.sym'): continue
            entries = []
            for l in open(os.path.join(root, f)):
                m = re.match(r'\s+7\s+(\S+)\s+([0-9A-F]{6})\s+(G?R)', l)
                if m: entries.append((m.group(1), int(m.group(2), 16), m.group(3)))
            base = None
            for name, off, kind in entries:
                if kind == 'GR' and name in glob: base = glob[name] - off; break
            if base is None: continue
            for name, off, kind in entries:
                if kind == 'R': syms.append((base + off, name + ' [' + f[:-6] + ']'))
syms.sort(); addrs = [ad for ad, _ in syms]

def fn(pc):
    if pc < 0x4000: return '<kernel %02xxx>' % (pc >> 8)
    i = bisect.bisect_right(addrs, pc) - 1
    return syms[i][1] if i >= 0 else '?'

cmds = ['run 30000000']
for text in ('cd h:/', 'exec h:/' + os.path.basename(binp)):
    for ch in text:
        if ch == ' ': cmds.append('tap SPACE')
        elif ch == ':': cmds += ['press SHIFT', 'tap SEMICOLON', 'release SHIFT']
        elif ch == '_': cmds += ['press SHIFT', 'tap MINUS', 'release SHIFT']
        elif ch == '/': cmds.append('tap SLASH')
        elif ch == '.': cmds.append('tap PERIOD')
        else: cmds.append('tap ' + ch.upper())
        cmds.append('run 200000')
    cmds += ['tap ENTER', 'run 2000000']
cmds += ['run 50000000']
if a.start: cmds += ['press ENTER', 'run 1000000', 'release ENTER', 'run 20000000']
if a.pre: cmds += a.pre.split(';')
n = int(a.secs * 10_000_000 / a.step)
for i in range(n): cmds.append('run %d\nregs' % (a.step - 1))
cmds.append('quit')
with tempfile.TemporaryDirectory() as temp:
    config = os.path.join(temp, 'zeal.ini')
    open(config, 'w').close()
    p = subprocess.run([a.emu, '-c', config, '-r', a.rom,
                        '-t', a.tf, '-e', a.eeprom,
                        '-H', os.path.dirname(binp), '-o', '-g'] + shlex.split(a.args),
                       input='\n'.join(cmds) + '\n', capture_output=True, text=True, timeout=7200)
pcs = [int(x, 16) for x in re.findall(r'PC=([0-9a-f]{4})', p.stdout + p.stderr)]
if len(pcs) < n * 0.9: print((p.stdout + p.stderr)[-2000:]); sys.exit(1)

# the vblank-end wait is too short to sample reliably, so a frame is everything between two
# vblank waits: game logic plus the vblank work (scroll, sprites), against the 167,680 T frame
cls = ['v' if WV <= pc < WV + 8 else '.' for pc in pcs]
frames = []
i = 0
while i < len(cls) and cls[i] != 'v': i += 1
while i < len(cls):
    while i < len(cls) and cls[i] == 'v': i += 1
    j = i
    while j < len(cls) and cls[j] != 'v': j += 1
    if j < len(cls): frames.append((i, j))
    i = j
budget = 167680 / a.step
allc, slowc = collections.Counter(), collections.Counter()
slow = []
for s, e in frames:
    c = collections.Counter(fn(pcs[k]) for k in range(s, e))
    allc.update(c)
    if e - s > budget:
        slowc.update(c)
        slow.append((s, e, c))
print('%s: %d frames, %d over budget (%.1f%%), step %d T, budget %.0f samples'
      % (os.path.basename(binp), len(frames), len(slow), 100.0 * len(slow) / max(1, len(frames)), a.step, budget))
def show(title, c, nfr):
    tot = sum(c.values())
    print('\n%s (%d frames, avg %.0f%% of budget)' % (title, nfr, 100.0 * tot / max(1, nfr) / budget))
    for k, v in c.most_common(a.top):
        print('  %5.1f%%  %6.1f%% of budget/frame  %s' % (100.0 * v / tot, 100.0 * v / max(1, nfr) / budget, k))
show('all frames', allc, len(frames))
if slow:
    show('slow frames', slowc, len(slow))
    print('\nslow frames one by one (time s, load, top functions):')
    for s, e, c in slow[:a.frames]:
        t = s * a.step / 10_000_000
        print('  %6.2f s %4.0f%%  %s' % (t, 100.0 * (e - s) / budget,
              ', '.join('%s %d' % (k.split(' ')[0], v) for k, v in c.most_common(4))))
