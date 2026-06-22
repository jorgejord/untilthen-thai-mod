#!/usr/bin/env python3
"""find_badtags.py — หา [..] / {..} ในบทพูด payload ที่ "ไม่ใช่แท็กเกมที่ถูกต้อง"
(แท็กแปลกที่คนแปลเผลอใส่ -> เกม parse BBCode พัง/แครช). เทียบ base ด้วยว่าเป็นของแปลใหม่ไหม."""
import sys, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY = {'th':  ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"th",
       'fil': ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"fil"}

BRK = re.compile(r'\[([^\]]*)\]')
# แท็กเกมที่ถูกต้อง (content ภายใน [])
KNOWN = re.compile(r'^(?:wave|/wave|shake\b.*|/shake|\.|\.\.|:.+|\$.*|\++)$', re.S)

def is_known(inner):
    return bool(KNOWN.match(inner))

issues = []
for reg, root in PAY.items():
    for p in sorted(root.rglob("*.inkb")):
        rel = p.relative_to(root).as_posix(); bp = BASE / rel
        if not bp.exists(): continue
        B = [x['text'] for x in core.parse_inkb(bp.read_bytes())['strings']]
        T = [x['text'] for x in core.parse_inkb(p.read_bytes())['strings']]
        if len(B) != len(T): continue
        bset = set()
        for s in B:
            for inner in BRK.findall(s): bset.add(inner)
        for i, s in enumerate(T):
            for inner in BRK.findall(s):
                if not is_known(inner):
                    novel = inner not in bset   # ไม่มีในต้นฉบับเลย = คนแปลใส่ใหม่
                    issues.append((reg, rel, i, inner, novel, s))

print(f"unknown/invalid [tags] in payload dialogue: {len(issues)}")
# จัดกลุ่มตาม novelty
novel = [x for x in issues if x[4]]
print(f"  -> ของที่ 'ไม่มีในต้นฉบับ' (คนแปลใส่เอง เสี่ยงแครช): {len(novel)}")
for reg, rel, i, inner, nv, s in issues[:40]:
    mark = "NEW!" if nv else "(in base too)"
    print(f"  [{reg}] {rel}#{i} {mark}  [<{inner[:40]}>]")
    print(f"        {s[:110]!r}")
if not issues:
    print("✅ ไม่มีแท็กแปลกในบทพูดเลย ทุก [..] เป็นแท็กเกมที่ถูกต้อง")
