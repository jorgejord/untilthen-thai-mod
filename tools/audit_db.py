#!/usr/bin/env python3
"""audit_db.py — ตรวจฐานข้อมูลแปล (_th/_fil) เทียบ base อังกฤษ
  (1) JSON valid
  (2) โครงสร้างตรง base (keys/ความยาว array/ชนิด/ค่าที่ไม่ใช่ string เท่ากันเป๊ะ)
  (3) token พิเศษใน string คงครบ ({var} %s @mention #tag http... <html> [tag] \\n)
"""
import sys, re, json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "databases"
PDB  = ROOT / "ThaiMod" / "payload" / "assets" / "databases"
DBS = ["cc","email","fb","geddit","landimu","matchy","minds_alike","text","the_liamson_times","web"]

TOKEN = re.compile(r'\{[^}]*\}|%[sd0-9.]*[sdf]|@[A-Za-z0-9_]+|#[A-Za-z0-9_]+|https?://\S+|<[^>]+>|\[[^\]]+\]|\\n|\\t')

struct_issues = []
token_issues  = []

def walk(b, t, path):
    if type(b) != type(t):
        struct_issues.append(f"{path}: type {type(b).__name__} -> {type(t).__name__}"); return
    if isinstance(b, dict):
        if set(b) != set(t):
            miss=set(b)-set(t); extra=set(t)-set(b)
            struct_issues.append(f"{path}: keys differ miss={list(miss)[:4]} extra={list(extra)[:4]}")
        for k in b:
            if k in t: walk(b[k], t[k], f"{path}/{k}")
    elif isinstance(b, list):
        if len(b) != len(t):
            struct_issues.append(f"{path}: list len {len(b)} -> {len(t)}"); return
        for i,(x,y) in enumerate(zip(b,t)):
            walk(x, y, f"{path}[{i}]")
    elif isinstance(b, str):
        bt, tt = sorted(TOKEN.findall(b)), sorted(TOKEN.findall(t))
        if bt != tt:
            token_issues.append(f"{path}: base={bt} pay={tt}")
    else:
        if b != t:
            struct_issues.append(f"{path}: scalar {b!r} -> {t!r}")

for reg in ['th','fil']:
    for db in DBS:
        bf = BASE / f"{db}.json"
        pf = PDB / f"{db}_{reg}.json"
        if not pf.exists():
            struct_issues.append(f"[{reg}] {db}: payload missing"); continue
        try:
            b = json.loads(bf.read_text(encoding='utf-8'))
        except Exception as e:
            struct_issues.append(f"base {db}: invalid JSON {e}"); continue
        try:
            t = json.loads(pf.read_text(encoding='utf-8'))
        except Exception as e:
            struct_issues.append(f"[{reg}] {db}: INVALID JSON {e}"); continue
        walk(b, t, f"[{reg}]{db}")

print(f"=== STRUCTURE issues: {len(struct_issues)} ===")
for s in struct_issues[:40]: print("  ", s)
print(f"\n=== TOKEN-preservation issues: {len(token_issues)} ===")
for s in token_issues[:40]: print("  ", s)
if not struct_issues and not token_issues:
    print("\n✅ ฐานข้อมูลทั้ง 10 ชุด (th+fil): JSON valid, โครงตรง base, token พิเศษคงครบ")
