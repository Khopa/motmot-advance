#!/usr/bin/env python3
"""Record docs/demo.gif: play tools/demo.lua in mGBA (unthrottled), grabbing a
screenshot every few frames, then assemble an animated GIF.

usage: make_demo.py [--rom build/motmot.gba] [--mgba PATH] [--every 3] [--scale 2]
                    [--lang 0]
                    [--out docs/demo.gif]
Reuses tests/emu/run.py (symbols, offsets, lib.lua) so the demo script can
drive the game exactly like a test scenario.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "emu"))
import run as emu  # noqa: E402

from PIL import Image  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rom", default=os.path.join(ROOT, "build", "motmot.gba"))
    ap.add_argument("--mgba", default=next((p for p in emu.MGBA_CANDIDATES if p and os.path.exists(p)), None))
    ap.add_argument("--every", type=int, default=3, help="capture one frame out of N (60 fps source)")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--lang", type=int, default=0, help="language index on the boot screen (0 FR, 1 EN, 2 ES, 3 DE, 4 IT)")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "demo.gif"))
    a = ap.parse_args()
    if not a.mgba:
        sys.exit("mGBA not found: pass --mgba or set MGBA")

    work = os.path.join(ROOT, "build", "demo")
    frames_dir = os.path.join(work, "frames")
    shutil.rmtree(frames_dir, ignore_errors=True)
    os.makedirs(frames_dir, exist_ok=True)
    rom = os.path.join(work, "demo.gba")
    shutil.copyfile(a.rom, rom)
    if os.path.exists(os.path.join(work, "demo.sav")):
        os.remove(os.path.join(work, "demo.sav"))

    syms = emu.symbol_addresses(os.path.splitext(a.rom)[0] + ".elf")
    offs = emu.struct_offsets()
    with open(os.path.join(HERE, "demo.lua"), encoding="utf-8") as f:
        source = f.read()
    cfg_extra = (f'CFG.frames_dir = "{frames_dir.replace(chr(92), "/")}"; '
                 f'CFG.every = {a.every}; CFG.lang = {a.lang}\n')
    emu.WORK = work
    script = emu.build_script("demo", cfg_extra + source, syms, offs, work.replace("\\", "/"))
    subprocess.run([a.mgba, "-C", "videoSync=0", "-C", "audioSync=0", "--script", script, rom],
                   timeout=300, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    files = sorted(glob.glob(os.path.join(frames_dir, "*.png")))
    if not files:
        sys.exit("no frames captured (see build/demo/demo.log)")
    print(f"{len(files)} frames captured")

    # one shared palette for the whole animation, built from a sample of frames
    sample = [Image.open(f).convert("RGB") for f in files[:: max(1, len(files) // 40)]]
    strip = Image.new("RGB", (240, 160 * len(sample)))
    for i, im in enumerate(sample):
        strip.paste(im, (0, 160 * i))
    palette = strip.quantize(colors=64)

    frames, durations = [], []
    step_ms = round(1000 * a.every / 60)
    last = None
    for f in files:
        im = Image.open(f).convert("RGB")
        data = im.tobytes()
        if data == last:
            durations[-1] += step_ms          # identical frame: just hold the previous one
            continue
        last = data
        if a.scale != 1:
            im = im.resize((240 * a.scale, 160 * a.scale), Image.NEAREST)
        frames.append(im.quantize(palette=palette, dither=Image.Dither.NONE))
        durations.append(step_ms)
    durations[-1] += 1500                      # linger on the last frame before looping

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    frames[0].save(a.out, save_all=True, append_images=frames[1:], duration=durations,
                   loop=0, optimize=True, disposal=1)
    size = os.path.getsize(a.out) / 1024
    print(f"{a.out}: {len(frames)} frames, {sum(durations) / 1000:.1f} s, {size:.0f} KiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
