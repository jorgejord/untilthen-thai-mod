import os, re, sys
BASE = "UntilThenExtrallPCK/assets/story"
roots = {'th': "ThaiMod/payload/assets/story/locales/th",
         'fil': "ThaiMod/payload/assets/story/locales/fil"}

def strings(path):
    d = open(path, 'rb').read()
    m = d.find(b'\xff\xff\xff\xff')
    sec = d[10:m] if m > 0 else d[10:]
    return [s.decode('utf-8', 'replace') for s in sec.split(b'\x00') if s]

def cmdtags(ss):
    return list(re.findall(r'\[\$[^\]]*\]', '\n'.join(ss)))

def norm(s):
    return re.sub(r'\s+', ' ', s).strip()

bad = {}
for reg, root in roots.items():
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if not fn.endswith('.inkb'):
                continue
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, root).replace(os.sep, '/')
            bp = os.path.join(BASE, rel)
            if not os.path.exists(bp):
                continue
            bt = cmdtags(strings(bp))
            tt = cmdtags(strings(p))
            if bt == tt:
                continue
            # whitespace-only normalization: same multiset after collapsing spaces, but raw differs
            if sorted(map(norm, bt)) == sorted(map(norm, tt)) and sorted(bt) != sorted(tt):
                bad.setdefault(rel, set()).add(reg)

print(f"files with whitespace-altered [$ ] command tags: {len(bad)}")
for rel in sorted(bad):
    print(f"   {rel}   ({'+'.join(sorted(bad[rel]))})")
