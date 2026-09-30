#pragma once
#include <stdint.h>
#include <string>

// One animation frame as a crisp pixel-art SVG (29x29 viewBox), in a slot's colours.
std::string buddySvg(uint16_t frame, uint8_t slot);
