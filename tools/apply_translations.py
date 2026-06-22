#!/usr/bin/env python3
"""
apply_translations.py
----------------------
เขียนคำแปลภาษาไทยลงในช่อง "th" ของ translation sheet

รับไฟล์ JSON รูปแบบ:  { "relpath/file.inkb": ["แปลบรรทัด0", "แปลบรรทัด1", ...], ... }
(ลำดับใน list ต้องตรงกับลำดับ "i" ใน sheet — คือลำดับบทพูดในไฟล์)

หรือรูปแบบ index:      { "relpath/file.inkb": {"0": "แปล", "3": "แปล"}, ... }

วิธีใช้:
  python tools/apply_translations.py <chapter> <translations.json>
  เช่น: python tools/apply_translations.py 1 my_th_ch1.json
"""

import sys, json
from pathlib import Path

ROOT      = Path(__file__).resolve().parent.parent
SHEET_DIR = ROOT / "translation_sheets"


def main():
    if len(sys.argv) < 3:
        print("ใช้: python tools/apply_translations.py <chapter> <translations.json>")
        sys.exit(1)

    chapter = sys.argv[1]
    trans_path = Path(sys.argv[2])
    # arg 3 (optional) = ระบุไฟล์ sheet โดยตรง (สำหรับเวอร์ชัน clean/rough)
    if len(sys.argv) >= 4:
        sheet_path = Path(sys.argv[3])
    else:
        sheet_path = SHEET_DIR / f"sheet_{chapter}.json"

    if not sheet_path.exists():
        print(f"❌ ไม่พบ {sheet_path}")
        sys.exit(1)
    if not trans_path.exists():
        print(f"❌ ไม่พบ {trans_path}")
        sys.exit(1)

    with open(sheet_path, encoding="utf-8") as f:
        sheet = json.load(f)
    with open(trans_path, encoding="utf-8") as f:
        trans = json.load(f)

    n_files = n_lines = n_skip = 0
    for rel, data in trans.items():
        if rel not in sheet:
            print(f"  ⚠️  ไม่พบไฟล์ใน sheet: {rel}")
            n_skip += 1
            continue
        lines = sheet[rel]

        if isinstance(data, list):
            if len(data) != len(lines):
                print(f"  ⚠️  {rel}: จำนวนไม่ตรง (sheet {len(lines)} vs trans {len(data)}) — ข้าม")
                n_skip += 1
                continue
            for i, th in enumerate(data):
                lines[i]['th'] = th
                if th.strip():
                    n_lines += 1
        elif isinstance(data, dict):
            for k, th in data.items():
                idx = int(k)
                if 0 <= idx < len(lines):
                    lines[idx]['th'] = th
                    if th.strip():
                        n_lines += 1
        n_files += 1

    with open(sheet_path, "w", encoding="utf-8") as f:
        json.dump(sheet, f, ensure_ascii=False, indent=2)

    print(f"✅ เขียนคำแปล: {n_files} ไฟล์ / {n_lines} บรรทัด → {sheet_path.name}")
    if n_skip:
        print(f"   ข้าม {n_skip} รายการ")


if __name__ == "__main__":
    main()
