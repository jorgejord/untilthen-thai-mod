#!/usr/bin/env python3
"""
analyze_commands.py — วิเคราะห์ทุก "คำสั่ง" ในไฟล์ payload เทียบ base อย่างละเอียด
ครอบคลุม:
  (1) multi-line bracket command   [$ ... ]  (ต่อข้ามหลาย string -> join ด้วย \n)
  (2) standalone command string     '$ ...'  (ทั้ง string เป็นคำสั่ง)
  (3) effect tags                    [wave] [/wave] [shake ...] [/shake]
รายงานทุกจุดที่ไม่ตรง base (ยกเว้น [:...] thought-peek กับ [.]/[..] timing ที่แปล/เพิ่มได้)
"""
import sys, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY = {'th':  ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"th",
       'fil': ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"fil"}

CMD_SPAN  = re.compile(r'\[\$[^\]]*\]')          # [$ ... ] (newlines ok)
SHAKE     = re.compile(r'\[shake[^\]]*\]')
DOLLAR    = re.compile(r'^\s*\$', )

def strings(path):
    return [s['text'] for s in core.parse_inkb(path.read_bytes())['strings']]

QUOTE = re.compile(r'"[^"]*"')
def norm_dollar(s):
    # neutralize translatable quoted content so $ thought "EN" == $ thought "TH"
    return QUOTE.sub('"@"', s)

def features(ss):
    joined = '\n'.join(ss)
    return {
        'cmds':  sorted(CMD_SPAN.findall(joined)),
        'shake': sorted(SHAKE.findall(joined)),
        'wave':  joined.count('[wave]'),  'wave_c':  joined.count('[/wave]'),
        'shk_c': joined.count('[/shake]'),
        'dollar': sorted(norm_dollar(s) for s in ss if s.lstrip().startswith('$')),
    }

def main():
    issues = {}
    for reg, root in PAY.items():
        for p in sorted(root.rglob("*.inkb")):
            rel = p.relative_to(root).as_posix()
            bp = BASE / rel
            if not bp.exists():
                continue
            b = features(strings(bp)); t = features(strings(p))
            probs = []
            if b['cmds'] != t['cmds']:
                miss = [c for c in b['cmds'] if c not in t['cmds']]
                extra = [c for c in t['cmds'] if c not in b['cmds']]
                if miss:  probs.append(("CMD-missing", miss))
                if extra: probs.append(("CMD-extra", extra))
            if b['shake'] != t['shake']:
                probs.append(("SHAKE-diff", (b['shake'], t['shake'])))
            for k in ('wave','wave_c','shk_c'):
                if b[k] != t[k]:
                    probs.append((f"{k}-count", (b[k], t[k])))
            if b['dollar'] != t['dollar']:
                miss = [c for c in b['dollar'] if c not in t['dollar']]
                extra = [c for c in t['dollar'] if c not in b['dollar']]
                if miss:  probs.append(("$cmd-missing", miss))
                if extra: probs.append(("$cmd-extra", extra))
            if probs:
                issues[f"[{reg}] {rel}"] = probs
    print(f"files with command/effect issues: {len(issues)}")
    for key in sorted(issues):
        print(f"\n{key}")
        for kind, detail in issues[key]:
            print(f"   {kind}: {detail!r}"[:300])

if __name__ == '__main__':
    main()
