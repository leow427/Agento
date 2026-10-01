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


def cloud(w, h, blobs):
    """Union of elliptical blobs (x, y, bw, bh) drawn as a lumpy cloud with a grey underside."""
    grid = [["."] * w for _ in range(h)]
    for bx, by, bw, bh in blobs:
        for y in range(bh):
            for x in range(bw):
                if _inside(x, y, bw, bh) and 0 <= bx + x < w and 0 <= by + y < h:
                    grid[by + y][bx + x] = "P"
    for y in range(h):
        for x in range(w):
            if grid[y][x] == "P" and (y == h - 1 or grid[y + 1][x] == "."):
                grid[y][x] = "L"
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

def _keyrow(offset):
    """One row of 2-wide keycaps separated by single dark gaps; `offset` staggers the row."""
    return "".join("L" if 1 <= x <= 17 and (x - 1 + offset) % 3 != 2 else "S" for x in range(19))


# 19x6 keyboard: two rows of keycaps with dark gap rows between them, a spacebar, a dark base.
KEYBOARD = [_keyrow(0), "S" * 19, _keyrow(1), "S" * 19, "SS" + "L" * 15 + "SS", "." + "S" * 17 + "."]

# Tiny arm stub: two clay cells and a darker hand.
ARM_TINY = ["OO", "OO", "DD"]

SMOKE = {
    "poof_s": cloud(7, 5, [(0, 2, 4, 3), (2, 0, 4, 4), (4, 1, 3, 3), (1, 2, 5, 3)]),
    "poof_m": cloud(15, 9, [(0, 3, 6, 5), (4, 0, 7, 6), (8, 2, 7, 6), (3, 4, 9, 5)]),
    "poof_big": cloud(21, 11, [(0, 3, 7, 5), (3, 1, 8, 7), (8, 0, 8, 7), (14, 3, 7, 5), (4, 5, 13, 6)]),
    "poof_puffs": cloud(
        23, 13,
        [(0, 4, 5, 4), (18, 4, 5, 4), (3, 9, 4, 3), (16, 9, 4, 3), (9, 10, 5, 3)],
    ),
}

# "!" floating above his head while a session needs you.
BANG = ["C", "C", ".", "C"]

# Wi-Fi symbol (light grey) for the disconnected screen, crossed by a clay slash drawn on top.
WIFI = [
    ".....LLLLLLL.....",
    "...LL.......LL...",
    ".LL...........LL.",
    "L.....LLLLL.....L",
    "....LL.....LL....",
    "...L.........L...",
    ".......LLL.......",
    "......L...L......",
    ".................",
    "........L........",
    ".......LLL.......",
    "........L........",
]
SLASH = [
    "OO..........",
    ".OO.........",
    "..OO........",
    "...OO.......",
    "....OO......",
    ".....OO.....",
    "......OO....",
    ".......OO...",
    "........OO..",
    ".........OO.",
    "..........OO",
    "...........O",
]

SPRITES = dict(
    {
        "bang": BANG,
        "wifi": WIFI,
        "slash": SLASH,
        "body": BODY,
        "cursor": ["KKK"],
        "arm_tiny": ARM_TINY,
        "keyboard": KEYBOARD,
    },
    **SMOKE
)


def body_3q():
    """The body turned 45 degrees to his right: front face (left), shaded side (right), lit top."""
    w, h = 14, 14
    g = [["."] * w for _ in range(h)]
    for x in range(2, 12):
        g[1][x] = "E"
    for x in range(4, 14):
        g[0][x] = "E"
    for y in range(2, 12):
        for x in range(0, 10):
            g[y][x] = "O"
    for x in (10, 11):
        for y in range(2, 11):
            g[y][x] = "D"
    for x in (12, 13):
        for y in range(1, 10):
            g[y][x] = "D"
    for y in (12, 13):
        for x in (1, 2, 6, 7):
            g[y][x] = "O"
    for x, y in [(1, 4), (2, 5), (3, 6), (2, 7), (1, 8)]:
        g[y][x] = "K"
    return ["".join(r) for r in g]


def laptop_3q():
    """The same laptop turned with him: lid back (left), thin light edge (right and top), base."""
    w, h = 14, 10
    g = [["."] * w for _ in range(h)]
    for x in range(1, 14):
        g[0][x] = "L"
    for y in range(1, 8):
        for x in range(0, 12):
            g[y][x] = "S"
    for x, y in [(6, 3), (5, 4), (6, 4), (7, 4), (6, 5)]:
        g[y][x] = "C"
    for x in (12, 13):
        for y in range(1, 8):
            g[y][x] = "L"
    for y in (8, 9):
        for x in range(0, 14):
            g[y][x] = "L"
    g[9][0] = g[9][13] = "."
    return ["".join(r) for r in g]


SPRITES_3Q = dict(
    {
        "body": body_3q(),
        "cursor": ["KKK"],
        "arm": ["OO", "OO", "OO", "DD"],
        "arm_side": ["DD", "DD", "DD", "DD"],
        "laptop": laptop_3q(),
    },
    **SMOKE
)
