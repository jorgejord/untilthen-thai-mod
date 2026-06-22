#!/usr/bin/env python3
"""
make_review.py — สร้างเอกสารเทียบ EN | ไม่หยาบ | หยาบ สำหรับตรวจ
ใช้: python tools/make_review.py <ch>  → CH<ch>_REVIEW.md
"""
import sys, json
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
SD = ROOT / "translation_sheets"

def main():
    ch = sys.argv[1]
    clean = json.load(open(SD / f"sheet_{ch}_clean.json", encoding="utf-8"))
    rough = json.load(open(SD / f"sheet_{ch}_rough.json", encoding="utf-8"))
    out = [f"# Chapter {ch} — ตรวจคำแปล (EN / ไม่หยาบ / หยาบ)\n"]
    done = 0
    for rel in clean:
        cl, rl = clean[rel], rough.get(rel, clean[rel])
        if not any((l.get('th') or '').strip() for l in cl):
            continue
        out.append(f"\n## `{rel}`\n")
        for i, l in enumerate(cl):
            sp = l.get('speaker') or ('::' if l['full'].startswith('::') else '')
            en = l['en']
            c = (cl[i].get('th') or '').strip() or "—"
            r = (rl[i].get('th') or '').strip() or "—"
            tag = f" *({sp})*" if sp else ""
            out.append(f"**{i}.**{tag} `{en}`")
            if c == r:
                out.append(f"  → {c}")
            else:
                out.append(f"  → ไม่หยาบ: {c}")
                out.append(f"  → หยาบ: {r}")
            out.append("")
            done += 1
    p = ROOT / f"CH{ch}_REVIEW.md"
    p.write_text("\n".join(out), encoding="utf-8")
    print(f"✅ {done} บรรทัด → {p}")

if __name__ == "__main__":
    main()
