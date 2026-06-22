import json, sys
from pathlib import Path

OUT = sys.argv[1]
res = json.loads(open(OUT, encoding="utf-8").read())['result']
ROOT = Path(__file__).resolve().parent.parent
base = json.loads((ROOT/"UntilThenExtrallPCK"/"assets"/"databases"/"the_liamson_times.json").read_text(encoding="utf-8"))

BSN = chr(92) + 'n'   # literal backslash-n
NL = chr(10)          # real newline

entries = {}
bad = []
for r in res:
    k = r['key']
    body = r['body_th'].replace(BSN, NL)   # literal \n -> real newline
    bb = base[k]['body']
    chk = {
        '[b]': (bb.count('[b]'), body.count('[b]')),
        '[/b]': (bb.count('[/b]'), body.count('[/b]')),
        'para': (bb.count(NL+NL), body.count(NL+NL)),
    }
    diff = {kk: v for kk, v in chk.items() if v[0] != v[1]}
    if diff:
        bad.append((k, diff))
    art = dict(base[k]); art['body'] = body
    entries[k] = art

print("articles:", len(entries), "| mismatches:", len(bad))
for k, d in bad:
    print("  ", k, d)

if bad:
    print("STOP: fix mismatches before writing")
    sys.exit(1)

# merge into existing th + fil (same content for news)
PD = ROOT/"ThaiMod"/"payload"/"assets"/"databases"
for reg in ['th', 'fil']:
    f = PD/f"the_liamson_times_{reg}.json"
    d = json.loads(f.read_text(encoding="utf-8"))
    d.update(entries)
    # keep key order = base order
    ordered = {k: d[k] for k in base if k in d}
    f.write_text(json.dumps(ordered, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"  wrote {reg}: {len(ordered)}/{len(base)} articles")
print("DONE")
