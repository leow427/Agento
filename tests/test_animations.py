import math

import pytest

from art.animations import ANIMATIONS, ANIMATIONS_3Q, START
from art.compose import compose
from art.palette import GRID
from art.sprites import SPRITES, SPRITES_3Q

MAX_RADIUS = 13.5  # cells from centre; one-cell margin inside the round bezel


VIEWS = {"front": (ANIMATIONS, SPRITES), "3q": (ANIMATIONS_3Q, SPRITES_3Q)}
VIEW_IDS = list(VIEWS)


def all_frames(anim, view="front"):
    anims, sprites = VIEWS[view]
    return [compose(step["layers"], sprites) for step in anims[anim]["steps"]]


def visible(frame, ch):
    return sum(row.count(ch) for row in frame)


def test_compose_places_and_clips():
    frame = compose([("cursor", 0, 0)])
    assert len(frame) == GRID and all(len(r) == GRID for r in frame)
    assert frame[0][:3] == "KKK"
    with pytest.raises(ValueError):
        compose([("cursor", GRID - 1, 0)])


@pytest.mark.parametrize("view", VIEW_IDS)
def test_flow_is_closed_and_well_formed(view):
    anims = VIEWS[view][0]
    assert START in anims
    for name, anim in anims.items():
        assert anim["next"] in anims, name
        assert all(s["ms"] > 0 for s in anim["steps"]), name
        if anim["loop"]:
            assert anim["hold_ms"] > 0, name
        else:
            assert "hold_ms" not in anim, name


@pytest.mark.parametrize("view", VIEW_IDS)
def test_timings_match_spec(view):
    anims = VIEWS[view][0]
    durations = {n: [s["ms"] for s in a["steps"]] for n, a in anims.items()}
    assert durations["idle"] == [400, 400, 400, 400]
    assert durations["to_coding"] == [70, 70, 90, 90]
    assert durations["coding"] == [80, 80, 80, 80]
    assert durations["to_idle"] == [90, 90, 70, 70]
    assert anims["idle"]["hold_ms"] == 5000
    assert anims["coding"]["hold_ms"] == 7000


@pytest.mark.parametrize("view", VIEW_IDS)
def test_every_frame_stays_inside_the_round_display(view):
    for name in VIEWS[view][0]:
        for i, frame in enumerate(all_frames(name, view)):
            for y, row in enumerate(frame):
                for x, ch in enumerate(row):
                    if ch == ".":
                        continue
                    dist = math.hypot(x + 0.5 - GRID / 2, y + 0.5 - GRID / 2)
                    assert dist <= MAX_RADIUS, f"{view} {name}[{i}] pixel ({x},{y}) is {dist:.1f} from centre"


@pytest.mark.parametrize("view", VIEW_IDS)
def test_prompt_glyph_visible_in_every_coding_frame(view):
    for i, frame in enumerate(all_frames("coding", view)):
        ink = visible(frame, "K")
        assert ink >= 5, f"{view} coding[{i}] shows only {ink} ink pixels; the '>' is covered"


@pytest.mark.parametrize("view", VIEW_IDS)
def test_laptop_is_hidden_by_the_cloud_then_revealed(view):
    full = visible(all_frames("coding", view)[0], "S")
    to_coding = [visible(f, "S") for f in all_frames("to_coding", view)]
    to_idle = [visible(f, "S") for f in all_frames("to_idle", view)]
    assert to_coding[0] == to_coding[1] == 0
    assert to_coding[2] <= 0.25 * full, "cloud should hide the laptop while it appears"
    assert to_coding[3] >= 0.75 * full, "laptop should be revealed once the smoke breaks up"
    assert to_idle[0] >= 0.75 * full
    assert to_idle[1] <= 0.25 * full
    assert to_idle[2] == to_idle[3] == 0
    assert all(visible(f, "S") == 0 for f in all_frames("idle", view))
    assert all(visible(f, "S") >= 0.75 * full for f in all_frames("coding", view))


@pytest.mark.parametrize("view", VIEW_IDS)
def test_idle_bobs_and_blinks(view):
    frames = all_frames("idle", view)
    assert len({tuple(f) for f in frames}) == 4  # all four idle frames differ
    assert [visible(f, "K") for f in frames] == [8, 8, 5, 5]  # cursor on, on, off, off
    tops = [next(y for y, r in enumerate(f) if r.strip(".")) for f in frames]
    assert [t - min(tops) for t in tops] == [1, 0, 1, 0]


def test_three_quarter_view_differs_from_front_view():
    assert all_frames("idle", "3q")[0] != all_frames("idle", "front")[0]
