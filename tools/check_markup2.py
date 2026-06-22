#!/usr/bin/env python3
"""ตรวจ markup ที่ยังไม่เช็ค: characters:{...}, [+]/[++], *emphasis*, braces"""
import sys, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY = {'th':  ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"th",
       'fil': ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"fil"}

PLUS  = re.compile(r'\[\++\]')          # [+] [++] [+++]
BRACE = re.compile(r'\{[^}]*\}')        # { "Mark": "MarkBorja" }

def feats(ss):
    j = '\n'.join(ss)
    return {
        'plus':  sorted(PLUS.findall(j)),
        'brace': sorted(BRACE.findall(j)),
        'star':  j.count('*'),
        'caret': j.count('^'),
        'chars_lines': sorted(s for s in ss if s.lstrip().lower().startswith('characters:')),
    }

issues = {}
for reg, root in PAY.items():
    for p in sorted(root.rglob("*.inkb")):
        rel = p.relative_to(root).as_posix()
        bp = BASE / rel
        if not bp.exists():
            continue
        b = feats([x['text'] for x in core.parse_inkb(bp.read_bytes())['strings']])
        t = feats([x['text'] for x in core.parse_inkb(p.read_bytes())['strings']])
        probs = []
        if b['plus'] != t['plus']:   probs.append(('PLUS', b['plus'], t['plus']))
        if b['brace'] != t['brace']: probs.append(('BRACE', b['brace'], t['brace']))
        if b['star'] != t['star']:   probs.append(('STAR-count', b['star'], t['star']))
        if b['caret'] != t['caret']: probs.append(('CARET-count', b['caret'], t['caret']))
        if b['chars_lines'] != t['chars_lines']:
            probs.append(('characters-line', b['chars_lines'], t['chars_lines']))
        if probs:
            issues[f"[{reg}] {rel}"] = probs

print(f"files with unchecked-markup issues: {len(issues)}")
for k in sorted(issues):
    print(f"\n{k}")
    for kind, bv, tv in issues[k]:
        print(f"   {kind}:")
        print(f"      base={bv!r}"[:160])
        print(f"      pay ={tv!r}"[:160])
if not issues:
    print("✅ characters:{...} / [+] / *emphasis* / ^ ตรง base 100% ทั้งเกม")
