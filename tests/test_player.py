from art.animations import ANIMATIONS, START
from tools.player import Player


def make(now=0):
    return Player(ANIMATIONS, START, now)


def test_starts_in_idle_first_step():
    p = make()
    assert p.tick(0) == 0
    assert p.anim == "idle"


def test_idle_steps_through_its_frames():
    p = make()
    assert p.tick(399) == 0
    assert p.tick(400) == 1
    assert p.tick(1200) == 3
    assert p.tick(1600) == 0  # looped, still idle
    assert p.anim == "idle"


def test_full_cycle_times():
    p = make()
    p.tick(6399)
    assert p.anim == "idle"
    p.tick(6400)
    assert p.anim == "to_coding"
    p.tick(6719)
    assert p.anim == "to_coding"
    p.tick(6720)
    assert p.anim == "coding"
    p.tick(13759)
    assert p.anim == "coding"
    p.tick(13760)
    assert p.anim == "to_idle"
    p.tick(14079)
    assert p.anim == "to_idle"
    p.tick(14080)
    assert p.anim == "idle"


def test_toggle_from_idle_and_coding():
    p = make()
    p.tick(100)
    p.toggle(100)
    assert p.anim == "to_coding"
    assert p.tick(100) == 0
    p.tick(420)
    assert p.anim == "coding"
    p.toggle(500)
    assert p.anim == "to_idle"


def test_toggle_is_ignored_during_transitions():
    p = make()
    p.toggle(0)
    assert p.anim == "to_coding"
    p.toggle(50)
    assert p.anim == "to_coding"
    assert p.tick(50) == 0  # not restarted
    p.tick(320)
    p.tick(330)
    p.toggle(330)
    p.toggle(331)  # second press lands in to_idle and must be ignored
    assert p.anim == "to_idle"
    assert p.tick(331) == 0


def test_big_time_gap_catches_up_without_hanging():
    p = make()
    frame = p.tick(10_000_000)
    assert p.anim in ANIMATIONS
    assert 0 <= frame < len(ANIMATIONS[p.anim]["steps"])
