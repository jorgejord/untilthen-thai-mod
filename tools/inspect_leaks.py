#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""พิมพ์ string เต็มของจุดที่ตัวสแกนหลัก flag (logic เดียวกัน) เทียบกับ base อังกฤษ"""
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
PAY = ROOT / "ThaiMod" / "payload" / "assets" / "story" / "locales"
BASE = ROOT / "UntilThenExtrallPCK" / "assets" / "story"

def strs(p):
    raw = p.read_bytes(); m = raw.find(b"\xff\xff\xff\xff")
    return [s.decode("utf-8","replace") for s in raw[10:(m if m>=0 else len(raw))].split(b"\x00")]
def has_thai(s): return any("฀" <= c <= "๿" for c in s)
ASSET = re.compile(r"assets/|\.png|\.tres|\.event|\.ogg|\.wav|look_at|wait_for|_walk\b|_clip\b|nookface\.com|Ctrl\+")
SPEAKER = re.compile(r"^[A-Z][A-Za-z0-9_]*:\s+(.+)", re.DOTALL)
def gap_text(s):
    t = re.sub(r"\[[^\]]*\]", "", s)
    if "]" in t:
        tail = t.split("]")[-1].strip()
        return tail if re.search(r"[A-Za-z]{3,}", tail) else None
    m = SPEAKER.match(s.strip())
    if m: return m.group(1).strip()
    if s.strip().startswith(":: "): return s.strip()[3:].strip()
    return None
def is_dlg(gt):
    if not gt: return False
    return len(re.findall(r"[A-Za-z]{3,}", gt)) >= 2

for rel in ["10/7/FlashA/1d.inkb","4/1a/1d.inkb","4/2a/1d.inkb","4/2b/1d.inkb","4/3a/1c.inkb",
 "5/2/$.inkb","5/5/Awake.1d.inkb","5/9/Letter.1d.inkb","8/3b/Gametime.1d.inkb",
 "8/4d/1d.inkb","lo/1/1b/1c.inkb","lo/3/2b/1d.inkb","lo/6/1/4c.inkb","6/3/1d.inkb"]:
    p = PAY / "th" / rel
    if not p.exists(): print(f"\n### MISSING {rel}"); continue
    print(f"\n### {rel}")
    for s in strs(p):
        if has_thai(s) or s.lstrip().startswith(("$","#")) or ASSET.search(s): continue
        gt = gap_text(s)
        if is_dlg(gt):
            print(f"    TH-file> {s!r}")
