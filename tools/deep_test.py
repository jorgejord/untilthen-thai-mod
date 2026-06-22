# -*- coding: utf-8 -*-
"""deep_test.py — full integrity test of the Thai MOD payload.
Checks every shipped file for things that could crash the game or ship broken text:
  1. .inkb idempotent rebuild (parse->rebuild==bytes) for th + fil  -> crash safety
  2. tag audit: dangerous Ink/Godot tags ([wave]/[shake]/[color]/[img]/[font]/[center]/$)
     count must match the English base per string; every payload string bracket-balanced
  3. databases *_th.json / *_fil.json valid JSON
  4. UI text.th.translation / text.fil.translation present
  5. Thai fonts (.fontdata) present
  6. Constants.gd / Game.gd present + carry the Thai-locale hijack
Exit non-zero if any CRITICAL check fails (idempotent / dangerous-tag / bracket / bad JSON)."""
import sys, pathlib, json
ROOT = pathlib.Path(r"C:\Users\theze\Desktop\UntilThenModeThailanguse")
sys.path.insert(0, str(ROOT / "tools"))
import inkb_core as core

BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
PAY  = ROOT / "ThaiMod" / "payload"

# tags whose count MUST be preserved (dropping/adding one can crash or break formatting)
DANGER = ['[wave]', '[/wave]', '[shake', '[/shake]',
          '[color', '[/color]', '[img', '[font', '[/font]',
          '[center]', '[/center]']
COSMETIC = ['[.]', '[..]']  # timing dots — harmless if dropped, reported separately

def danger_counts(s):
    return {t: s.count(t) for t in DANGER}

crit = 0   # critical failures
print("="*64)
print("  DEEP TEST — Until Then Thai MOD")
print("="*64)

# ---------- 1 + 2 : inkb safety + tag audit ----------
for loc in ('th', 'fil'):
    root = PAY / "assets" / "story" / "locales" / loc
    if not root.exists():
        print(f"\n[{loc}] MISSING locale folder!"); crit += 1; continue
    n = ok = 0
    idem_fail = []; tag_fail = []; unbal = []; cosmetic_drops = 0
    for f in root.rglob("*.inkb"):
        n += 1
        rel = f.relative_to(root)
        data = f.read_bytes()
        try:
            p = core.parse_inkb(data)
            section, offmap = core.build_string_section(p['strings'])
            tail = core.patch_binary_offsets(p['binary_tail'], offmap)
            rebuilt = p['header'] + section + tail
        except Exception as e:
            idem_fail.append((str(rel), f"parse/rebuild error: {e}")); continue
        if rebuilt != data:
            idem_fail.append((str(rel), f"NOT idempotent ({len(data)} vs {len(rebuilt)})")); continue
        ok += 1
        # compare tags/brackets vs English base per string (tags may legally span strings,
        # so the valid test is "same as the base", not "self-balanced")
        bp = BASE / rel
        if bp.exists():
            pb = core.parse_inkb(bp.read_bytes())
            if len(pb['strings']) == len(p['strings']):
                for i, (a, b) in enumerate(zip(pb['strings'], p['strings'])):
                    ta, tb = a['text'], b['text']
                    ca, cb = danger_counts(ta), danger_counts(tb)
                    diffs = {k: (ca[k], cb[k]) for k in ca if ca[k] != cb[k]}
                    if diffs:
                        tag_fail.append((str(rel), i, diffs))
                    # raw bracket count must match base, EXCEPT for dropped cosmetic [.]/[..]
                    cos_a = sum(ta.count(c) for c in COSMETIC)
                    cos_b = sum(tb.count(c) for c in COSMETIC)
                    if cos_a > cos_b:
                        cosmetic_drops += cos_a - cos_b
                    # each [.]/[..] drop removes one '[' and one ']'
                    exp_open  = ta.count('[') - (cos_a - cos_b)
                    exp_close = ta.count(']') - (cos_a - cos_b)
                    if tb.count('[') != exp_open or tb.count(']') != exp_close:
                        unbal.append((str(rel), i, f"base[={ta.count('[')} ]={ta.count(']')} -> th[={tb.count('[')} ]={tb.count(']')}"))
    print(f"\n[{loc}]  files={n}  idempotent_OK={ok}")
    print(f"      idempotent FAIL : {len(idem_fail)}")
    print(f"      dangerous-tag FAIL: {len(tag_fail)}")
    print(f"      bracket-count != base : {len(unbal)}")
    print(f"      cosmetic [.] drops (harmless): {cosmetic_drops}")
    for r, e in idem_fail[:8]:  print(f"        ! {r}: {e}")
    for r, i, d in tag_fail[:8]: print(f"        ! {r} #{i}: {d}")
    for r, i, m in unbal[:8]:    print(f"        ! {r} #{i}: {m}")
    crit += len(idem_fail) + len(tag_fail) + len(unbal)

# ---------- 3 : databases ----------
dbdir = PAY / "assets" / "databases"
db = list(dbdir.glob("*_th.json")) + list(dbdir.glob("*_fil.json")) if dbdir.exists() else []
bad = []
for f in db:
    try: json.loads(f.read_text(encoding='utf-8'))
    except Exception as e: bad.append((f.name, str(e)))
print(f"\n[databases]  files={len(db)}  invalid_JSON={len(bad)}")
for nme, e in bad[:8]: print(f"        ! {nme}: {e}")
crit += len(bad)

# ---------- 4 : UI translations ----------
loc = PAY / "assets" / "locales"
ui_th  = (loc / "text.th.translation").exists()
ui_fil = (loc / "text.fil.translation").exists()
print(f"\n[UI]  text.th.translation={ui_th}  text.fil.translation={ui_fil}")

# ---------- 5 : fonts ----------
fonts = list((PAY / ".godot" / "imported").glob("*.fontdata")) if (PAY/".godot"/"imported").exists() else []
print(f"[fonts]  .fontdata files={len(fonts)}")

# ---------- 6 : Constants.gd / Game.gd ----------
cg = list(PAY.rglob("Constants.gd"))
gg = list(PAY.rglob("Game.gd"))
const_ok = game_ok = False
if cg:
    txt = cg[0].read_text(encoding='utf-8', errors='ignore')
    const_ok = ("ภาษาไทย" in txt)
if gg:
    txt = gg[0].read_text(encoding='utf-8', errors='ignore')
    game_ok = ("th" in txt) and ("translation" in txt.lower())
print(f"[scripts]  Constants.gd={'found' if cg else 'MISSING'} (Thai locale={const_ok})  "
      f"Game.gd={'found' if gg else 'MISSING'} (th hook={game_ok})")

print("\n" + "="*64)
print(f"  RESULT: {'PASS ✅  no critical issues' if crit==0 else f'FAIL ❌  {crit} critical issue(s)'}")
print("="*64)
sys.exit(1 if crit else 0)
