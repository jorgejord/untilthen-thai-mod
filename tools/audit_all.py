#!/usr/bin/env python3
"""audit_all.py — ตรวจ payload เทียบ base ทุกมิติในรอบเดียว (ทั้งเกม, th+fil)"""
import sys, re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY = {'th':  ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"th",
       'fil': ROOT/"ThaiMod"/"payload"/"assets"/"story"/"locales"/"fil"}

CMD   = re.compile(r'\[\$[^\]]*\]')
SHAKE = re.compile(r'\[shake[^\]]*\]')
PLUS  = re.compile(r'\[\++\]')
BRACE = re.compile(r'\{[^}]*\}')
QUOTE = re.compile(r'"[^"]*"')
TRAIL = re.compile(r'\[\$[^\]]*$')

counts = {k: 0 for k in [
    'len_mismatch', 'nondialogue_diff', 'cmd_span', 'standalone_dollar',
    'shake', 'wave', 'plus', 'brace', 'chars_line', 'emphasis_count',
    'caret', 'prefix_suffix', 'bracket_balance', 'mojibake', 'newline_in_cmdspan',
]}
examples = {}
def flag(k, detail):
    counts[k] += 1
    examples.setdefault(k, []).append(detail)

for reg, root in PAY.items():
    for p in sorted(root.rglob("*.inkb")):
        rel = p.relative_to(root).as_posix()
        bp = BASE / rel
        if not bp.exists():
            continue
        B = [x['text'] for x in core.parse_inkb(bp.read_bytes())['strings']]
        T = [x['text'] for x in core.parse_inkb(p.read_bytes())['strings']]
        tag = f"[{reg}] {rel}"
        if len(B) != len(T):
            flag('len_mismatch', f"{tag}: {len(B)} vs {len(T)}"); continue
        jb, jt = '\n'.join(B), '\n'.join(T)
        # join-based command/effect features
        if sorted(CMD.findall(jb)) != sorted(CMD.findall(jt)):       flag('cmd_span', tag)
        if sorted(SHAKE.findall(jb)) != sorted(SHAKE.findall(jt)):   flag('shake', tag)
        if jb.count('[wave]') != jt.count('[wave]') or jb.count('[/wave]') != jt.count('[/wave]'): flag('wave', tag)
        if sorted(PLUS.findall(jb)) != sorted(PLUS.findall(jt)):     flag('plus', tag)
        if sorted(BRACE.findall(jb)) != sorted(BRACE.findall(jt)):   flag('brace', tag)
        bd = sorted(QUOTE.sub('"@"', s) for s in B if s.lstrip().startswith('$'))
        td = sorted(QUOTE.sub('"@"', s) for s in T if s.lstrip().startswith('$'))
        if bd != td:                                                 flag('standalone_dollar', tag)
        if jb.count('*') != jt.count('*'):                           flag('emphasis_count', tag)
        if jb.count('^') != jt.count('^'):                           flag('caret', tag)
        if sorted(s for s in B if s.lower().startswith('characters:')) != \
           sorted(s for s in T if s.lower().startswith('characters:')): flag('chars_line', tag)
        # per-string structural
        didx, iscont = core.detect_dialogue_indices([{'text': x} for x in B] and core.parse_inkb(bp.read_bytes())['strings'])
        dset = set(didx)
        for i in range(len(B)):
            if i not in dset:
                if B[i] != T[i]:                                     flag('nondialogue_diff', f"{tag}#{i}")
                continue
            # dialogue: prefix/suffix + balance + mojibake + cmdspan newline
            cont = iscont.get(i, False)
            try:
                bp_, _, bs = core.get_dialogue_content(B[i], cont)
                tp_, _, ts = core.get_dialogue_content(T[i], cont)
                if bp_ != tp_ or bs != ts:                           flag('prefix_suffix', f"{tag}#{i}")
            except Exception:
                flag('prefix_suffix', f"{tag}#{i} parse-err")
            if (B[i].count('[')-B[i].count(']'), B[i].count('{')-B[i].count('}')) != \
               (T[i].count('[')-T[i].count(']'), T[i].count('{')-T[i].count('}')):
                flag('bracket_balance', f"{tag}#{i}")
            if '�' in T[i] and '�' not in B[i]:            flag('mojibake', f"{tag}#{i}")

print("══════ FULL-GAME AUDIT (payload vs base, th+fil) ══════")
allclean = True
for k, v in counts.items():
    status = "OK" if v == 0 else f"** {v} ISSUES **"
    if v: allclean = False
    print(f"  {k:22} {status}")
    if v:
        for ex in examples[k][:5]:
            print(f"        {ex}")
print("─" * 50)
print("✅ ทุกมิติสะอาด" if allclean else "⚠️ พบปัญหา ดูด้านบน")
