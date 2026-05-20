#!/usr/bin/env python3
"""Generate simple cartoon cat face PNGs for smart badge emotions."""

from PIL import Image, ImageDraw
import os

SIZE = 120
OUT_DIR = os.path.join(os.path.dirname(__file__), "png")


def draw_base(draw, ear_color="#FF9933"):
    """Draw cat head + ears."""
    # Ears (triangles)
    draw.polygon([(20, 35), (35, 5), (50, 35)], fill=ear_color, outline="#CC7722", width=2)
    draw.polygon([(70, 35), (85, 5), (100, 35)], fill=ear_color, outline="#CC7722", width=2)
    # Inner ears
    draw.polygon([(28, 32), (35, 12), (42, 32)], fill="#FFB3B3")
    draw.polygon([(78, 32), (85, 12), (92, 32)], fill="#FFB3B3")
    # Head (circle)
    draw.ellipse([15, 25, 105, 110], fill=ear_color, outline="#CC7722", width=2)
    # Nose
    draw.polygon([(57, 72), (63, 72), (60, 76)], fill="#FF6699")


def draw_whiskers(draw):
    draw.line([(15, 70), (45, 68)], fill="#CC7722", width=1)
    draw.line([(15, 78), (45, 76)], fill="#CC7722", width=1)
    draw.line([(75, 68), (105, 70)], fill="#CC7722", width=1)
    draw.line([(75, 76), (105, 78)], fill="#CC7722", width=1)


def make_neutral():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_base(draw)
    # Eyes: normal circles
    draw.ellipse([38, 48, 52, 62], fill="white", outline="black", width=2)
    draw.ellipse([68, 48, 82, 62], fill="white", outline="black", width=2)
    draw.ellipse([43, 52, 49, 58], fill="black")
    draw.ellipse([73, 52, 79, 58], fill="black")
    # Mouth: small smile
    draw.arc([48, 76, 72, 92], start=0, end=180, fill="black", width=2)
    draw_whiskers(draw)
    return img


def make_happy():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_base(draw)
    # Eyes: happy curves (closed arcs)
    draw.arc([36, 48, 54, 64], start=200, end=340, fill="black", width=3)
    draw.arc([66, 48, 84, 64], start=200, end=340, fill="black", width=3)
    # Mouth: big smile
    draw.arc([42, 74, 78, 98], start=0, end=180, fill="black", width=2)
    # Blush
    draw.ellipse([28, 65, 42, 75], fill=(255, 180, 180, 150))
    draw.ellipse([78, 65, 92, 75], fill=(255, 180, 180, 150))
    draw_whiskers(draw)
    return img


def make_sad():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_base(draw)
    # Eyes: sad (droopy)
    draw.ellipse([38, 50, 52, 64], fill="white", outline="black", width=2)
    draw.ellipse([68, 50, 82, 64], fill="white", outline="black", width=2)
    draw.ellipse([43, 55, 49, 61], fill="black")
    draw.ellipse([73, 55, 79, 61], fill="black")
    # Eyebrows: sad angle
    draw.line([(36, 44), (52, 48)], fill="black", width=2)
    draw.line([(68, 48), (84, 44)], fill="black", width=2)
    # Mouth: frown
    draw.arc([48, 82, 72, 98], start=180, end=360, fill="black", width=2)
    # Tear
    draw.ellipse([52, 66, 56, 74], fill=(100, 180, 255, 200))
    draw_whiskers(draw)
    return img


def make_angry():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_base(draw, ear_color="#FF8833")
    # Eyes: angry (narrow)
    draw.ellipse([38, 50, 52, 62], fill="white", outline="black", width=2)
    draw.ellipse([68, 50, 82, 62], fill="white", outline="black", width=2)
    draw.ellipse([43, 53, 49, 59], fill="red")
    draw.ellipse([73, 53, 79, 59], fill="red")
    # Eyebrows: angry V shape
    draw.line([(34, 48), (54, 42)], fill="black", width=3)
    draw.line([(66, 42), (86, 48)], fill="black", width=3)
    # Mouth: gritting teeth
    draw.arc([46, 78, 74, 96], start=0, end=180, fill="black", width=2)
    draw.line([(52, 87), (52, 87)], fill="black", width=2)
    draw.line([(60, 87), (60, 82)], fill="black", width=1)
    draw.line([(68, 87), (68, 87)], fill="black", width=2)
    # Anger mark
    draw.line([(88, 30), (96, 38)], fill="red", width=2)
    draw.line([(96, 30), (88, 38)], fill="red", width=2)
    draw_whiskers(draw)
    return img


def make_surprise():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_base(draw)
    # Eyes: big round surprise
    draw.ellipse([34, 44, 54, 64], fill="white", outline="black", width=2)
    draw.ellipse([66, 44, 86, 64], fill="white", outline="black", width=2)
    draw.ellipse([41, 50, 49, 58], fill="black")
    draw.ellipse([73, 50, 81, 58], fill="black")
    # Highlight in eyes
    draw.ellipse([43, 51, 46, 54], fill="white")
    draw.ellipse([75, 51, 78, 54], fill="white")
    # Mouth: O shape
    draw.ellipse([52, 78, 68, 96], fill="black")
    draw.ellipse([54, 80, 66, 94], fill=(255, 100, 100))
    # Swirl marks
    draw.arc([18, 20, 30, 32], start=0, end=270, fill="#6666FF", width=2)
    draw.arc([90, 20, 102, 32], start=0, end=270, fill="#6666FF", width=2)
    draw_whiskers(draw)
    return img


def make_sleep():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw_base(draw)
    # Eyes: closed (horizontal lines)
    draw.arc([36, 50, 54, 62], start=0, end=180, fill="black", width=2)
    draw.arc([66, 50, 84, 62], start=0, end=180, fill="black", width=2)
    # Mouth: small wavy
    draw.arc([50, 80, 70, 92], start=0, end=180, fill="black", width=2)
    # Zzz
    draw.text((90, 20), "Z", fill=(100, 100, 255, 200))
    draw.text((96, 14), "z", fill=(100, 100, 255, 150))
    draw.text((100, 10), "z", fill=(100, 100, 255, 100))
    # Blush
    draw.ellipse([28, 65, 42, 75], fill=(255, 180, 180, 120))
    draw.ellipse([78, 65, 92, 75], fill=(255, 180, 180, 120))
    draw_whiskers(draw)
    return img


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    faces = {
        "cat_neutral": make_neutral,
        "cat_happy": make_happy,
        "cat_sad": make_sad,
        "cat_angry": make_angry,
        "cat_surprise": make_surprise,
        "cat_sleep": make_sleep,
    }

    for name, func in faces.items():
        img = func()
        path = os.path.join(OUT_DIR, f"{name}.png")
        img.save(path)
        print(f"Generated: {path} ({img.size[0]}x{img.size[1]})")


if __name__ == "__main__":
    main()
