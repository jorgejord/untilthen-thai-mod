# -*- coding: utf-8 -*-
"""vulgar_sweep.py <chapter> — kid-safe sweep on sheet_<ch>_clean/rough.json.
HARD vulgar removed from BOTH registers. Rough particles (กู/มึง/ว่ะ...) removed from CLEAN only.
Avoids legit words: ทะแม่ง (suspicious), แม่งาน (organizer), กู้ (rescue)."""
import json, re, sys, pathlib
SHEETS = pathlib.Path(r"C:\Users\theze\Desktop\UntilThenModeThailanguse\translation_sheets")

def lines_of(fv): return fv.get("lines", fv) if isinstance(fv, dict) else fv

# (regex, repl) applied to BOTH registers — real vulgarity -> mild
HARD = [
    (r"ช่างแม่ง", "ช่างมัน"),
    (r"(?<!ทะ)แม่ง(?!าน)", "มัน"),
    (r"เชี่ยเอ๊ย|เชี่ย", "เวร"),
    (r"เหี้ยเอ๊ย|ไอ้เหี้ย|เหี้ย", "เวร"),
    (r"ไอ้สัส|สัสๆ|สัส", "ไอ้เวร"),
    (r"ควยไรวะ|ไอ้ควย|ควย", "บ้าอะไร"),
    (r"เย็ด\S*", "บ้าเอ๊ย"),
    (r"สนห่าอะไร|สนห่า", "ไม่สน"),
    (r"ตายห่า", "ให้ตาย"),
    (r"ห่าเหว", "ที่ไหน"),
    (r"ไอ้ห่า", "ไอ้บ้า"),
]
# CLEAN-only: rough particles/pronouns -> clean
CLEAN_ONLY = [
    (r"กู(?!้)", "เรา"),
    (r"มึง", "นาย"),
    (r"ว่ะ", "อ่ะ"),
    (r"เว้ย|โว้ย|เฮ้ย", ""),
    (r"หว่า", "อ่ะ"),
    (r"(?<=[ก-๙])วะ(?=[\s!?.,…\"']|$)", "อ่ะ"),
]

def sweep_text(t, rules):
    for pat, rep in rules:
        t = re.sub(pat, rep, t)
    return re.sub(r"  +", " ", t).strip()

def main(ch):
    for reg in ["clean", "rough"]:
        p = SHEETS / f"sheet_{ch}_{reg}.json"
        d = json.loads(p.read_text(encoding="utf-8"))
        rules = HARD + (CLEAN_ONLY if reg == "clean" else [])
        n = 0
        for f in d:
            for ln in lines_of(d[f]):
                t = ln.get("th") or ""
                if not t: continue
                t2 = sweep_text(t, rules)
                if t2 != t: ln["th"] = t2; n += 1
        p.write_text(json.dumps(d, ensure_ascii=False, indent=1), encoding="utf-8")
        # rescan hard
        left = {}
        for f in d:
            for ln in lines_of(d[f]):
                t = ln.get("th") or ""
                for w in ["เหี้ย", "แม่ง", "สัส", "เชี่ย", "ควย", "เย็ด"]:
                    if w in t and "ทะแม่ง" not in t and "แม่งาน" not in t:
                        left[w] = left.get(w, 0) + 1
        print(f"{ch} {reg}: fixed {n} lines | hard-vulgar remaining: {left or 'none'}")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "1")
