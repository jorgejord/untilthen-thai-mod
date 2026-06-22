# -*- coding: utf-8 -*-
"""validate_inkb.py — structural safety check on injected Thai .inkb files.
For each file under payload/locales/<loc>: parse it, rebuild it (build_string_section +
patch_binary_offsets), and require byte-identical (idempotent). Also: re-decode every
string as UTF-8, and compare #strings + tail length against the English base file.
A file that fails any check could crash the game when that scene loads."""
import sys, pathlib
ROOT = pathlib.Path(r"C:\Users\theze\Desktop\UntilThenModeThailanguse")
sys.path.insert(0, str(ROOT / "tools"))
import inkb_core as core

BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAYLOAD = ROOT / "ThaiMod" / "payload" / "assets" / "story" / "locales"

def rebuild(data):
    p = core.parse_inkb(data)
    section, offmap = core.build_string_section(p['strings'])
    tail = core.patch_binary_offsets(p['binary_tail'], offmap)
    return p['header'] + section + tail, p

def validate_file(path, base_path):
    data = path.read_bytes()
    try:
        rebuilt, p = rebuild(data)
    except Exception as e:
        return f"PARSE/REBUILD ERROR: {e}"
    if rebuilt != data:
        return f"NOT IDEMPOTENT (rebuild != file): len {len(data)} vs {len(rebuilt)}"
    # utf-8 validity of every string
    for i, s in enumerate(p['strings']):
        try:
            (s['text'] if isinstance(s, dict) else s).encode("utf-8")
        except Exception as e:
            return f"BAD UTF-8 in string {i}: {e}"
    # parity vs base
    if base_path.exists():
        pb = core.parse_inkb(base_path.read_bytes())
        if len(pb['strings']) != len(p['strings']):
            return f"STRING COUNT MISMATCH vs base: base {len(pb['strings'])} != {len(p['strings'])}"
        if len(pb['binary_tail']) != len(p['binary_tail']):
            return f"TAIL LEN MISMATCH vs base: base {len(pb['binary_tail'])} != {len(p['binary_tail'])}"
    return None  # OK

def main(locs=("th", "fil")):
    total = 0; ok = 0; fails = []
    for loc in locs:
        root = PAYLOAD / loc
        if not root.exists(): continue
        for f in root.rglob("*.inkb"):
            rel = f.relative_to(root)
            base_path = BASE / rel
            total += 1
            err = validate_file(f, base_path)
            if err is None: ok += 1
            else: fails.append((loc, str(rel), err))
    print(f"validated {total} files: {ok} OK, {len(fails)} FAILED")
    for loc, rel, err in fails[:40]:
        print(f"  [{loc}/{rel}] {err}")
    return len(fails)

if __name__ == "__main__":
    sys.exit(1 if main() else 0)
