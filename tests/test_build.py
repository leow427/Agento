import json

from art.animations import ANIMATIONS
from art.palette import GRID
from tools import build


def test_model_shape_and_dedupe():
    m = build.build_model()
    total_steps = sum(len(a["steps"]) for a in ANIMATIONS.values())
    assert len(m["frames"]) <= total_steps
    assert len(m["frames"]) >= 10  # sanity: the art really produced distinct frames
    for rows in m["frames"]:
        assert len(rows) == GRID and all(len(r) == GRID for r in rows)
    for name, a in m["anims"].items():
        assert [s["ms"] for s in a["steps"]] == [s["ms"] for s in ANIMATIONS[name]["steps"]]
        assert all(0 <= s["frame"] < len(m["frames"]) for s in a["steps"])


def test_js_export_is_valid_json_with_palette(tmp_path):
    m = build.build_model()
    out = tmp_path / "data.js"
    build.write_js(m, out)
    text = out.read_text()
    assert text.startswith("window.DATA = ")
    data = json.loads(text[len("window.DATA = "):].rstrip(";\n"))
    assert data["palette"]["O"] == "#d97757"
    assert data["palette"]["."] is None
    assert data["start"] == "idle"


def test_header_export(tmp_path):
    m = build.build_model()
    out = tmp_path / "frames_data.h"
    build.write_header(m, out)
    h = out.read_text()
    assert "GENERATED" in h
    assert f"constexpr int FRAME_COUNT = {len(m['frames'])};" in h
    for name in ("ANIM_IDLE", "ANIM_TO_CODING", "ANIM_CODING", "ANIM_TO_IDLE", "ANIM_COUNT"):
        assert name in h
    assert "constexpr uint8_t ANIM_START = ANIM_IDLE;" in h
    assert "PALETTE565[9]" in h
    assert "0xDBAA" in h  # clay orange #D97757 as RGB565
    assert h.count("{") == h.count("}")
