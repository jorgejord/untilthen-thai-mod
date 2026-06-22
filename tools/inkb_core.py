#!/usr/bin/env python3
"""
inkb_core.py
------------
Core .inkb logic ที่ reuse จากสคริปต์เดิม (translate_inkb_final2 - Copy.py)
เพื่อให้ extract_inkb.py และ inject_inkb.py ใช้ฟังก์ชันแยกบทพูด/rebuild ตัวเดียวกันเป๊ะ
(ดึงด้วย importlib เพื่อไม่ต้องคัดลอกโค้ด — ลดความเสี่ยงพิมพ์ผิด)

ฟังก์ชันที่ export:
  parse_inkb, build_string_section, patch_binary_offsets
  is_dialogue, get_dialogue_content, rebuild_dialogue
  _is_split_opener, _has_real_text
  post_process, MAIN_CHAR_GENDER
"""

import importlib.util
from pathlib import Path

_ORIG = Path(__file__).resolve().parent.parent / "translate_inkb_final2 - Copy.py"

if not _ORIG.exists():
    raise FileNotFoundError(f"ไม่พบสคริปต์ต้นฉบับ: {_ORIG}")

_spec = importlib.util.spec_from_file_location("_inkb_orig", _ORIG)
_mod  = importlib.util.module_from_spec(_spec)
# การ import จะรันเฉพาะ top-level defs (main() ถูกกันด้วย __main__ guard จึงไม่รัน)
_spec.loader.exec_module(_mod)

# ---- re-export ----
parse_inkb            = _mod.parse_inkb
build_string_section  = _mod.build_string_section
patch_binary_offsets  = _mod.patch_binary_offsets

is_dialogue           = _mod.is_dialogue
get_dialogue_content  = _mod.get_dialogue_content
rebuild_dialogue      = _mod.rebuild_dialogue
_is_split_opener      = _mod._is_split_opener
_has_real_text        = _mod._has_real_text

post_process          = _mod.post_process
MAIN_CHAR_GENDER      = _mod.MAIN_CHAR_GENDER

STRING_SECTION_START  = _mod.STRING_SECTION_START


def detect_dialogue_indices(strings: list) -> tuple:
    """
    หา index ของ string ที่เป็นบทพูด + flag continuation
    ใช้ logic เดียวกับ translate_inkb_file() ในสคริปต์เดิม

    คืน (dialogue_idx: list[int], is_continuation: dict[int,bool])
    """
    dialogue_idx    = []
    is_continuation = {}
    prev_opener     = False

    for i, s in enumerate(strings):
        text = s['text']
        cont = prev_opener and not text.startswith(' ') and _has_real_text(text)
        if is_dialogue(text, prev_was_opener=prev_opener):
            dialogue_idx.append(i)
            if cont:
                is_continuation[i] = True
        prev_opener = _is_split_opener(text)

    return dialogue_idx, is_continuation


def extract_speaker(full_text: str) -> str:
    """ดึงชื่อผู้พูดจากบรรทัด (สำหรับ context การแปล)"""
    import re
    m = re.match(r'^([A-Z][A-Za-z0-9_]*)', full_text)
    return m.group(1) if m else ""
