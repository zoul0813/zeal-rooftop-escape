#!/usr/bin/env python3
"""
Writes the Rooftop Escape music as ZMT (Zeal Music Tracker) files in assets/.

All tracks are original. Each pattern is 32 sixteenth steps on up to 3 voices; voice 3 is
never written, so the game's sound effects own it. The files load with ZGDK's zmt_rom_load()
and can also be opened in ZMT itself.

Step tokens (one per sixteenth):
  .      nothing (the previous note keeps sounding)
  -      note off
  C5 F#4 a note (octave 4 holds middle C, like ZMT)
  k s h  drum hits for a noise voice (kick, snare, hat); a hit is cut off on the next empty step
"""
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets")

SEMI = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NOTE_OFF = 108
WAV = {"square": 0, "triangle": 1, "saw": 2, "noise": 3}
VOL = {0: 0xF0, 25: 0xF2, 50: 0xF4, 75: 0xF6, 100: 0xF8}
# noise "pitch" only changes the colour of the hiss
DRUMS = {"k": 24, "s": 64, "h": 96}   # C2, E5, C8


def note(tok):
    name = tok[0]
    sharp = 1 if tok[1] == "#" else (-1 if tok[1] == "b" else 0)
    octave = int(tok[-1])
    return octave * 12 + SEMI[name] + sharp


def steps(line, drums=False):
    toks = line.split()
    assert len(toks) == 32, (len(toks), line)
    out = []
    for i, t in enumerate(toks):
        if t == ".":
            # cut drum hits after one step, they are noise and would ring forever
            if drums and i > 0 and toks[i - 1] in DRUMS:
                out.append(NOTE_OFF)
            else:
                out.append(None)
        elif t == "-":
            out.append(NOTE_OFF)
        elif drums:
            out.append(DRUMS[t])
        else:
            out.append(note(t))
    return out


def voice(wave, vol, line):
    """One voice of a pattern: waveform, volume (percent) and 32 step tokens."""
    return (WAV[wave], VOL[vol], steps(line, drums=(wave == "noise")))


def pattern_bytes(voices):
    out = bytearray()
    bitmap = 0
    for i, v in enumerate(voices):
        if v is not None:
            bitmap |= 1 << i
    out.append(bitmap)
    for v in voices:
        if v is None:
            continue
        wave, vol, notes = v
        cells = []
        step_bits = 0
        for i, n in enumerate(notes):
            hdr = 0
            data = []
            if n is not None:
                hdr |= 0x01
                data.append(n)
            if i == 0:
                # every pattern sets its own waveform and volume, so patterns and tracks can follow
                # each other in any order
                hdr |= 0x02 | 0x04
                data += [wave, vol]
            if hdr:
                step_bits |= 1 << i
                cells.append(bytes([hdr] + data))
        out += struct.pack("<I", step_bits)
        for c in cells:
            out += c
    return bytes(out)


def zmt(title, tempo, patterns, arrangement):
    """tempo is frames per quarter note (a step is tempo/4 frames). arrangement: pattern indexes."""
    assert tempo % 4 == 0
    assert len(patterns) <= 16 and len(arrangement) <= 64
    out = bytearray(b"ZMT\x00")
    out += title.ljust(12)[:12].encode("ascii")
    out.append(tempo)
    out.append(len(patterns))
    bits = (1 << len(arrangement)) - 1
    out += struct.pack("<II", bits & 0xFFFFFFFF, bits >> 32)
    for p in arrangement:
        out += bytes([p, 0xFF])
    for p in patterns:
        out += pattern_bytes(p)
    return bytes(out)


# ---------------------------------------------------------------------------
# "Storm Chase", the running loop. D minor, 225 bpm (4 frames a step), A B A C A B D C.
# ---------------------------------------------------------------------------
BEAT = "k . h . s . h k k . h . s . h h k . h . s . h k k . h . s . s s"
BEAT2 = "k . h . s . h k k . h . s . h h k . k . s . k . s . s s s s s s"

RUN = [
    [   # A: Dm | Bb
        voice("square", 50, "D5 . F5 . A5 . D6 . C6 . A5 . F5 . G5 . A5 . . . G5 . F5 . E5 . C5 . D5 . - ."),
        voice("triangle", 100, "D2 . D3 . D2 . D3 . D2 . D3 . D2 . D3 . Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 ."),
        voice("noise", 25, BEAT),
        None,
    ],
    [   # B: Bb | C
        voice("square", 50, "F5 . D5 . F5 . Bb5 . A5 . F5 . D5 . . . E5 . G5 . C6 . . . Bb5 . A5 . G5 . E5 ."),
        voice("triangle", 100, "Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 . Bb1 . Bb2 . C2 . C3 . C2 . C3 . C2 . C3 . E2 . G2 ."),
        voice("noise", 25, BEAT),
        None,
    ],
    [   # C: Dm | A, the turn back to the top
        voice("square", 50, "D5 . . . F5 . . . A5 . G5 . F5 . E5 . C#5 . E5 . A5 . . . G5 . F5 . E5 . C#5 ."),
        voice("triangle", 100, "D2 . D3 . D2 . D3 . F2 . F3 . D2 . F2 . A1 . A2 . A1 . A2 . A1 . A2 . C#2 . E2 ."),
        voice("noise", 25, BEAT2),
        None,
    ],
    [   # D: the break, arpeggios over a held bass
        voice("square", 25, "D5 F5 A5 D6 A5 F5 D5 F5 A5 D6 A5 F5 D5 . . . Bb5 . A5 . G5 . F5 . E5 . F5 . G5 . A5 ."),
        voice("triangle", 100, "D2 . . . D3 . . . D2 . . . D3 . . . Bb1 . . . Bb2 . . . A1 . . . A2 . . ."),
        voice("noise", 25, "k . . . s . . . k . . . s . . . k . h . s . h . k . h . s s s s"),
        None,
    ],
]

# ---------------------------------------------------------------------------
# Title: slow and moody. Em9 | Cmaj7, triangle melody over a soft square arpeggio.
# ---------------------------------------------------------------------------
TITLE = [
    [
        voice("triangle", 100, "B5 . . . G5 . . . F#5 . . . E5 . . . D5 . . . E5 . F#5 . G5 . . . - . . ."),
        voice("square", 25, "E4 G4 B4 F#5 E4 G4 B4 F#5 E4 G4 B4 F#5 E4 G4 B4 F#5 C4 E4 G4 B4 C4 E4 G4 B4 C4 E4 G4 B4 C4 E4 G4 B4"),
        voice("triangle", 75, "E2 . . . . . . . . . . . . . . . C2 . . . . . . . . . . . . . . ."),
        None,
    ],
    [
        voice("triangle", 100, "A5 . . . G5 . . . F#5 . . . D5 . . . E5 . . . . . . . B4 . . . - . . ."),
        voice("square", 25, "A3 C4 E4 G4 A3 C4 E4 G4 D4 F#4 A4 C5 D4 F#4 A4 C5 E4 G4 B4 D5 E4 G4 B4 D5 B3 D#4 F#4 A4 B3 D#4 F#4 A4"),
        voice("triangle", 75, "A1 . . . . . . . D2 . . . . . . . E2 . . . . . . . B1 . . . . . . ."),
        None,
    ],
]

# ---------------------------------------------------------------------------
# Jingles, one pattern each. The game stops a one-shot when its pattern wraps.
# ---------------------------------------------------------------------------
SAFE = [[   # 4 frames a step: 128 frames
    voice("square", 50, "D5 . F5 . A5 . D6 . - . C6 . D6 . . . F6 . . . . . . . . . . . - . . ."),
    voice("triangle", 100, "D3 . . . F3 . . . A3 . . . D4 . . . F3 . . . . . . . D3 . . . - . . ."),
    voice("noise", 25, "k . h . s . h . k . k . s . s s k . . . . . . . . . . . . . . ."),
    None,
]]

WIN = [[   # 6 frames a step: 192 frames, the storm passes into D major
    voice("square", 50, "D5 . E5 . F#5 . A5 . F#5 . A5 . D6 . . . C#6 . . . A5 . . . D6 . . . - . . ."),
    voice("triangle", 100, "D3 . . . A2 . . . D3 . . . F#3 . . . A3 . . . E3 . . . D3 . . . - . . ."),
    voice("noise", 25, "k . h . s . h . k . h . s . s s k . . . s . . . k . . . s s s s"),
    None,
]]

CAUGHT = [[   # 3 frames a step: 96 frames, falling chromatically
    voice("square", 50, "D5 . . . C#5 . . . C5 . . . B4 . . . Bb4 . . . . . . . A4 . . . - . . ."),
    voice("triangle", 100, "D3 . . . C#3 . . . C3 . . . B2 . . . Bb2 . . . . . . . A2 . . . - . . ."),
    voice("noise", 25, "s . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ."),
    None,
]]

TRACKS = {
    "run.zmt": zmt("STORM CHASE", 16, RUN, [0, 1, 0, 2, 0, 1, 3, 2]),
    "title.zmt": zmt("ROOFTOPS", 32, TITLE, [0, 1]),
    "safe.zmt": zmt("SAFE", 16, SAFE, [0]),
    "win.zmt": zmt("STORM PASSES", 24, WIN, [0]),
    "caught.zmt": zmt("CAUGHT", 12, CAUGHT, [0]),
}

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, data in TRACKS.items():
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(data)
        print(f"{name}: {len(data)} bytes")
