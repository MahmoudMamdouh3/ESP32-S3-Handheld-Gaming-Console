# Display & SPI Bus Contract
Purpose: the display/SPI subsystem has several non-obvious rules that,
if violated, either produce silently wrong colors or corrupt the SPI bus.
This file is the single reference so agents do not have to reverse-engineer
display_emu.cpp from scratch.

## Pixel format — THE most common source of color bugs
The ST7789 on this hardware is wired in BGR mode. The MADCTL register is
manually written at boot (0xA0 | 0x08 = BGR bit set). This means:

- **Emulator palettes** must be in **BGR565 byte-swapped** format.
  "Byte-swapped" means the two bytes of each uint16_t are reversed relative
  to what you'd get from a standard `(r << 11) | (g << 5) | b` formula.
- **The correct formula for a palette entry:**
  ```c
  uint16_t bgr565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
  uint16_t wire   = (bgr565 >> 8) | (bgr565 << 8); // byte-swap for SPI
  ```
- **Menu/UI pixels** go through a GFXcanvas16 which uses a different path
  (writeBytes) -- see uiColor() in display_emu.cpp for the correct helper.
- If colors are wrong (red/blue swapped, or blue sky turns orange):
  check whether the palette was built with BGR or RGB, and whether the
  byte swap was applied. Do NOT touch the MADCTL register to fix colors --
  fix the palette instead.

## startFrame / endFrame contract
```
DisplayEmu::startFrame();     // asserts SPI CS, calls setAddrWindow ONCE
  // Inside here: lcd_draw_line fires 144 times, each calls:
  //   DisplayEmu::streamPixelRow(rowBuffer, 240 or 480);
DisplayEmu::endFrame();       // releases CS
```
**Rules:**
- startFrame() must ALWAYS be paired with endFrame(). Missing endFrame()
  leaves the SPI bus locked -- subsequent SD card operations will hang.
- streamPixelRow() must ONLY be called between startFrame/endFrame.
  Calling it outside that context is undefined behavior on the ST7789.
- The address window is set for the GB viewport (240x216 at OFFSET_X=40,
  OFFSET_Y=12). Streaming more than 240*216*2 bytes will overrun the window.

## NES and DOOM render paths
- NES: streamNESFrame() is self-contained (startWrite/endWrite internally).
  It does NOT use startFrame/endFrame. Do not mix the two.
- DOOM: streamDoomFrame() is also self-contained. DOOM interleaves SD reads
  between frames; do not call startFrame before doomgeneric_Tick().

## Dual-Core Asynchronous Display Pipeline (Core 0 Worker)
- **Architecture:** To eliminate display transfer stalls on the emulation thread, the system runs an asynchronous display worker task `vDisplayTask` pinned to Core 0.
- **Contract:**
  - Core 1 generates the frame and calls `DisplayEmu::streamGBFrame(fb)` or `DisplayEmu::streamRawFrameAsync(fb, x, y, w, h)`.
  - Frame submission swaps between double-buffered PSRAM canvases and signals Core 0 via FreeRTOS notification.
  - Core 0 acquires `SpiArbiter`, configures ST7789 window address, and streams DMA pixel data over SPI.
  - Core 1 immediately begins emulating the next frame in parallel, yielding steady 60 FPS without frame-drop.
  - Call `DisplayEmu::waitForDisplay()` to synchronize if state tear-down or SD access requires the display task to be idle.

## SPI bus sharing & SpiArbiter rules
- The SD card and TFT share SCK (GPIO12) and MOSI (GPIO11).
- They have SEPARATE CS pins: TFT=GPIO10, SD=GPIO13.
- The SD card uses MISO (GPIO15); the TFT does not use MISO.
- **SpiArbiter Synchronization:** Because Core 0 and Core 1 run in parallel, concurrent SPI access is prevented by `SpiArbiter` (`spi_arbiter.h`), a recursive FreeRTOS mutex.
  - Every SD Card operation (`SDCard::scanRoms()`, `SaveManager::saveState()`, `BoxArt::load()`) must acquire `SpiArbiter::lock()`.
  - Every TFT blit (`DisplayEmu::pushPixelsFullScreen()`, `vDisplayTask`) must acquire `SpiArbiter::lock()`.
- The Arduino SPI library manages CS via the transaction API.
  Never manually toggle CS pins outside startWrite/endWrite or
  SPISettings transactions -- this will corrupt in-flight data.
- SD card reads are BLOCKED during an active startFrame()/endFrame() window or while `vDisplayTask` holds `SpiArbiter`.


## Adding a new display region or blitting API
Any new blit function must:
1. Use one of: pushPixelsAt(), pushPixelsFullScreen(), pushPixels(),
   or be a new self-contained startWrite/setAddrWindow/writeBytes/endWrite.
2. Never call setAddrWindow() inside startFrame/endFrame (that context
   already has an address window set -- a second setAddrWindow will corrupt).
3. State the pixel format explicitly in the function comment (BGR565 byteswap?
   RGB565? raw index?).
4. Not exceed 320x240 total pixels -- the display is exactly that size.

## Scaling reference (nearest-neighbor, verified)
| Source | Target | Method |
|---|---|---|
| GB 160x144 | 240x216 | 1.5x: 4 source pixels -> 6 output (A A B C C D); even rows doubled |
| NES 256x240 | 256x240 at x=32 | 1:1 center crop (left/right letterbox) |
| DOOM 320x200 | 320x200 at y=20 | 1:1 center (top/bottom letterbox) |
| BMO face FB 128x128 | variable | bilinear or nearest in bmo_face.cpp |
