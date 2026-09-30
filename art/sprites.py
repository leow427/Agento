def rect(w, h, ch):
    return [ch * w for _ in range(h)]


def _inside(x, y, w, h):
    nx = (x + 0.5 - w / 2) / (w / 2)
    ny = (y + 0.5 - h / 2) / (h / 2)
    return nx * nx + ny * ny <= 1.0


def ellipse(w, h, ch, hole=None):
    """Filled ellipse w x h. With hole=(hw, hh), punch a centred elliptical hole."""
    rows = []
    for y in range(h):
        row = ""
        for x in range(w):
            on = _inside(x, y, w, h)
            if on and hole:
                hw, hh = hole
                ox, oy = (w - hw) / 2, (h - hh) / 2
                if _inside(x - ox, y - oy, hw, hh):
                    on = False
            row += ch if on else "."
        rows.append(row)
    return rows


def dots(w, h, points, ch):
    grid = [["."] * w for _ in range(h)]
    for x, y in points:
        grid[y][x] = ch
    return ["".join(r) for r in grid]


# 13x12 body. The ">" is part of the body; the "_" cursor is a separate sprite so it can blink.
BODY = [
    "..OOOOOOOOO..",
    ".OOOOOOOOOOO.",
    "OOOOOOOOOOOOO",
    "OOKOOOOOOOOOO",
    "OOOKOOOOOOOOO",
    "OOOOKOOOOOOOO",
    "OOOKOOOOOOOOO",
    "OOKOOOOOOOOOO",
    "OOOOOOOOOOOOO",
    "OOOOOOOOOOOOO",
    ".DDDDDDDDDDD.",
    "..OO.....OO..",
]

# 19x8 laptop seen from behind: slate lid with a cream logo dot, light-grey base.
LAPTOP = (
    ["." + "S" * 17 + "."]
    + [
        "S" * 19,
        "S" * 19,
        "S" * 9 + "C" + "S" * 9,
        "S" * 19,
        "S" * 19,
        "L" * 19,
        "." + "L" * 17 + ".",
    ]
)

SPRITES = {
    "body": BODY,
    "cursor": ["KKK"],
    "arm": ["OO", "OO", "OO", "DD"],
    "laptop": LAPTOP,
    "poof_s": ellipse(5, 3, "P"),
    "poof_m": ellipse(13, 7, "P"),
    "poof_ring": ellipse(23, 11, "P", hole=(13, 5)),
    "poof_dots": dots(23, 11, [(1, 5), (21, 5), (4, 1), (18, 1), (5, 9), (17, 9)], "P"),
}
