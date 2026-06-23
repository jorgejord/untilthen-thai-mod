#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""สแกนหา 'บทพูดที่ยังเป็นอังกฤษ' (แปลตกหล่น) ทั่วทั้งเกม ทั้ง th และ fil.
ใช้ตัวกรองแบบเดียวกับ find_dlc_gaps3 (ตัดคำสั่ง/asset/ชื่อ proper noun) ขยายครอบทุกบท."""
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
PAY = ROOT / "ThaiMod" / "payload" / "assets" / "story" / "locales"

# ไฟล์ dev/test ที่ผู้เล่นไม่เห็น -> ข้าม
SKIP_FILES = {"base.inkb","dlgtest.inkb","adaptive_sb_test.inkb","geddit_test.inkb",
 "sound_test_fmod.inkb","speech_bubble_test.inkb","compose_reply_test.inkb",
 "compose_reply_test.1c.inkb"}

def strs(p):
    raw = p.read_bytes(); m = raw.find(b"\xff\xff\xff\xff")
    return [s.decode("utf-8","replace") for s in raw[10:(m if m>=0 else len(raw))].split(b"\x00")]

def has_thai(s): return any("฀" <= c <= "๿" for c in s)
ASSET = re.compile(r"assets/|\.png|\.tres|\.event|\.ogg|\.wav|look_at|wait_for|_walk\b|_clip\b")
SPEAKER = re.compile(r"^[A-Z][A-Za-z0-9_]*:\s+(.+)", re.DOTALL)

def gap_text(s):
    t = re.sub(r"\[[^\]]*\]", "", s)               # ตัด [..] tag สมบูรณ์
    if "]" in t:                                    # หางคำสั่ง split
        tail = t.split("]")[-1].strip()
        return tail if re.search(r"[A-Za-z]{3,}", tail) else None
    m = SPEAKER.match(s.strip())
    if m: return m.group(1).strip()
    if s.strip().startswith(":: "): return s.strip()[3:].strip()
    return None

def is_real_dialogue(gt):
    if not gt: return False
    # ต้องมีคำอังกฤษ >=2 คำ (>=3 ตัวอักษร) = น่าจะประโยคจริง ไม่ใช่ชื่อเฉพาะคำเดียว
    words = re.findall(r"[A-Za-z]{3,}", gt)
    return len(words) >= 2

results = {}
for loc in ["th","fil"]:
    base = PAY / loc
    for p in sorted(base.rglob("*.inkb")):
        if p.name in SKIP_FILES: continue
        for s in strs(p):
            if has_thai(s) or s.lstrip().startswith(("$","#")) or ASSET.search(s):
                continue
            gt = gap_text(s)
            if is_real_dialogue(gt):
                rel = p.relative_to(base).as_posix()
                results.setdefault(loc, {}).setdefault(rel, []).append(s.strip())

for loc in ["th","fil"]:
    locres = results.get(loc, {})
    total = sum(len(v) for v in locres.values())
    print(f"\n########## {loc.upper()}: {total} บรรทัดน่าสงสัย ใน {len(locres)} ไฟล์ ##########")
    for rel in sorted(locres):
        print(f"\n[{loc}/{rel}]")
        for s in locres[rel][:30]:
            print(f"   {s[:160]!r}")
