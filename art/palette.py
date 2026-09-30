GRID = 29
CELL = 16
ORIGIN = 2

PALETTE = {
    ".": None,                # transparent -> black AMOLED background
    "O": (0xD9, 0x77, 0x57),  # clay orange
    "D": (0xB0, 0x5A, 0x3F),  # clay shadow
    "K": (0x14, 0x13, 0x13),  # ink
    "C": (0xFA, 0xF9, 0xF5),  # cream
    "S": (0x3D, 0x3D, 0x3A),  # slate
    "L": (0xB0, 0xAE, 0xA5),  # light grey
    "P": (0xF0, 0xEE, 0xE6),  # poof
    "E": (0xE8, 0x95, 0x7A),  # clay highlight (top faces in the 3/4 view)
}
PALETTE_ORDER = ".ODKCSLPE"  # index in this string == palette index in the firmware
BACKGROUND = (0, 0, 0)
