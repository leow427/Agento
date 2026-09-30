import math

import pytest

from art.animations import ANIMATIONS, START
from art.compose import compose
from art.palette import GRID

MAX_RADIUS = 13.5  # cells from centre; one-cell margin inside the round bezel


def all_frames(anim):
    return [compose(step["layers"]) for step in ANIMATIONS[anim]["steps"]]


def test_compose_places_and_clips():
    frame = compose([("cursor", 0, 0)])
    assert len(frame) == GRID and all(len(r) == GRID for r in frame)
    assert frame[0][:3] == "KKK"
    with pytest.raises(ValueError):
        compose([("cursor", GRID - 1, 0)])


def test_flow_is_closed_and_well_formed():
    assert START in ANIMATIONS
    for name, anim in ANIMATIONS.items():
        assert anim["next"] in ANIMATIONS, name
        assert all(s["ms"] > 0 for s in anim["steps"]), name
        if anim["loop"]:
            assert anim["hold_ms"] > 0, name
        else:
            assert "hold_ms" not in anim, name


def test_timings_match_spec():
    durations = {n: [s["ms"] for s in a["steps"]] for n, a in ANIMATIONS.items()}
    assert durations["idle"] == [400, 400, 400, 400]
    assert durations["to_coding"] == [70, 70, 90, 90]
    assert durations["coding"] == [80, 80, 80, 80]
    assert durations["to_idle"] == [90, 90, 70, 70]
    assert ANIMATIONS["idle"]["hold_ms"] == 5000
    assert ANIMATIONS["coding"]["hold_ms"] == 7000


def test_every_frame_stays_inside_the_round_display():
    for name in ANIMATIONS:
        for i, frame in enumerate(all_frames(name)):
            for y, row in enumerate(frame):
                for x, ch in enumerate(row):
                    if ch == ".":
                        continue
                    dist = math.hypot(x + 0.5 - GRID / 2, y + 0.5 - GRID / 2)
                    assert dist <= MAX_RADIUS, f"{name}[{i}] pixel ({x},{y}) is {dist:.1f} from centre"


def test_prompt_glyph_visible_in_every_coding_frame():
    for i, frame in enumerate(all_frames("coding")):
        ink = sum(row.count("K") for row in frame)
        assert ink >= 5, f"coding[{i}] shows only {ink} ink pixels; the '>' is covered"


def test_laptop_appears_and_disappears_under_smoke():
    def has_laptop(frame):
        return any("S" in row for row in frame)

    to_coding = [has_laptop(f) for f in all_frames("to_coding")]
    to_idle = [has_laptop(f) for f in all_frames("to_idle")]
    assert to_coding == [False, False, True, True]
    assert to_idle == [True, True, False, False]
    assert not any(has_laptop(f) for f in all_frames("idle"))
    assert all(has_laptop(f) for f in all_frames("coding"))


def test_idle_bobs_and_blinks():
    frames = all_frames("idle")
    assert len({tuple(f) for f in frames}) == 4  # all four idle frames differ
    cursor_ink = [sum(r.count("K") for r in f) for f in frames]
    assert cursor_ink == [8, 8, 5, 5]  # cursor on, on, off, off
    body_top = [next(y for y, r in enumerate(f) if "O" in r) for f in frames]
    assert body_top == [9, 8, 9, 8]
