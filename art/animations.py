BODY_X = 8
IDLE_Y = 9          # body top while idle
CODE_Y = 8          # body top while coding (bounce goes UP to 7 so the face never meets the lid)
LAPTOP_X, LAPTOP_Y = 5, 16
ARM_L_X, ARM_R_X = 6, 21
ARM_UP_Y, ARM_DOWN_Y = 10, 12

# Poof art is centred on the laptop: poof_ring/poof_dots are 23x11, poof_m 13x7, poof_s 5x3.
POOF_RING_POS = (3, 15)
POOF_M_POS = (8, 17)
POOF_S_POS = (12, 19)


def body(y, cursor):
    layers = [("body", BODY_X, y)]
    if cursor:
        layers.append(("cursor", BODY_X + 7, y + 7))
    return layers


def coding_pose(left_down, cursor, bounce):
    y = CODE_Y - (1 if bounce else 0)
    left_y = ARM_DOWN_Y if left_down else ARM_UP_Y
    right_y = ARM_UP_Y if left_down else ARM_DOWN_Y
    return (
        body(y, cursor)
        + [("arm", ARM_L_X, left_y), ("arm", ARM_R_X, right_y)]
        + [("laptop", LAPTOP_X, LAPTOP_Y)]
    )


def step(ms, layers):
    return {"ms": ms, "layers": layers}


IDLE = [
    step(400, body(IDLE_Y, True)),
    step(400, body(IDLE_Y - 1, True)),
    step(400, body(IDLE_Y, False)),
    step(400, body(IDLE_Y - 1, False)),
]

TO_CODING = [
    step(70, body(CODE_Y, True) + [("poof_s", *POOF_S_POS)]),
    step(70, body(CODE_Y, True) + [("poof_m", *POOF_M_POS)]),
    step(90, coding_pose(True, True, False) + [("poof_ring", *POOF_RING_POS)]),
    step(90, coding_pose(False, True, False) + [("poof_dots", *POOF_RING_POS)]),
]

CODING = [
    step(80, coding_pose(True, True, False)),
    step(80, coding_pose(False, True, False)),
    step(80, coding_pose(True, False, True)),
    step(80, coding_pose(False, False, True)),
]

TO_IDLE = [
    step(90, coding_pose(True, True, False) + [("poof_dots", *POOF_RING_POS)]),
    step(90, coding_pose(False, True, False) + [("poof_ring", *POOF_RING_POS)]),
    step(70, body(IDLE_Y, True) + [("poof_m", *POOF_M_POS)]),
    step(70, body(IDLE_Y, True) + [("poof_s", *POOF_S_POS)]),
]

ANIMATIONS = {
    "idle": {"steps": IDLE, "loop": True, "hold_ms": 5000, "next": "to_coding"},
    "to_coding": {"steps": TO_CODING, "loop": False, "next": "coding"},
    "coding": {"steps": CODING, "loop": True, "hold_ms": 7000, "next": "to_idle"},
    "to_idle": {"steps": TO_IDLE, "loop": False, "next": "idle"},
}
START = "idle"
