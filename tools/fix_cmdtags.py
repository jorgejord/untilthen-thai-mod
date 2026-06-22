#!/usr/bin/env python3
"""
fix_cmdtags.py
--------------
บั๊ก: บทพูดที่ลงท้ายด้วย inline command opener ข้ามบรรทัด เช่น  'Sofia: ...[$ F '
post_process() เรียก .strip() -> เว้นวรรคท้าย ('[$ F ' -> '[$ F') หาย
-> คำสั่ง $ F / $ D (คุมสีหน้า/ทิศตัวละคร) พัง -> เกม crash.

วิธีแก้ (ผ่าตัดเฉพาะจุด ไม่แตะคำแปล):
  เทียบ payload กับ base ทีละ string (index ตรงกัน). ถ้า base string ลงท้ายด้วย
  fragment '[$ ... ' ที่ยังไม่ปิด ']' -> บังคับให้ payload ลงท้ายด้วย fragment เดียวกันเป๊ะ.
จากนั้น rebuild + patch offset (marker-safe) + ตรวจ idempotent ก่อนเขียนทับ.
"""
import sys, re
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core
from inject_inkb import patch_binary_offsets_safe, _is_safe_inkb

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY = {
    'th':  ROOT / "ThaiMod" / "payload" / "assets" / "story" / "locales" / "th",
    'fil': ROOT / "ThaiMod" / "payload" / "assets" / "story" / "locales" / "fil",
}
DRY = '--apply' not in sys.argv

# trailing unclosed command opener: '[$' ... to end-of-string with no ']' after it
TRAIL = re.compile(r'\[\$[^\]]*$')


def fix_one(base_path: Path, pay_path: Path):
    b = core.parse_inkb(base_path.read_bytes())['strings']
    parsed = core.parse_inkb(pay_path.read_bytes())
    p = parsed['strings']
    if len(b) != len(p):
        return None, 0, "len-mismatch"
    fixes = []
    for i in range(len(b)):
        bt, pt = b[i]['text'], p[i]['text']
        if bt == pt:
            continue
        mb = TRAIL.search(bt)
        if not mb:
            continue                       # base ไม่มี trailing opener -> ข้าม
        frag = mb.group(0)                 # '[$ F '
        mp = TRAIL.search(pt)
        if mp and mp.group(0) != frag:
            new = pt[:mp.start()] + frag    # คืน fragment ให้ตรง base
            p[i]['text'] = new
            fixes.append((i, repr(pt), repr(new)))
        elif not mp:
            # payload ไม่เหลือ opener เลย (โดน strip ทั้งก้อน) -> เติมต่อท้าย
            p[i]['text'] = pt + frag
            fixes.append((i, repr(pt), repr(p[i]['text'])))
    if not fixes:
        return None, 0, "ok"
    sec, off = core.build_string_section(p)
    tail = patch_binary_offsets_safe(parsed['binary_tail'], off)
    out = parsed['header'] + sec + tail
    if not _is_safe_inkb(out):
        return None, len(fixes), "UNSAFE"
    return out, len(fixes), "fixed"


def main():
    total_files = total_fixes = 0
    for reg, root in PAY.items():
        for p in sorted(root.rglob("*.inkb")):
            rel = p.relative_to(root).as_posix()
            bp = BASE / rel
            if not bp.exists():
                continue
            out, n, status = fix_one(bp, p)
            if status in ("ok",):
                continue
            if status == "UNSAFE":
                print(f"  !! [{reg}] {rel}: {n} fixes but REBUILD UNSAFE - skipped")
                continue
            if status == "len-mismatch":
                print(f"  !! [{reg}] {rel}: string count mismatch - skipped")
                continue
            print(f"  [{reg}] {'FIX' if not DRY else 'would-fix'} {n:2}  {rel}")
            for i, old, new in n and [] or []:
                pass
            total_files += 1
            total_fixes += n
            if not DRY and out is not None:
                p.write_bytes(out)
    print("─" * 60)
    print(f"{'DRY-RUN ' if DRY else ''}{total_files} files, {total_fixes} trailing-command fixes")
    if DRY:
        print("rerun with --apply to write")


if __name__ == '__main__':
    main()
