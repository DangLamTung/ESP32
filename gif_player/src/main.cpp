/**
 * ST7789H2 198×240 — Animation + FPS Demo
 */
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS   10
#define TFT_DC    1
#define TFT_RST   0
#define TFT_MOSI  4
#define TFT_SCLK  2

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

unsigned long fc = 0, lt = 0, fps = 0;
void tck() { fc++; if (millis()-lt>=1000) { fps=fc; fc=0; lt=millis(); } }
void show(const char* s) { tft.setTextColor(0xFFFF); tft.setCursor(2,2); tft.print(s); tft.print(" "); tft.print(fps); }

// ── Bouncing ball ────────────────────────────────────────────
void ball(int sec) {
    tft.fillScreen(0x0000);
    int x=50,y=60,dx=3,dy=2,r=14;
    unsigned long t0=millis();
    while (millis()-t0<(unsigned long)sec*1000){
        tft.fillCircle(x,y,r,0x0000);
        x+=dx;y+=dy;
        if(x<=r||x>=197-r)dx=-dx;
        if(y<=r||y>=239-r)dy=-dy;
        tft.fillCircle(x,y,r,0xF800);
        show("Ball"); tck();
    }
}

// ── Pattern ──────────────────────────────────────────────────
void pattern(int sec) {
    tft.fillScreen(0x0000);
    unsigned long t0=millis(); int p=0;
    while(millis()-t0<(unsigned long)sec*1000){
        for(int y=0;y<240;y++){
            uint16_t c=((y+p)&1)?0x001F:0x0000;
            tft.drawFastHLine(0,y,198,c);
        }
        p++; show("Stripes"); tck();
    }
}

// ── Zoom rect ────────────────────────────────────────────────
void zoom(int sec) {
    tft.fillScreen(0x0000);
    unsigned long t0=millis(); int s=20, ds=1;
    while(millis()-t0<(unsigned long)sec*1000){
        tft.fillScreen(0x0000);
        int x=(198-s)/2, y=(240-s)/2;
        tft.fillRect(x,y,s,s,0x07E0);
        s+=ds; if(s>=120||s<=10)ds=-ds;
        show("Zoom"); tck();
    }
}

void setup() {
    pinMode(8, OUTPUT);
    tft.init(198, 240, SPI_MODE3);
    tft.setRotation(0);
    tft.fillScreen(0x0000);
    tft.setTextSize(1);

    ball(5);
    pattern(5);
    zoom(5);

    tft.fillScreen(0x0000);
    tft.setTextColor(0x07E0); tft.setTextSize(2);
    tft.setCursor(20, 100); tft.print("Done! FPS:");
    tft.setCursor(60, 140); tft.print(fps);
}

void loop() { digitalWrite(8, !digitalRead(8)); delay(2000); }



