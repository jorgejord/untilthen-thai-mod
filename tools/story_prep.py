# -*- coding: utf-8 -*-
"""story_prep.py <chapter> — dump untranslated story files into agent-input chunks.
Reads sheet_<ch>_clean.json (structure+en). Groups untranslated files into chunks of
~MAXLINES lines. Each chunk: {relpath: [{i, speaker, full, en}, ...]} (ALL lines of the file).
Agent returns {relpath: {clean:[content...], rough:[content...]}} full arrays."""
import json, sys, pathlib
ROOT = pathlib.Path(r"C:\Users\theze\Desktop\UntilThenModeThailanguse")
SHEETS = ROOT / "translation_sheets"
WORK = ROOT / "tools" / "story_tasks"
MAXLINES = 480

def lines_of(fileval):
    return fileval.get("lines", fileval) if isinstance(fileval, dict) else fileval

def main(ch):
    clean = json.loads((SHEETS / f"sheet_{ch}_clean.json").read_text(encoding="utf-8"))
    WORK.mkdir(parents=True, exist_ok=True)
    # collect files that have >=1 untranslated line
    todo = []
    for rel, fv in clean.items():
        lns = lines_of(fv)
        if any(not (ln.get("th") or "").strip() for ln in lns):
            todo.append((rel, lns))
    # chunk by accumulated line count
    chunks = []
    cur = {}
    cur_n = 0
    for rel, lns in todo:
        if cur_n and cur_n + len(lns) > MAXLINES:
            chunks.append(cur); cur = {}; cur_n = 0
        cur[rel] = [{"i": ln["i"], "speaker": ln.get("speaker",""), "full": ln["full"], "en": ln["en"]} for ln in lns]
        cur_n += len(lns)
    if cur: chunks.append(cur)
    # write
    for i, c in enumerate(chunks):
        (WORK / f"ch{ch}_{i:02d}.json").write_text(json.dumps(c, ensure_ascii=False, indent=1), encoding="utf-8")
    total = sum(len(lns) for _, lns in todo)
    print(f"ch{ch}: {len(todo)} files / {total} untranslated lines -> {len(chunks)} chunks")
    for i, c in enumerate(chunks):
        n = sum(len(v) for v in c.values())
        print(f"  ch{ch}_{i:02d}.json : {len(c)} files, {n} lines")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "1")
