from art.palette import PALETTE, PALETTE_ORDER
from art.sprites import SPRITES, ellipse, rect

REQUIRED = ["body", "cursor", "arm", "laptop", "poof_s", "poof_m", "poof_ring", "poof_dots"]


def test_all_required_sprites_exist():
    for name in REQUIRED:
        assert name in SPRITES, name


def test_sprites_are_rectangular_and_use_palette_chars():
    for name, rows in SPRITES.items():
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
    ring = ellipse(23, 11, "P", hole=(13, 5))
    assert ring[5][11] == "."  # hollow centre
    assert ring[0][11] == "P"  # solid rim
