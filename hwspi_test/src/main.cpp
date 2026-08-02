// ST7789H2 198×240 — HW SPI Benchmark
// GPIO6→SCLK GPIO7→MOSI GPIO10→CS GPIO1→DC GPIO0→RST
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS   10
#define TFT_DC    1
#define TFT_RST   0
#define TFT_MOSI  7
#define TFT_SCLK  6

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

unsigned long fc=0, lt=0, fps=0, best=0;
void tck(){fc++;if(millis()-lt>=1000){fps=fc;if(fps>best)best=fps;fc=0;lt=millis();}}
void show(const char*s){tft.setTextColor(0xFFFF);tft.setCursor(2,2);tft.print(s);tft.print(":");tft.print(fps);}

void ball(){tft.fillScreen(0x0000);int x=50,y=60,dx=3,dy=2,r=14;unsigned long t0=millis();
while(millis()-t0<3000){tft.fillCircle(x,y,r,0x0000);x+=dx;y+=dy;if(x<=r||x>=197-r)dx=-dx;if(y<=r||y>=239-r)dy=-dy;tft.fillCircle(x,y,r,0xF800);show("Ball");tck();}}

void stripes(){tft.fillScreen(0x0000);unsigned long t0=millis();int p=0;
while(millis()-t0<3000){for(int y=0;y<240;y++)tft.drawFastHLine(0,y,198,((y+p)&1)?0x07E0:0x001F);p++;show("Bar");tck();}}

void scroll(){tft.fillScreen(0x0000);unsigned long t0=millis();int x=200;
while(millis()-t0<3000){tft.fillScreen(0x0000);tft.setTextColor(0xFFE0);tft.setTextSize(3);tft.setCursor(x,90);tft.print("FAST!");x-=5;if(x<-100)x=198;show("Text");tck();}}

void setup(){
    tft.init(198,240,SPI_MODE3);tft.setRotation(0);tft.setTextSize(1);
    ball();stripes();scroll();
    tft.fillScreen(0x0000);tft.setTextColor(0x07E0);tft.setTextSize(3);
    tft.setCursor(20,80);tft.print("PEAK:");tft.setCursor(40,130);tft.print(best);tft.print("fps");
}
void loop(){delay(5000);}



