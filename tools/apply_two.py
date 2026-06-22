#!/usr/bin/env python3
"""
apply_two.py — เขียนคำแปล 2 เวอร์ชัน (clean/rough) พร้อมกัน
รับ JSON: { "relpath": {"clean": [...], "rough": [...]}, ... }
  - ถ้าไม่มี "rough" → ใช้ค่า clean (บรรทัดที่ไม่ต่างกัน)
เขียนลง sheet_<ch>_clean.json และ sheet_<ch>_rough.json

ใช้: python tools/apply_two.py <ch> <trans2.json>
"""
import sys, json
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
SD = ROOT / "translation_sheets"

def load(p): return json.load(open(p, encoding="utf-8"))
def dump(o, p): json.dump(o, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)

def main():
    ch, tp = sys.argv[1], Path(sys.argv[2])
    sc, sr = SD / f"sheet_{ch}_clean.json", SD / f"sheet_{ch}_rough.json"
    clean, rough, trans = load(sc), load(sr), load(tp)
    nc = nr = 0
    for rel, data in trans.items():
        c = data.get("clean", [])
        r = data.get("rough", c)
        if rel not in clean:
            print(f"  ⚠️ ไม่พบ {rel}"); continue
        if len(c) != len(clean[rel]):
            print(f"  ⚠️ {rel}: clean {len(c)} != sheet {len(clean[rel])} — ข้าม"); continue
        if len(r) != len(clean[rel]):
            print(f"  ⚠️ {rel}: rough {len(r)} != sheet {len(clean[rel])} — ข้าม"); continue
        for i, th in enumerate(c):
            clean[rel][i]['th'] = th
            if th.strip(): nc += 1
        for i, th in enumerate(r):
            rough[rel][i]['th'] = th
            if th.strip(): nr += 1
    dump(clean, sc); dump(rough, sr)
    print(f"✅ clean {nc} บรรทัด → {sc.name} | rough {nr} บรรทัด → {sr.name}")

if __name__ == "__main__":
    main()
