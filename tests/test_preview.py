import re
import shutil
import subprocess
from collections import Counter
from pathlib import Path

import pytest

PAGE = Path(__file__).resolve().parent.parent / "preview" / "index.html"


def page():
    return PAGE.read_text()


def inline_scripts(html):
    return re.findall(r"<script>(.*?)</script>", html, re.S)


def test_every_element_id_is_unique():
    ids = re.findall(r'\bid="([^"]+)"', page())
    dupes = [i for i, n in Counter(ids).items() if n > 1]
    assert not dupes, f"duplicate ids: {dupes}"


def test_every_id_the_script_looks_up_exists():
    html = page()
    ids = set(re.findall(r'\bid="([^"]+)"', html))
    wanted = set(re.findall(r'getElementById\("([^"]+)"\)', "\n".join(inline_scripts(html))))
    assert wanted <= ids, f"script looks up ids that are not in the page: {wanted - ids}"


@pytest.mark.skipif(shutil.which("node") is None, reason="node not installed")
def test_inline_script_is_valid_javascript(tmp_path):
    js = tmp_path / "page.js"
    js.write_text("\n".join(inline_scripts(page())))
    result = subprocess.run(["node", "--check", str(js)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_page_offers_every_animation_and_slot_control():
    html = page()
    for hook in ('id="screen"', 'id="toggle"', 'id="anims"', 'id="slots"', 'id="name"', 'id="sheet"'):
        assert hook in html, hook
