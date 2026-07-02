#!/usr/bin/env python3
"""
inject_inkb.py
--------------
อ่าน translation sheet (ที่ Claude เติมช่อง "th" แล้ว) → สร้างไฟล์ .inkb ภาษาไทย
ลงในโครงสร้าง mod: ThaiMod/assets/story/locales/th/...

ใช้ฟังก์ชัน .inkb เดิม (ผ่าน inkb_core) ตัวเดียวกับตอน extract → index ตรงกันเป๊ะ

วิธีใช้:
  python tools/inject_inkb.py                 ← ฉีดทุก sheet ที่แปลแล้ว
  python tools/inject_inkb.py --chapter 1     ← เฉพาะ chapter 1
  python tools/inject_inkb.py --selftest 1    ← โหมดทดสอบ: round-trip ด้วย en (ไม่แปล)
                                                 เทียบ byte กับ base เพื่อพิสูจน์ว่า logic ไม่พัง
"""

import os, re, sys, json, shutil, argparse, struct
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core
import oracle_patch


def patch_binary_offsets_safe(binary_tail: bytes, offset_map: dict) -> bytes:
    """เหมือน core.patch_binary_offsets แต่ 'ห้ามแตะ' ไบต์ที่เป็น marker \\xff\\xff\\xff\\xff.
    บั๊กเดิม: sliding window อ่านไบต์ที่คาบเกี่ยว marker (เช่น \\xff\\x00\\x00\\x00 = 255)
    แล้ว patch เป็น offset ใหม่ → marker พัง → parse หาขอบผิด → เกม crash."""
    src = bytes(binary_tail)
    result = bytearray(binary_tail)
    patchable = {o: n for o, n in offset_map.items() if o != 0 and o != n}
    if not patchable:
        return bytes(result)
    protected = set()
    i = src.find(b'\xff\xff\xff\xff')
    while i != -1:
        protected.update(range(i, i + 4))
        i = src.find(b'\xff\xff\xff\xff', i + 1)
    # โครงสร้าง bytecode = instruction <type:u32><operand:u32>
    # string reference จริง = operand ที่ตามหลัง 'instruction type' (ค่าเล็ก < 256, เป็น
    # byte เดียว เช่น 0, 7, 24). false-match = ค่าที่บังเอิญตรง offset แต่ "นำหน้าด้วยค่าใหญ่"
    # (เป็น data/offset bytes ไม่ใช่ type เช่น 655360, 1463).
    # เดิม sliding-window patch 'ทุก' ค่าที่ตรง offset → ไป patch พารามิเตอร์คำสั่งที่บังเอิญ
    # ตรง → คำสั่งพัง → ตัวละครเซ็ตอัพพัง → custom_aabb crash
    # FIX: patch เฉพาะ operand ที่ instruction-type (u32 นำหน้า) < 256
    for i in range(4, len(src) - 3):
        if i in protected or i + 1 in protected or i + 2 in protected or i + 3 in protected:
            continue
        prec = struct.unpack_from('<I', src, i - 4)[0]
        if prec >= 256:           # นำหน้าด้วยค่าใหญ่ = ไม่ใช่ instruction type = false-match
            continue
        val = struct.unpack_from('<I', src, i)[0]
        if val in patchable:
            struct.pack_into('<I', result, i, patchable[val])
    return bytes(result)

# ══════════════════════════════════════════════
ROOT      = Path(__file__).resolve().parent.parent
STORY_DIR = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
SHEET_DIR = ROOT / "translation_sheets"
MOD_TH_DIR = ROOT / "ThaiMod" / "payload" / "assets" / "story" / "locales" / "th"
# ══════════════════════════════════════════════


def build_inkb(base_path: Path, lines: list, selftest: bool = False) -> bytes:
    """สร้าง bytes ของ .inkb ภาษาไทยจาก base + คำแปลใน lines"""
    data    = base_path.read_bytes()
    parsed  = core.parse_inkb(data)
    strings = parsed['strings']

    dialogue_idx, is_cont = core.detect_dialogue_indices(strings)

    if len(dialogue_idx) != len(lines):
        raise ValueError(
            f"จำนวนบทพูดไม่ตรง: base มี {len(dialogue_idx)} แต่ sheet มี {len(lines)}")

    for order, si in enumerate(dialogue_idx):
        full = strings[si]['text']
        cont = is_cont.get(si, False)
        _, en_content, _ = core.get_dialogue_content(full, cont)

        if selftest:
            new_content = en_content                 # round-trip เดิม
        else:
            th = (lines[order].get('th') or '').strip()
            if not th:
                new_content = en_content             # ยังไม่แปล → คงอังกฤษ (fallback)
            else:
                new_content = core.post_process(en_content, th, full)

        strings[si]['text'] = core.rebuild_dialogue(full, new_content, is_continuation=cont)

    rel = base_path.resolve().relative_to(STORY_DIR.resolve()).as_posix()
    new_section, offset_map = core.build_string_section(strings)
    new_binary = oracle_patch.patch(rel, parsed['binary_tail'], offset_map)
    return parsed['header'] + new_section + new_binary


def _is_safe_inkb(out_bytes: bytes) -> bool:
    """โครงสร้างปลอดภัยไหม: parse→rebuild ต้อง byte-identical (idempotent).
    ถ้าไม่ผ่าน = offset/marker อาจเพี้ยน → เสี่ยงเกม crash → ห้าม ship."""
    try:
        p = core.parse_inkb(out_bytes)
        sec, off = core.build_string_section(p['strings'])
        tail = core.patch_binary_offsets(p['binary_tail'], off)
        return (p['header'] + sec + tail) == out_bytes
    except Exception:
        return False


def run(chapter: str = None, selftest: bool = False,
        sheet_path: str = None, out_dir: str = None):
    global MOD_TH_DIR
    if out_dir:
        MOD_TH_DIR = Path(out_dir)

    if sheet_path:
        sheets = [Path(sheet_path)]
        if not sheets[0].exists():
            print(f"❌ ไม่พบ {sheet_path}")
            sys.exit(1)
    else:
        if not SHEET_DIR.exists():
            print(f"❌ ไม่พบ {SHEET_DIR} — รัน extract_inkb.py ก่อน")
            sys.exit(1)
        sheets = sorted(SHEET_DIR.glob("sheet_*.json"))
        if chapter:
            target = SHEET_DIR / f"sheet_{chapter}.json"
            if not target.exists():
                print(f"❌ ไม่พบ {target.name}")
                sys.exit(1)
            sheets = [target]

    n_files = n_lines = n_err = n_fallback = 0
    selftest_diffs = []

    for sheet_path in sheets:
        with open(sheet_path, encoding="utf-8") as f:
            sheet = json.load(f)

        for rel, lines in sheet.items():
            base_path = STORY_DIR / rel
            if not base_path.exists():
                print(f"  ⚠️  ไม่พบ base: {rel}")
                n_err += 1
                continue

            # ข้ามไฟล์ที่ยังไม่แปลเลย → ปล่อยให้เกม fallback เป็นอังกฤษ (mod เล็กลง)
            if not selftest and not any((l.get('th') or '').strip() for l in lines):
                continue

            try:
                out_bytes = build_inkb(base_path, lines, selftest=selftest)
            except Exception as e:
                print(f"  ❌ {rel}: {e}")
                n_err += 1
                continue

            if selftest:
                orig = base_path.read_bytes()
                if out_bytes != orig:
                    selftest_diffs.append((rel, len(orig), len(out_bytes)))
                n_files += 1
                continue

            # 🛡️ safety gate: operand ทุกตัวต้องชี้จุดเริ่มสตริง (ไม่ชี้กลางตัว UTF-8 = เกมค้าง)
            # + idempotent. ไม่ผ่าน → fallback อังกฤษ
            translated = out_bytes
            ok_mid, n_mid = oracle_patch.validate_no_midchar(rel, out_bytes)
            if not ok_mid or not _is_safe_inkb(out_bytes):
                why = f"midchar={n_mid}" if not ok_mid else "idempotent FAIL"
                print(f"  🛡️  {rel}: {why} → ใช้อังกฤษแทน (ปลอดภัย ไม่ ship ไฟล์เสี่ยง)")
                translated = base_path.read_bytes()
                n_fallback += 1

            dst = MOD_TH_DIR / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(translated)
            n_files += 1
            if translated is out_bytes:
                n_lines += sum(1 for l in lines if (l.get('th') or '').strip())

    print("─" * 56)
    if selftest:
        print(f"  SELFTEST: round-trip {n_files} ไฟล์ด้วยข้อความเดิม (en)")
        if not selftest_diffs:
            print(f"  ✅ ทุกไฟล์ byte-identical กับ base → binary logic ปลอดภัย")
        else:
            print(f"  ⚠️  {len(selftest_diffs)} ไฟล์ byte ต่างจาก base:")
            for rel, a, b in selftest_diffs[:15]:
                print(f"       {rel}: base={a} out={b}")
    else:
        print(f"  ✅ ฉีดเสร็จ: {n_files} ไฟล์ / {n_lines} บรรทัดที่แปล")
        if n_fallback:
            print(f"  🛡️  fallback อังกฤษ (กัน crash): {n_fallback} ไฟล์")
        print(f"  ผลลัพธ์: {MOD_TH_DIR}")
    if n_err:
        print(f"  ❌ ผิดพลาด {n_err} ไฟล์")


def main():
    ap = argparse.ArgumentParser(description="ฉีดคำแปลกลับเป็น .inkb ภาษาไทย")
    ap.add_argument("--chapter", default=None)
    ap.add_argument("--sheet", default=None, help="ระบุไฟล์ sheet โดยตรง (เช่น sheet_1_rough.json)")
    ap.add_argument("--out", default=None, help="โฟลเดอร์ปลายทาง (เช่น ThaiMod_rough/payload/.../th)")
    ap.add_argument("--selftest", nargs="?", const="__ALL__", default=None,
                    help="โหมดทดสอบ round-trip (ระบุ chapter หรือเว้นว่างเพื่อทั้งหมด)")
    args = ap.parse_args()

    if args.selftest is not None:
        ch = None if args.selftest == "__ALL__" else args.selftest
        run(chapter=ch, selftest=True)
    else:
        run(chapter=args.chapter, selftest=False,
            sheet_path=args.sheet, out_dir=args.out)


if __name__ == "__main__":
    main()
