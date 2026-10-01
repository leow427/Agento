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
def test_desk_is_hidden_by_the_cloud_then_revealed(view):
    full = visible(all_frames("coding", view)[0], "S")
    to_coding = [visible(f, "S") for f in all_frames("to_coding", view)]
    to_idle = [visible(f, "S") for f in all_frames("to_idle", view)]
    assert to_coding[0] == to_coding[1] == 0
    assert to_coding[2] <= 0.25 * full, "cloud should hide the laptop/keyboard while it appears"
    assert to_coding[3] >= 0.70 * full, "it should be revealed once the smoke breaks up"
    assert to_idle[0] >= 0.70 * full
    assert to_idle[1] <= 0.25 * full
    assert to_idle[2] == to_idle[3] == 0
    assert all(visible(f, "S") == 0 for f in all_frames("idle", view))
    assert all(visible(f, "S") >= 0.75 * full for f in all_frames("coding", view))


def layer_names(anim, step=0, view="front"):
    return [layer[0] for layer in VIEWS[view][0][anim]["steps"][step]["layers"]]


def test_front_view_types_on_a_keyboard_not_a_laptop():
    for anim in ("to_coding", "coding", "to_idle"):
        names = {n for i in range(4) for n in layer_names(anim, i)}
        assert "laptop" not in names, anim
    assert "keyboard" in layer_names("coding")
    assert "laptop" in layer_names("coding", 0, "3q")  # the 3/4 exploration keeps its laptop


def test_keys_never_change_colour_while_typing():
    frames = all_frames("coding")
    below_the_hands = [tuple(f[19:24]) for f in frames]
    assert len(set(below_the_hands)) == 1, "keyboard rows below the hands must not flicker"


def test_tiny_arms_peck_one_cell_and_alternate_instead_of_flailing():
    steps = ANIMATIONS["coding"]["steps"]
    arm_ys = [[l[2] for l in s["layers"] if l[0] == "arm_tiny"] for s in steps]
    body_ys = {l[2] for s in steps for l in s["layers"] if l[0] == "body"}
    assert len(body_ys) == 1, "the body stays still while he types"
    flat = [y for pair in arm_ys for y in pair]
    assert max(flat) - min(flat) == 1, "each peck moves one cell"
    downs = [sum(1 for y in pair if y == max(flat)) for pair in arm_ys]
    assert downs == [1, 0, 1, 0], "one hand at a time, with a rest between pecks"
    left_down = [pair[0] == max(flat) for pair in arm_ys]
    right_down = [pair[1] == max(flat) for pair in arm_ys]
    assert left_down == [True, False, False, False] and right_down == [False, False, True, False]


@pytest.mark.parametrize("view", VIEW_IDS)
def test_idle_bobs_and_blinks(view):
    frames = all_frames("idle", view)
    assert len({tuple(f) for f in frames}) == 4  # all four idle frames differ
    assert [visible(f, "K") for f in frames] == [8, 8, 5, 5]  # cursor on, on, off, off
    tops = [next(y for y, r in enumerate(f) if r.strip(".")) for f in frames]
    assert [t - min(tops) for t in tops] == [1, 0, 1, 0]


def test_three_quarter_view_differs_from_front_view():
    assert all_frames("idle", "3q")[0] != all_frames("idle", "front")[0]
