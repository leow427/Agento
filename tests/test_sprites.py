from art.palette import PALETTE, PALETTE_ORDER
from art.sprites import SPRITES, SPRITES_3Q, ellipse, rect

FRONT = ["body", "cursor", "arm", "laptop", "poof_s", "poof_m", "poof_big", "poof_puffs"]
THREEQ = ["body", "cursor", "arm", "arm_side", "laptop", "poof_s", "poof_m", "poof_big", "poof_puffs"]
FILLED = "PL"  # cloud body and its grey underside


def components(rows, chars):
    seen, count = set(), 0
    for y0, row in enumerate(rows):
        for x0, ch in enumerate(row):
            if ch not in chars or (x0, y0) in seen:
                continue
            count += 1
            stack = [(x0, y0)]
            while stack:
                x, y = stack.pop()
                if (x, y) in seen or not (0 <= y < len(rows) and 0 <= x < len(rows[0])):
                    continue
                if rows[y][x] not in chars:
                    continue
                seen.add((x, y))
                stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return count


def enclosed_holes(rows):
    """Empty cells that cannot be reached from outside the sprite (i.e. a hollow ring)."""
    h, w = len(rows), len(rows[0])
    seen, stack = set(), []
    for x in range(w):
        stack += [(x, 0), (x, h - 1)]
    for y in range(h):
        stack += [(0, y), (w - 1, y)]
    while stack:
        x, y = stack.pop()
        if (x, y) in seen or not (0 <= x < w and 0 <= y < h) or rows[y][x] in FILLED:
            continue
        seen.add((x, y))
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return sum(1 for y in range(h) for x in range(w) if rows[y][x] not in FILLED and (x, y) not in seen)


def test_all_required_sprites_exist():
    for name in FRONT:
        assert name in SPRITES, name
    for name in THREEQ:
        assert name in SPRITES_3Q, name


def test_sprites_are_rectangular_and_use_palette_chars():
    for table in (SPRITES, SPRITES_3Q):
        for name, rows in table.items():
            assert rows, name
            widths = {len(r) for r in rows}
            assert len(widths) == 1, f"{name} has ragged rows: {widths}"
            for row in rows:
                for ch in row:
                    assert ch in PALETTE, f"{name} uses unknown char {ch!r}"


def test_palette_order_covers_palette():
    assert sorted(PALETTE_ORDER) == sorted(PALETTE)
    assert PALETTE_ORDER[0] == "."


def test_body_is_13_by_12_and_has_the_prompt_glyph():
    body = SPRITES["body"]
    assert (len(body[0]), len(body)) == (13, 12)
    glyph = [(x, y) for y, row in enumerate(body) for x, ch in enumerate(row) if ch == "K"]
    assert glyph == [(2, 3), (3, 4), (4, 5), (3, 6), (2, 7)]


def test_cursor_is_three_ink_pixels():
    assert SPRITES["cursor"] == ["KKK"]


def test_helpers():
    assert rect(3, 2, "O") == ["OOO", "OOO"]
    assert ellipse(5, 3, "P") == [".PPP.", "PPPPP", ".PPP."]


def test_laptop_is_tall_enough_to_read_as_a_laptop():
    for table in (SPRITES, SPRITES_3Q):
        laptop = table["laptop"]
        assert len(laptop) >= 10, len(laptop)
        assert len(laptop[0]) >= 12


def test_big_cloud_is_solid_and_lumpy_not_a_ring():
    cloud = SPRITES["poof_big"]
    assert cloud[len(cloud) // 2][len(cloud[0]) // 2] in FILLED
    assert enclosed_holes(cloud) == 0
    tops = [next(y for y in range(len(cloud)) if cloud[y][x] in FILLED) for x in range(len(cloud[0])) if any(r[x] in FILLED for r in cloud)]
    assert max(tops) - min(tops) >= 3, "outline is too flat to look like a cloud"


def test_clouds_have_a_grey_underside():
    for name in ("poof_s", "poof_m", "poof_big"):
        assert any("L" in row for row in SPRITES[name]), name


def test_dispersing_smoke_is_several_separate_puffs():
    assert components(SPRITES["poof_puffs"], FILLED) >= 4


def test_three_quarter_body_shows_front_side_and_top():
    body = SPRITES_3Q["body"]
    flat = "".join(body)
    assert "O" in flat and "D" in flat and "E" in flat
    assert sum(row.count("K") for row in body) == 5  # the ">" survives the turn
    assert len(body[0]) > len(SPRITES["body"][0])  # the side face adds width
