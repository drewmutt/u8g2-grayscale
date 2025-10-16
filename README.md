[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/olikraus/u8g2) 

![https://raw.githubusercontent.com/wiki/olikraus/u8g2/ref/u8g2_logo_transparent_orange.png](https://raw.githubusercontent.com/wiki/olikraus/u8g2/ref/u8g2_logo_transparent_orange.png) 


U8g2: Library for monochrome displays, version 2

Note: This fork adds helpers for SH1122 true 4-bit grayscale.

Natural API for grayscale with standard drawing (no extra buffers needed):
- On SH1122, calling u8g2.setDrawColor(n) with n in 0..15 selects a 4-bit gray level for subsequent drawing (0 = black, 15 = full white). XOR is not used on SH1122 in this mode.
- On non‑SH1122 displays, standard semantics apply (0 = clear, 1 = set, 2 = XOR).
- You can switch levels multiple times before a single u8g2.sendBuffer(). The library accumulates your drawings into an internal 4bpp framebuffer and transmits true grayscale on sendBuffer().
- Example:
  u8g2.clearBuffer();
  for (int i=3; i<16; i++) {
    u8g2.setDrawColor(i);
    u8g2.drawBox(0, i*5, 4, 4);
  }
  u8g2.sendBuffer();

Notes:
- This path allocates an internal 8192-byte (256x64 @ 4bpp) temporary buffer while building the frame.
- For advanced control or custom composition, you can still use the rotation-aware 4bpp helpers below (CompositePageTo4bpp + Send4bppBuffer).

- u8g2_sh1122_Send4bppBuffer(u8g2_t* u8g2, const uint8_t* buf)
  - buf is a 8192-byte frame (256x64 at 4bpp), two pixels per byte.
  - High nibble = even x, low nibble = odd x.
  - Each row is 128 bytes; rows 0..63.
- NEW (rotation-aware, integrated with u8g2 drawing):
  - u8g2_sh1122_CompositePageTo4bpp(u8g2_t*, uint8_t* gray, uint8_t fg_level, uint8_t mode)
    - Composite the CURRENT u8g2 page buffer into a 4bpp framebuffer at the given gray level.
    - mode: 0=overwrite dest nibble with fg_level; 1=max blend (dest=max(dest, fg_level)).
  - u8g2_sh1122_GrayClear(uint8_t* gray, uint8_t level)

Integrated grayscale example (C++/Arduino-style, respects rotation):

// Create any of the SH1122 setups, e.g. 4-wire HW SPI full buffer
U8G2_SH1122_256X64_F_4W_HW_SPI u8g2(U8G2_R0, PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST);

uint8_t gray[256*64/2]; // 8192 bytes

void loopFrame() {
  // Build a 4bpp frame using standard u8g2 drawing APIs
  u8g2.sh1122_GrayClear(gray, 0); // background level

  u8g2.firstPage();
  do {
    // Draw bright layer
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 24, "Hello");
    u8g2.drawBox(0, 32, 60, 12);
    u8g2.sh1122_CompositePageTo4bpp(gray, 15 /*fg level*/, 0 /*overwrite*/);

    // Draw mid-gray layer on top
    u8g2.clearBuffer();
    u8g2.drawStr(64, 24, "World");
    u8g2.drawCircle(100, 48, 15, U8G2_DRAW_ALL);
    u8g2.sh1122_CompositePageTo4bpp(gray, 8 /*fg level*/, 1 /*max blend*/);

    // Add a dark layer
    u8g2.clearBuffer();
    u8g2.drawFrame(10, 10, 100, 40);
    u8g2.sh1122_CompositePageTo4bpp(gray, 3 /*fg level*/, 1 /*max blend*/);
  } while (u8g2.nextPage());

  // Send the final 4bpp frame to the display
  u8g2.sh1122_Send4bppBuffer(gray);
}

C usage is identical with the C functions (call u8g2_sh1122_GrayClear, then loop over FirstPage/NextPage with u8g2 drawing and u8g2_sh1122_CompositePageTo4bpp, finish with u8g2_sh1122_Send4bppBuffer). Rotation is respected automatically by u8g2’s rotation callbacks.

U8g2 is a monochrome graphics library for embedded devices. 
U8g2 supports monochrome OLEDs and LCDs, which include the following controllers:
SSD1305, SSD1306, SSD1309, SSD1312, SSD1316, SSD1318, SSD1320, SSD1322, 
SSD1325, SSD1327, SSD1329, SSD1606, SSD1607, SH1106, SH1107, SH1108, SH1122, 
T6963, RA8835, LC7981, PCD8544, PCF8812, HX1230, UC1601, UC1604, UC1608, UC1610, 
UC1611, UC1617, UC1638, UC1701, ST7511, ST7528, ST7565, ST7567, ST7571, ST7586, 
ST7588, ST75160, ST75256, ST75320, NT7534, ST7920, IST3020, IST3088, IST7920, 
LD7032, KS0108, KS0713, HD44102, T7932, SED1520, SBN1661, IL3820, MAX7219, 
GP1287, GP1247, GU800
(see [here](https://github.com/olikraus/u8g2/wiki/u8g2setupcpp) for a full list).

The Arduino library U8g2 can be installed from the library manager of the Arduino IDE. U8g2 also includes U8x8 library:
 * U8g2
   * Includes all graphics procedures (line/box/circle draw).
   * Supports many fonts. (Almost) no restriction on the font height.
   * Requires some memory in the microcontroller to render the display.
 * U8x8
   * Text output only (character) device.
   * Only fonts allowed with fit into a 8x8 pixel grid.
   * Writes directly to the display. No buffer in the microcontroller required.


[Setup Guide and Reference Manual](https://github.com/olikraus/u8g2/wiki)


