#pragma once
#include <stdint.h>
#define TFT_CS 1
#define TFT_DC 2
#define TFT_RST 3
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
#define ST77XX_BLACK 0
class Adafruit_ST7789 { public: Adafruit_ST7789(SPIClass*, int, int, int); void init(int, int); void setRotation(int); void fillScreen(uint16_t); void startWrite(); void setAddrWindow(int,int,int,int); void endWrite(); void setTextSize(int); void setTextColor(uint16_t); void setCursor(int,int); void println(const char*); void print(const char*); void drawFastHLine(int,int,int,uint16_t); void fillRect(int,int,int,int,uint16_t); void drawRGBBitmap(int,int,const uint16_t*,int,int); void drawRect(int,int,int,int,uint16_t); };
