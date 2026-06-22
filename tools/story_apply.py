# -*- coding: utf-8 -*-
"""story_apply.py <chapter> — merge agent outputs (ch<ch>_*_th.json) into the
clean/rough sheets, validating per-file array length. Writes sheets back."""
import json, sys, pathlib
ROOT = pathlib.Path(r"C:\Users\theze\Desktop\UntilThenModeThailanguse")
SHEETS = ROOT / "translation_sheets"
WORK = ROOT / "tools" / "story_tasks"

def lines_of(fv):
    return fv.get("lines", fv) if isinstance(fv, dict) else fv

def main(ch):
    clean = json.loads((SHEETS / f"sheet_{ch}_clean.json").read_text(encoding="utf-8"))
    rough = json.loads((SHEETS / f"sheet_{ch}_rough.json").read_text(encoding="utf-8"))
    trans = {}
    for f in sorted(WORK.glob(f"ch{ch}_*_th.json")):
        trans.update(json.loads(f.read_text(encoding="utf-8")))
    applied = 0; warns = []
    for rel, regs in trans.items():
        if rel not in clean:
            warns.append(f"unknown relpath {rel}"); continue
        cl = lines_of(clean[rel]); ro = lines_of(rough[rel])
        ca = regs.get("clean", []); ra = regs.get("rough", [])
        if len(ca) != len(cl) or len(ra) != len(ro):
            warns.append(f"LEN MISMATCH {rel}: file={len(cl)} clean={len(ca)} rough={len(ra)} -> SKIPPED")
            continue
        for i, ln in enumerate(cl):
            if str(ca[i]).strip(): ln["th"] = ca[i]
        for i, ln in enumerate(ro):
            if str(ra[i]).strip(): ln["th"] = ra[i]
        applied += 1
    (SHEETS / f"sheet_{ch}_clean.json").write_text(json.dumps(clean, ensure_ascii=False, indent=1), encoding="utf-8")
    (SHEETS / f"sheet_{ch}_rough.json").write_text(json.dumps(rough, ensure_ascii=False, indent=1), encoding="utf-8")
    # report coverage
    def done(sheet):
        return sum(1 for f in sheet for ln in lines_of(sheet[f]) if (ln.get("th") or "").strip())
    def tot(sheet):
        return sum(len(lines_of(sheet[f])) for f in sheet)
    print(f"ch{ch}: applied {applied} files. clean {done(clean)}/{tot(clean)}, rough {done(rough)}/{tot(rough)}")
    for w in warns: print("  WARN:", w)

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "1")
