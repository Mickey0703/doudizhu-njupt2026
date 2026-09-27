#!/usr/bin/env python3
"""重新生成贴图占位图 —— 彩色底 + emoji + 中文大字，一眼能看清内容"""
import json, struct, zlib, os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# 每个表情的 emoji 和底色
STICKER_THEMES = {
    "nailong": [
        ("尬笑", "😅", (255, 200, 50)),   # 金黄色
        ("破防", "💔", (220, 60, 60)),    # 红色
        ("梭哈", "🎰", (50, 180, 50)),    # 绿色
        ("得意", "😏", (255, 150, 50)),   # 橙色
        ("偷笑", "🤭", (255, 180, 200)),  # 粉色
        ("困惑", "😕", (150, 150, 220)),  # 淡紫
    ],
    "hakimi": [
        ("尬笑", "😸", (255, 200, 100)),
        ("破防", "😿", (200, 80, 200)),
        ("梭哈", "🐱", (80, 200, 200)),
        ("得意", "😼", (255, 160, 80)),
        ("偷笑", "🙀", (180, 180, 255)),
        ("困惑", "😾", (180, 180, 180)),
    ],
    "maodie": [
        ("吃惊", "😱", (255, 100, 100)),
        ("破防", "💥", (220, 50, 50)),
        ("得意", "😤", (255, 180, 60)),
        ("嘲讽", "😒", (140, 200, 80)),
        ("梭哈", "🌀", (80, 200, 200)),
        ("偷笑", "👀", (200, 150, 255)),
        ("困惑", "🤔", (200, 200, 200)),
    ],
}

def chunk(ctype, data):
    c = ctype + data
    return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

def make_sticker_png(path, label, emoji, bg_color):
    """生成 128×128 PNG：纯色底 + emoji 大字 + 中文标签"""
    path.parent.mkdir(parents=True, exist_ok=True)
    w, h = 128, 128
    r, g, b = bg_color

    # 用 Pillow 生成好看的图
    try:
        from PIL import Image, ImageDraw, ImageFont
        img = Image.new("RGB", (w, h), (r, g, b))
        d = ImageDraw.Draw(img)
        # 边框
        d.rectangle([2, 2, w - 3, h - 3], outline=(255, 255, 255, 180), width=2)
        # emoji 大字（居中偏上）
        try:
            f_emoji = ImageFont.truetype("seguiemj.ttf", 48)  # Windows emoji font
        except:
            try:
                f_emoji = ImageFont.truetype("C:/Windows/Fonts/seguiemj.ttf", 48)
            except:
                f_emoji = ImageFont.load_default()
        bb = d.textbbox((0, 0), emoji, font=f_emoji)
        tw = bb[2] - bb[0]
        d.text(((w - tw) / 2, 18), emoji, fill=(255, 255, 255), font=f_emoji)
        # 中文标签（底部居中）
        try:
            f_label = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 22)
        except:
            f_label = ImageFont.load_default()
        bb2 = d.textbbox((0, 0), label, font=f_label)
        lw = bb2[2] - bb2[0]
        d.text(((w - lw) / 2, 76), label, fill=(255, 255, 255), font=f_label)
        # 底部小提示行
        d.text((10, 108), "表情包", fill=(255, 255, 255, 140), font=f_label)
        img.save(path)
        return
    except ImportError:
        pass

    # 回退：纯 PNG 二进制 + 文件名可见
    raw = b""
    # 简单地把底色和白色条纹交错，形成可辨认的色块
    for y in range(h):
        raw += b"\x00"
        for x in range(w):
            # 画一个白色圆角矩形边框
            if x < 3 or x >= w - 3 or y < 3 or y >= h - 3:
                raw += bytes([255, 255, 255])
            # 中间画十字（粗略表示 emoji 位置）
            elif 40 < x < 88 and 20 < y < 60:
                raw += bytes([255, 255, 255])
            elif 30 < x < 98 and 70 < y < 100:
                raw += bytes([255, 255, 255])
            else:
                raw += bytes([r, g, b])
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw)) +
           chunk(b"IEND", b""))
    path.write_bytes(png)
    print(f"  [fallback] {path.name}")


def main():
    for theme, stickers in STICKER_THEMES.items():
        sticker_dir = ROOT / "themes" / theme / "stickers"
        print(f"\n{theme} ({len(stickers)} stickers):")
        for i, (label, emoji, color) in enumerate(stickers):
            fname = f"sticker_{i+1:02d}.png"
            make_sticker_png(sticker_dir / fname, label, emoji, color)
            print(f"  {fname} - {label}")

    print("\nDone! All stickers regenerated.")


if __name__ == "__main__":
    main()
