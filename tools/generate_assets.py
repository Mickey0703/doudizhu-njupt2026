#!/usr/bin/env python3
"""Generate placeholder theme/avatar/sticker assets for DouDiZhu meme themes."""
import json, os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
THEMES = ROOT / "themes"
AVATARS = ROOT / "avatars"

def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")

def make_png(path, size=(420,600)):
    """Create minimal valid PNG placeholder."""
    import struct, zlib
    path.parent.mkdir(parents=True, exist_ok=True)
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
    w, h = size
    raw = b""
    r,g,b = hash(path.stem)%256, (hash(path.stem)*7)%256, (hash(path.stem)*13)%256
    for _ in range(h):
        raw += b"\x00" + bytes([r,g,b]) * w
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw)) +
           chunk(b"IEND", b""))
    path.write_bytes(png)

def gen_theme(name, display, desc, bots, genders, chaos=False):
    base = THEMES / name
    write_json(base / "theme.json", {
        "name": name, "displayName": display, "description": desc,
        "robotNames": bots, "robotGender": genders,
        "cardBack": "card_back.png", "cardFaceDir": "cards/",
        "cardFaceSuffix": ".png", "cardNaming": "{point}{suit_letter}",
        "bgm": "sfx/bgm.mp3"
    })
    make_png(base / "card_back.png")
    labels = ["尬笑","破防","梭哈","得意","偷笑","困惑"] if not chaos else \
             ["吃惊","破防","得意","嘲讽","梭哈","偷笑","困惑"]
    for i, label in enumerate(labels):
        make_png(base / "stickers" / f"sticker_{i+1:02d}.png", (128,128))
    if chaos:
        write_json(base / "chaos.json", {
            "disappearChance": 0.10, "transformChance": 0.15,
            "disappearTiming": ["onDraw","onIdle"], "transformTiming": ["onPlay"]
        })

gen_theme("nailong", "奶龙", "我奶龙不是龙！", ["奶龙一号","奶龙二号"], ["Man","Man"])
gen_theme("hakimi", "哈基米", "哈基米～", ["哈基米一号","哈基米二号"], ["Woman","Woman"])
gen_theme("maodie", "耄耋", "混沌领域", ["耄耋左护法","耄耋右护法"], ["Man","Woman"], chaos=True)

AVATARS.mkdir(parents=True, exist_ok=True)
av_list = [
    {"id":"tian_suo","name":"田所浩二","file":"tian_suo.png"},
    {"id":"nai_long","name":"奶龙","file":"nai_long.png"},
    {"id":"man_bo","name":"曼波","file":"man_bo.png"},
    {"id":"dong_hai","name":"东海帝皇","file":"dong_hai_di_huang.png"},
    {"id":"donk","name":"donk","file":"donk.png"},
]
for av in av_list:
    make_png(AVATARS / av["file"], (128,128))
write_json(AVATARS / "avatar_index.json", av_list)
print("Assets generated successfully.")
