ARM_UP_Y, ARM_DOWN_Y = 10, 12
LAPTOP_Y = 16

# Smoke positions (top-left of each sprite), shared by every view and centred on the laptop.
POOF_BIG_POS = (4, 16)
POOF_M_POS = (7, 17)
POOF_S_POS = (11, 19)
POOF_PUFFS_POS = (3, 14)

# A rig says where each part sits for one camera angle.
FRONT_RIG = {
    "body_x": 8,
    "idle_y": 9,       # body top while idle
    "code_y": 8,       # body top while coding (the bounce goes UP so the face never meets the lid)
    "cursor": (7, 7),  # cursor offset from the body's top-left
    "laptop_x": 6,
    "arms": [("arm", 6), ("arm", 21)],  # (sprite, x) for his right arm, then his left arm
}
THREEQ_RIG = {
    "body_x": 7,
    "idle_y": 8,
    "code_y": 7,
    "cursor": (5, 8),
    "laptop_x": 7,
    "arms": [("arm", 5), ("arm_side", 21)],
}


def _step(ms, layers):
    return {"ms": ms, "layers": layers}


def build(rig):
    def body(y, cursor):
        layers = [("body", rig["body_x"], y)]
        if cursor:
            layers.append(("cursor", rig["body_x"] + rig["cursor"][0], y + rig["cursor"][1]))
        return layers

    def coding_pose(left_down, cursor, bounce):
        y = rig["code_y"] - (1 if bounce else 0)
        (arm_a, xa), (arm_b, xb) = rig["arms"]
        return (
            body(y, cursor)
            + [(arm_a, xa, ARM_DOWN_Y if left_down else ARM_UP_Y), (arm_b, xb, ARM_UP_Y if left_down else ARM_DOWN_Y)]
            + [("laptop", rig["laptop_x"], LAPTOP_Y)]
        )

    idle_y, code_y = rig["idle_y"], rig["code_y"]
    return {
        "idle": {
            "steps": [
                _step(400, body(idle_y, True)),
                _step(400, body(idle_y - 1, True)),
                _step(400, body(idle_y, False)),
                _step(400, body(idle_y - 1, False)),
            ],
            "loop": True,
            "hold_ms": 5000,
            "next": "to_coding",
        },
        "to_coding": {
            "steps": [
                _step(70, body(code_y, True) + [("poof_s", *POOF_S_POS)]),
                _step(70, body(code_y, True) + [("poof_m", *POOF_M_POS)]),
                _step(90, coding_pose(True, True, False) + [("poof_big", *POOF_BIG_POS)]),
                _step(90, coding_pose(False, True, False) + [("poof_puffs", *POOF_PUFFS_POS)]),
            ],
            "loop": False,
            "next": "coding",
        },
        "coding": {
            "steps": [
                _step(80, coding_pose(True, True, False)),
                _step(80, coding_pose(False, True, False)),
                _step(80, coding_pose(True, False, True)),
                _step(80, coding_pose(False, False, True)),
            ],
            "loop": True,
            "hold_ms": 7000,
            "next": "to_idle",
        },
        "to_idle": {
            "steps": [
                _step(90, coding_pose(True, True, False) + [("poof_puffs", *POOF_PUFFS_POS)]),
                _step(90, coding_pose(False, True, False) + [("poof_big", *POOF_BIG_POS)]),
                _step(70, body(idle_y, True) + [("poof_m", *POOF_M_POS)]),
                _step(70, body(idle_y, True) + [("poof_s", *POOF_S_POS)]),
            ],
            "loop": False,
            "next": "idle",
        },
    }


ANIMATIONS = build(FRONT_RIG)
ANIMATIONS_3Q = build(THREEQ_RIG)
START = "idle"
