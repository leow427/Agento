from .palette import GRID
from .sprites import SPRITES


def compose(layers):
    """Draw (sprite_name, x, y) layers first-to-last onto a GRID x GRID canvas of palette chars."""
    grid = [["."] * GRID for _ in range(GRID)]
    for name, x, y in layers:
        for dy, row in enumerate(SPRITES[name]):
            for dx, ch in enumerate(row):
                if ch == ".":
                    continue
                gx, gy = x + dx, y + dy
                if not (0 <= gx < GRID and 0 <= gy < GRID):
                    raise ValueError(f"{name} at ({x},{y}) leaves the {GRID}x{GRID} grid")
                grid[gy][gx] = ch
    return ["".join(r) for r in grid]
