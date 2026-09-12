#!/usr/bin/env python3
"""Run the emulator scenarios (tests/emu/scenarios/*.lua) in mGBA.

Each scenario is a Lua script using the T.* helpers of lib.lua. For every
scenario the runner:
  1. builds a self-contained script: CFG table (symbol addresses read from the
     ELF with nm, struct offsets printed by offsets.c, paths) + lib.lua +
     the scenario;
  2. runs `mGBA --script <script> <rom>` on a private copy of the ROM
     (build/emu/rom.gba, save file build/emu/rom.sav) with video/audio sync
     disabled, so the emulator runs as fast as it can;
  3. parses the scenario log (PASS / FAIL / ERROR / END lines) and collects
     the screenshots in tests/out/.

Scenario headers:
  -- @fresh    delete the save file before running (default: keep it, so a
               scenario can rely on what earlier ones left in SRAM)
  -- @timeout N   seconds allowed for the scenario (default 120)

usage: run.py [--rom build/motmot.gba] [--mgba PATH] [--list] [--slow]
              [--keep-going] [scenario ...]
  scenario     names (with or without .lua / numeric prefix); default: all,
               in file-name order.
Needs an mGBA build with the --script option (0.11 development builds).
Exit status is 0 when every scenario passed.
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SCENARIOS = os.path.join(HERE, "scenarios")
OUT = os.path.join(ROOT, "tests", "out")
WORK = os.path.join(ROOT, "build", "emu")

MGBA_CANDIDATES = [
    os.environ.get("MGBA", ""),
    "C:/Tools/mgba-nightly/mGBA.exe",
    "/usr/bin/mgba-qt",
    "/usr/local/bin/mgba-qt",
]


def find_tool(name):
    """arm-none-eabi-<name> from DEVKITARM / DEVKITPRO / the usual Windows path."""
    for base in (os.environ.get("DEVKITARM"), os.environ.get("DEVKITPRO", "") + "/devkitARM",
                 "C:/msys64/opt/devkitpro/devkitARM", "/opt/devkitpro/devkitARM"):
        if base:
            for ext in ("", ".exe"):
                p = os.path.join(base, "bin", f"arm-none-eabi-{name}{ext}")
                if os.path.exists(p):
                    return p
    return shutil.which(f"arm-none-eabi-{name}") or sys.exit(f"arm-none-eabi-{name} not found")


def symbol_addresses(elf):
    out = subprocess.check_output([find_tool("nm"), elf], text=True)
    syms = {}
    for line in out.splitlines():
        m = re.match(r"([0-9a-fA-F]+) [BbDdRr] (\w+)$", line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
    for s in ("game", "kb", "save", "marathon", "ta", "menu_item", "sub_item", "records_page",
              "menu_lang", "frames", "current_screen"):
        if s not in syms:
            sys.exit(f"symbol {s} not found in {elf}")
    return syms


def struct_offsets():
    """Compile tests/emu/offsets.c with the host compiler and parse its output."""
    os.makedirs(WORK, exist_ok=True)
    exe = os.path.join(WORK, "offsets.exe" if os.name == "nt" else "offsets")
    src = os.path.join(HERE, "offsets.c")
    cc = next((c for c in (os.environ.get("HOSTCC"), shutil.which("gcc"), "C:/msys64/usr/bin/gcc.exe")
               if c and (os.path.exists(c) or shutil.which(c))), None)
    if not cc:
        sys.exit("no host C compiler found (set HOSTCC, or run from the MSYS2 shell)")
    env = dict(os.environ)
    if cc.lower().startswith("c:/msys64"):          # MSYS2's gcc needs its own DLLs on PATH
        env["PATH"] = "C:/msys64/usr/bin;" + env.get("PATH", "")
    subprocess.check_call([cc, "-DHOST_TEST", "-I" + os.path.join(ROOT, "include"),
                           "-I" + os.path.join(ROOT, "tests", "unit"), src, "-o", exe], env=env)
    offs = {}
    for line in subprocess.check_output([exe], text=True, env=env).splitlines():
        k, v = line.split("=")
        offs[k] = int(v)
    return offs


def lua_table(d):
    return "{ " + ", ".join(f'["{k}"] = {v}' for k, v in d.items()) + " }"


def build_script(name, source, syms, offs, out_dir):
    cfg = (f'CFG = {{ scenario = "{name}", out = "{out_dir}", '
           f"sym = {lua_table(syms)}, off = {lua_table(offs)} }}\n")
    with open(os.path.join(HERE, "lib.lua"), encoding="utf-8") as f:
        lib = f.read()
    script = os.path.join(WORK, name + ".lua")
    with open(script, "w", encoding="utf-8") as f:
        f.write(cfg + lib + "\n-- ===== scenario =====\n" + source)
    return script


def parse_log(path):
    if not os.path.exists(path):
        return {"passed": 0, "failed": 0, "fails": [], "error": "no log written", "ended": False}
    passed = failed = 0
    fails, error, ended = [], None, False
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("PASS "):
                passed += 1
            elif line.startswith("FAIL "):
                failed += 1
                fails.append(line[5:])
            elif line.startswith("ERROR "):
                error = line[6:]
            elif line.startswith("END "):
                ended = True
    return {"passed": passed, "failed": failed, "fails": fails, "error": error, "ended": ended}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("scenarios", nargs="*")
    ap.add_argument("--rom", default=os.path.join(ROOT, "build", "motmot.gba"))
    ap.add_argument("--mgba", default=next((p for p in MGBA_CANDIDATES if p and os.path.exists(p)), None))
    ap.add_argument("--list", action="store_true", help="list the scenarios and exit")
    ap.add_argument("--slow", action="store_true", help="keep the emulator at normal speed (watch it play)")
    ap.add_argument("--keep-going", action="store_true", help="do not stop at the first failing scenario")
    ap.add_argument("-v", "--verbose", action="store_true", help="print every log line")
    a = ap.parse_args()

    files = sorted(glob.glob(os.path.join(SCENARIOS, "*.lua")))
    names = [os.path.splitext(os.path.basename(f))[0] for f in files]
    if a.list:
        for n in names:
            print(n)
        return 0
    if a.scenarios:
        selected = []
        for want in a.scenarios:
            want = os.path.splitext(os.path.basename(want))[0]
            match = [n for n in names if n == want or n.endswith("_" + want) or want in n]
            if not match:
                sys.exit(f"unknown scenario: {want} (see --list)")
            selected += match
        names = [n for n in names if n in selected]
    if not a.mgba:
        sys.exit("mGBA not found: pass --mgba or set MGBA")

    elf = os.path.splitext(a.rom)[0] + ".elf"
    syms = symbol_addresses(elf)
    offs = struct_offsets()
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    rom = os.path.join(WORK, "rom.gba")
    sav = os.path.join(WORK, "rom.sav")
    shutil.copyfile(a.rom, rom)
    out_dir = OUT.replace("\\", "/")

    results = []
    total_start = time.time()
    for name in names:
        with open(os.path.join(SCENARIOS, name + ".lua"), encoding="utf-8") as f:
            source = f.read()
        if re.search(r"^--\s*@fresh\b", source, re.M) and os.path.exists(sav):
            os.remove(sav)
        m = re.search(r"^--\s*@timeout\s+(\d+)", source, re.M)
        timeout = int(m.group(1)) if m else 120
        for old in glob.glob(os.path.join(OUT, name + "_*.png")) + [os.path.join(OUT, name + ".log")]:
            if os.path.exists(old):
                os.remove(old)

        script = build_script(name, source, syms, offs, out_dir)
        cmd = [a.mgba]
        if not a.slow:
            cmd += ["-C", "videoSync=0", "-C", "audioSync=0"]
        cmd += ["--script", script, rom]
        start = time.time()
        try:
            subprocess.run(cmd, timeout=timeout, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            timed_out = False
        except subprocess.TimeoutExpired:
            timed_out = True
        r = parse_log(os.path.join(OUT, name + ".log"))
        r["time"] = time.time() - start
        if timed_out:
            r["error"] = f"timeout after {timeout}s"
        ok = r["failed"] == 0 and r["error"] is None and r["ended"]
        results.append((name, ok, r))

        status = "ok  " if ok else "FAIL"
        print(f"{status} {name:<28} {r['passed']:3d} passed {r['failed']:3d} failed  {r['time']:5.1f}s")
        if a.verbose and os.path.exists(os.path.join(OUT, name + ".log")):
            with open(os.path.join(OUT, name + ".log"), encoding="utf-8", errors="replace") as f:
                for line in f:
                    print("     " + line.rstrip())
        for fl in r["fails"]:
            print("       FAIL " + fl)
        if r["error"]:
            print("       ERROR " + r["error"])
        elif not r["ended"]:
            print("       scenario did not reach its end")
        if not ok and not a.keep_going:
            break

    n_ok = sum(1 for _, ok, _ in results if ok)
    print(f"\n{n_ok}/{len(results)} scenarios passed in {time.time() - total_start:.0f}s "
          f"(logs and screenshots in {os.path.relpath(OUT, ROOT)})")
    return 0 if n_ok == len(results) == len(names) else 1


if __name__ == "__main__":
    sys.exit(main())
