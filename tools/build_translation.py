#!/usr/bin/env python3
# build_translation.py — write a Godot 4 .translation (binary Resource) WITHOUT Godot.
# Format (reverse-engineered): [263-byte fixed prefix: RSRC header + string table +
# int-resource table + "Translation" + propcount(3) + messages-name-idx + DICT type(26)]
# then: dict_count, {String key, StringName value}..., locale(String), script(Nil), trailing "RSRC".
import struct, json, argparse
from pathlib import Path

def vstr(t, s):  # variant: type(t) + len(incl null) + utf8 + null
    b = s.encode('utf-8')
    return struct.pack('<I', t) + struct.pack('<I', len(b)+1) + b + b'\x00'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', required=True)
    ap.add_argument('--locale', default='th')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    if not Path(a.json).exists():
        print(f"(no UI translation at {a.json} — skipping UI build)")
        return
    data = json.loads(Path(a.json).read_text(encoding='utf-8'))
    if not data:
        print("(UI translation is empty — skipping)")
        return
    prefix = (Path(__file__).resolve().parent / 'tr_prefix.bin').read_bytes()
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    out = bytearray(prefix)
    out += struct.pack('<I', len(data))                 # messages dict count
    for k, v in data.items():
        out += vstr(5, str(k))                          # key   = String(5)
        out += vstr(44, '' if v is None else str(v))    # value = StringName(44)
    out += struct.pack('<I', 3) + vstr(5, a.locale)     # prop locale (idx 3) = String
    out += struct.pack('<I', 4) + struct.pack('<I', 1)  # prop script (idx 4) = Nil
    out += b'RSRC'                                       # end-of-file marker
    Path(a.out).write_bytes(out)
    print(f"wrote {a.out}  ({len(data)} keys, locale={a.locale}, {len(out)} bytes)")

if __name__ == '__main__':
    main()
