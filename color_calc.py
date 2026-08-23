def to_bgr565_swapped(r, g, b):
    # R: 5 bits, G: 6 bits, B: 5 bits
    r = r >> 3
    g = g >> 2
    b = b >> 3
    bgr = (b << 11) | (g << 5) | r
    return ((bgr >> 8) | (bgr << 8)) & 0xFFFF

# White: 255, 255, 255
# Black: 0, 0, 0
print(f"White: {hex(to_bgr565_swapped(255, 255, 255))}")
print(f"Black: {hex(to_bgr565_swapped(0, 0, 0))}")

# Light Blue: 96, 176, 240
print(f"Light Blue: {hex(to_bgr565_swapped(96, 176, 240))}")
# Dark Blue: 0, 0, 128
print(f"Dark Blue: {hex(to_bgr565_swapped(0, 0, 128))}")

# Red: 248, 0, 0
print(f"Red: {hex(to_bgr565_swapped(248, 0, 0))}")
# Dark Red: 128, 0, 0
print(f"Dark Red: {hex(to_bgr565_swapped(128, 0, 0))}")

# Green: 0, 224, 0
print(f"Green: {hex(to_bgr565_swapped(0, 224, 0))}")
# Dark Green: 0, 96, 0
print(f"Dark Green: {hex(to_bgr565_swapped(0, 96, 0))}")

print('--- CLASSIC ---')
# Classic Colors:
# 0xE19D was byte-swapped RGB565 for 0x9DE1.
# 0x9DE1 in RGB565: R=19, G=47, B=1. (R=152, G=188, B=8)
print(f"Classic 0: {hex(to_bgr565_swapped(152, 188, 8))}")
# 0x8D61: R=17, G=43, B=1. (R=136, G=172, B=8)
print(f"Classic 1: {hex(to_bgr565_swapped(136, 172, 8))}")
# 0x3306: R=6, G=24, B=6. (R=48, G=96, B=48)
print(f"Classic 2: {hex(to_bgr565_swapped(48, 96, 48))}")
# 0x09C1: R=1, G=14, B=1. (R=8, G=56, B=8)
print(f"Classic 3: {hex(to_bgr565_swapped(8, 56, 8))}")

