from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
WINDOWS = ASSETS / "windows"
MACOS = ASSETS / "macos"


def rounded_rect_mask(size: int, radius: int) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
    return mask


def gradient_background(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    c1 = (15, 23, 42)
    c2 = (18, 56, 95)
    c3 = (13, 148, 136)
    for y in range(size):
        for x in range(size):
            t = (x + y) / (2 * (size - 1))
            if t < 0.52:
                u = t / 0.52
                c = tuple(int(c1[i] * (1 - u) + c2[i] * u) for i in range(3))
            else:
                u = (t - 0.52) / 0.48
                c = tuple(int(c2[i] * (1 - u) + c3[i] * u) for i in range(3))
            px[x, y] = (*c, 255)
    return img


def draw_icon(size: int) -> Image.Image:
    scale = size / 1024.0
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    bg = gradient_background(size)
    mask = rounded_rect_mask(size, int(188 * scale))
    img.alpha_composite(Image.composite(bg, Image.new("RGBA", (size, size), (0, 0, 0, 0)), mask))

    draw = ImageDraw.Draw(img)

    def xy(points):
        return [(int(x * scale), int(y * scale)) for x, y in points]

    def line(points, fill, width, joint="curve"):
        draw.line(xy(points), fill=fill, width=max(1, int(width * scale)), joint=joint)

    pale = (219, 234, 254, 222)
    cyan = (103, 232, 249, 210)
    yellow = (250, 204, 21, 255)
    white = (254, 252, 232, 255)

    line([(260, 748), (764, 748)], pale, 36)
    for x, y1, y2 in [(286, 690, 532), (424, 690, 430), (562, 690, 494), (700, 690, 342)]:
        line([(x, y1), (x, y2)], pale, 44)

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_points = xy([(256, 662), (342, 628), (386, 520), (470, 515), (548, 510), (568, 600), (642, 552), (706, 510), (710, 398), (790, 326)])
    shadow_draw.line(shadow_points, fill=(2, 6, 23, 110), width=max(1, int(70 * scale)), joint="curve")
    shadow = shadow.filter(ImageFilter.GaussianBlur(max(1, int(12 * scale))))
    img.alpha_composite(shadow)

    line_points = [(256, 662), (342, 628), (386, 520), (470, 515), (548, 510), (568, 600), (642, 552), (706, 510), (710, 398), (790, 326)]
    colors = [(34, 211, 238), (163, 230, 53), (250, 204, 21)]
    for idx in range(len(line_points) - 1):
        t = idx / max(1, len(line_points) - 2)
        if t < 0.52:
            u = t / 0.52
            c = tuple(int(colors[0][i] * (1 - u) + colors[1][i] * u) for i in range(3))
        else:
            u = (t - 0.52) / 0.48
            c = tuple(int(colors[1][i] * (1 - u) + colors[2][i] * u) for i in range(3))
        line([line_points[idx], line_points[idx + 1]], (*c, 255), 58)

    cx, cy, r = int(790 * scale), int(326 * scale), int(46 * scale)
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=yellow, outline=white, width=max(1, int(18 * scale)))

    line([(266, 286), (498, 286)], (191, 219, 254, 210), 32)
    line([(266, 354), (442, 354)], (191, 219, 254, 210), 32)
    line([(266, 422), (368, 422)], (191, 219, 254, 210), 32)
    line([(636, 218), (730, 236), (808, 304), (842, 394)], cyan, 30)
    line([(846, 466), (842, 492), (834, 518), (822, 542)], (103, 232, 249, 150), 30)

    for x, y, rr, fill in [(634, 218, 24, cyan), (846, 466, 20, (103, 232, 249, 205))]:
        rr = int(rr * scale)
        draw.ellipse((int(x * scale) - rr, int(y * scale) - rr, int(x * scale) + rr, int(y * scale) + rr), fill=fill)

    return img


def main() -> None:
    WINDOWS.mkdir(parents=True, exist_ok=True)
    MACOS.mkdir(parents=True, exist_ok=True)

    icon1024 = draw_icon(1024)
    icon1024.save(ASSETS / "app-icon.png")

    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_images = [draw_icon(size) for size in ico_sizes]
    ico_images[-1].save(WINDOWS / "app-icon.ico", sizes=[(s, s) for s in ico_sizes], append_images=ico_images[:-1])

    icns_sizes = [16, 32, 64, 128, 256, 512, 1024]
    icns_images = [draw_icon(size) for size in icns_sizes]
    icns_images[-1].save(MACOS / "app-icon.icns", format="ICNS", append_images=icns_images[:-1])


if __name__ == "__main__":
    main()
