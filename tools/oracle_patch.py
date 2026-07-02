#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""oracle_patch — patcher offset แบบ 'ground truth' แทน heuristic prec<256 ที่เปราะ.

ปัญหาเดิม (เกมค้าง): `patch_binary_offsets_safe` ข้าม operand ที่ prec>=256 → 516 ไฟล์/
2039 operand ไม่ถูก patch → ชี้ offset อังกฤษเดิม → ในไฟล์ไทย offset เลื่อน → ชี้กลางตัว
UTF-8 → Godot "Invalid UTF-8 leading byte" → ค้าง.

แนวคิด: เกมแถมไฟล์ .inkb ทางการ (de/ja/es/fr/it/zh-CN). ความยาว binary_tail ของ locale
ทางการ == base เป๊ะ (การ patch offset ไม่เปลี่ยนความยาว tail) → ตำแหน่ง byte ตรงกันทุก locale.
ตำแหน่งที่ base != ทางการ และค่า base เป็น 'จุดเริ่มสตริง' = operand string-offset 'จริง' แน่นอน.
union ข้าม 6 locale = เซ็ต operand ที่ครบเกือบสมบูรณ์ (operand ของสตริงที่ขยับใน locale ใดก็ตาม).
เสริมด้วย prec<256 backup (จับ operand ของสตริงที่ 'ไม่ขยับเลย' ใน 6 locale).

patch ตาม 'ตำแหน่ง' (ไม่ใช่ match ค่า) → ใช้ได้ทั้ง inject ใหม่ (tail=base) และแก้ทับ payload
(tail=ไทยที่ patch แล้ว) เพราะตำแหน่ง operand คงที่ (ความยาว tail ไม่เปลี่ยน)."""
import sys, struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import inkb_core as core

ROOT = Path(__file__).resolve().parent.parent
STORY_DIR = ROOT / "UntilThenExtrallPCK" / "assets" / "story"
LOCS = ["de", "ja", "es", "fr", "it", "zh-CN"]


def _u32(b, i):
    return struct.unpack_from('<I', b, i)[0]


def _base_starts(strings):
    starts = set()
    off = 0
    for s in strings:
        starts.add(off)
        off += len(s['text'].encode('utf-8')) + 1
    return starts


_pos_cache = {}


def operand_positions(rel: str):
    """เซ็ตตำแหน่ง byte ใน binary_tail ที่เป็น operand string-offset จริง (cached)."""
    if rel in _pos_cache:
        return _pos_cache[rel]
    base = (STORY_DIR / rel).read_bytes()
    bpar = core.parse_inkb(base)
    btail = bpar['binary_tail']
    starts = _base_starts(bpar['strings'])

    prot = set()
    k = btail.find(b'\xff\xff\xff\xff')
    while k != -1:
        prot.update(range(k, k + 4))
        k = btail.find(b'\xff\xff\xff\xff', k + 1)

    pos = set()
    n_official = 0
    # ground truth: diff vs locale ทางการ
    for loc in LOCS:
        lp = STORY_DIR / "locales" / loc / rel
        if not lp.exists():
            continue
        try:
            ot = core.parse_inkb(lp.read_bytes())['binary_tail']
        except Exception:
            continue
        if len(ot) != len(btail):
            continue
        n_official += 1
        for i in range(0, len(btail) - 3):
            if i in prot:
                continue
            bv = _u32(btail, i)
            if bv in starts and _u32(ot, i) != bv:
                pos.add(i)
    # backup: prec<256 (operand ของสตริงที่ไม่ขยับใน locale ใดเลย / ไฟล์ไม่มี locale ทางการ)
    for i in range(4, len(btail) - 3):
        if any((i + j) in prot for j in range(4)):
            continue
        if _u32(btail, i - 4) < 256 and _u32(btail, i) in starts:
            pos.add(i)

    _pos_cache[rel] = (pos, n_official)
    return _pos_cache[rel]


def patch(rel: str, tail: bytes, offset_map: dict) -> bytes:
    """patch operand ตามตำแหน่ง (oracle). offset_map: cur_section_offset -> new_section_offset."""
    pos, _ = operand_positions(rel)
    result = bytearray(tail)
    for i in pos:
        cur = _u32(result, i)
        if cur in offset_map and offset_map[cur] != cur:
            struct.pack_into('<I', result, i, offset_map[cur])
    return bytes(result)


def validate_no_midchar(rel: str, out_bytes: bytes):
    """ตรวจว่า operand ทุกตัวในไฟล์ที่ build แล้ว ชี้ 'จุดเริ่มสตริง' จริง (ไม่ชี้กลางตัว UTF-8).
    คืน (ok, n_bad, n_wrongstr). n_bad = ชี้กลางตัว (=crash), n_wrongstr = ชี้ start แต่ผิดสตริง."""
    pos, _ = operand_positions(rel)
    par = core.parse_inkb(out_bytes)
    tail = par['binary_tail']
    sec = b"".join(s['text'].encode('utf-8') + b'\x00' for s in par['strings'])
    starts = _base_starts(par['strings'])
    n_bad = 0
    for i in pos:
        v = _u32(tail, i)
        if v in starts:
            continue
        if v < len(sec) and 0x80 <= sec[v] < 0xC0:
            n_bad += 1  # กลางตัว UTF-8 → crash แน่
    return (n_bad == 0, n_bad)


if __name__ == "__main__":
    # smoke test
    rel = "rdvt/1/1c/Dining.1d.inkb"
    pos, no = operand_positions(rel)
    print(f"{rel}: {len(pos)} operand positions, {no} official locales")
