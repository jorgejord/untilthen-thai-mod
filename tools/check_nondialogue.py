#!/usr/bin/env python3
"""
check_nondialogue.py — ตรวจขั้นเด็ดขาด:
string ที่ "ไม่ใช่บทพูด" (= ไม่อยู่ใน dialogue_idx) ต้อง byte-identical กับ base ทุกตัว.
ถ้าต่าง = มีคำสั่ง/พารามิเตอร์/ชื่อ โดน pipeline แก้ผิด -> เสี่ยง crash. รายงานทุกจุด.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY = {'th':  ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"th",
       'fil': ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"fil"}

bad = []
for reg, root in PAY.items():
    for p in sorted(root.rglob("*.inkb")):
        rel = p.relative_to(root).as_posix()
        bp = BASE / rel
        if not bp.exists():
            continue
        b = core.parse_inkb(bp.read_bytes())['strings']
        t = core.parse_inkb(p.read_bytes())['strings']
        if len(b) != len(t):
            bad.append((reg, rel, -1, "LEN", len(b), len(t)))
            continue
        didx, _ = core.detect_dialogue_indices(b)
        dset = set(didx)
        for i in range(len(b)):
            if i in dset:
                continue                       # บทพูด -> แปลได้ ข้าม
            if b[i]['text'] != t[i]['text']:
                bad.append((reg, rel, i, b[i]['text'], t[i]['text']))

print(f"non-dialogue strings that differ from base: {len(bad)}")
for item in bad[:40]:
    reg, rel, i = item[0], item[1], item[2]
    print(f"  [{reg}] {rel}#{i}")
    print(f"     BASE={item[3]!r}")
    print(f"     PAY ={item[4]!r}")
if not bad:
    print("✅ ทุก string ที่ไม่ใช่บทพูด byte-identical กับ base — ไม่มีคำสั่งใดถูกแตะเลย")
