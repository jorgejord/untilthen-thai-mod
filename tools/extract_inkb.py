#!/usr/bin/env python3
"""
extract_inkb.py
---------------
ดึงบทพูดภาษาอังกฤษจากไฟล์ .inkb ออกมาเป็น "translation sheet" (JSON)
ให้ Claude แปล แล้วค่อยใช้ inject_inkb.py ฉีดกลับเป็น .inkb ภาษาไทย

ไม่ใช้ Ollama — Claude เป็นคนเติมช่อง "th" ใน sheet เอง

วิธีใช้:
  python tools/extract_inkb.py            ← ดึงทุก chapter
  python tools/extract_inkb.py --chapter 1  ← ดึงเฉพาะ chapter 1
"""

import os, re, sys, json, argparse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

# ══════════════════════════════════════════════
ROOT       = Path(__file__).resolve().parent.parent
STORY_DIR  = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
SHEET_DIR  = ROOT / "translation_sheets"
# ══════════════════════════════════════════════


def extract_file(inkb_path: Path) -> list:
    """คืน list ของ {i, speaker, full, en} สำหรับบทพูดในไฟล์ (ว่าง = ไม่มีบทพูด)"""
    data    = inkb_path.read_bytes()
    parsed  = core.parse_inkb(data)
    strings = parsed['strings']

    dialogue_idx, is_cont = core.detect_dialogue_indices(strings)
    if not dialogue_idx:
        return []

    lines = []
    for order, si in enumerate(dialogue_idx):
        full = strings[si]['text']
        cont = is_cont.get(si, False)
        _, content, _ = core.get_dialogue_content(full, cont)
        lines.append({
            "i":       order,          # ลำดับในกลุ่มบทพูด (ใช้ map ตอน inject)
            "speaker": core.extract_speaker(full),
            "full":    full,           # ข้อความเต็ม (ไว้ตรวจสอบ + ให้บริบทผู้แปล)
            "en":      content,        # เนื้อหาที่ต้องแปล (ไม่รวม prefix ชื่อผู้พูด)
            "th":      "",             # ← Claude เติมตรงนี้
        })
    return lines


def main():
    ap = argparse.ArgumentParser(description="ดึงบทพูดจาก .inkb เป็น translation sheet")
    ap.add_argument("--chapter", default=None, help="เฉพาะ chapter (เช่น 1, 2, lo, rdvt)")
    args = ap.parse_args()

    if not STORY_DIR.exists():
        print(f"❌ ไม่พบ {STORY_DIR}")
        sys.exit(1)

    SHEET_DIR.mkdir(parents=True, exist_ok=True)

    # หาไฟล์ base ทั้งหมด (ไม่รวม locales/)
    all_files = sorted(
        f for f in STORY_DIR.rglob("*.inkb")
        if "locales" not in f.relative_to(STORY_DIR).parts
    )

    # จัดกลุ่มตาม chapter (top-level folder) — ไฟล์ที่อยู่ราก story/ ใส่กลุ่ม "_root"
    chapters = {}
    for f in all_files:
        rel   = f.relative_to(STORY_DIR)
        group = rel.parts[0] if len(rel.parts) > 1 else "_root"
        chapters.setdefault(group, []).append(f)

    if args.chapter:
        if args.chapter not in chapters:
            print(f"❌ ไม่พบ chapter '{args.chapter}'  มี: {', '.join(sorted(chapters))}")
            sys.exit(1)
        chapters = {args.chapter: chapters[args.chapter]}

    grand_files = 0
    grand_lines = 0

    for group in sorted(chapters):
        files = chapters[group]
        sheet = {}            # relpath -> list[lines]
        n_files_with_text = 0
        n_lines = 0

        for f in files:
            rel = str(f.relative_to(STORY_DIR)).replace("\\", "/")
            lines = extract_file(f)
            if lines:
                sheet[rel] = lines
                n_files_with_text += 1
                n_lines += len(lines)

        if not sheet:
            print(f"  chapter {group:>6}: ไม่มีบทพูด ({len(files)} ไฟล์ logic ล้วน)")
            continue

        out = SHEET_DIR / f"sheet_{group}.json"
        with open(out, "w", encoding="utf-8") as fp:
            json.dump(sheet, fp, ensure_ascii=False, indent=2)

        print(f"  chapter {group:>6}: {n_files_with_text:>4} ไฟล์ / {n_lines:>5} บรรทัด → {out.name}")
        grand_files += n_files_with_text
        grand_lines += n_lines

    print("─" * 56)
    print(f"  รวม: {grand_files} ไฟล์ที่ต้องแปล / {grand_lines} บรรทัด")
    print(f"  sheets อยู่ที่: {SHEET_DIR}")
    print(f"\n  ขั้นต่อไป: ให้ Claude เติมช่อง \"th\" ในแต่ละ sheet แล้วรัน inject_inkb.py")


if __name__ == "__main__":
    main()
