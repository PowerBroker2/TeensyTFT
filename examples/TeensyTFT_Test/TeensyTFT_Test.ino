#include "TeensyTFT.h"
#include <elapsedMillis.h>

const int S = 32;
uint16_t redSquare[S * S];
uint16_t blackSquare[S * S];

float x = 50, y = 50, prevX = 50, prevY = 50;
float vx = 1.2, vy = 0.9;

TeensyTFT display(10, 9, 255);

struct TouchPoint
{
    int16_t x, y;
    elapsedMillis age;
    bool active;
};

TouchPoint touches[20];

elapsedMillis fpsTimer;
uint32_t frames = 0;

void setup()
{
    Serial.begin(115200);
    display.begin();

    for (int i = 0; i < S * S; i++)
    {
        redSquare[i] = 0xF800;
        blackSquare[i] = 0x0000;
    }
    
    for (int i = 0; i < 20; i++)
        touches[i].active = false;
    
    display.clear();
}

void loop()
{
    display.fillRegion(prevX, prevY, S, S, blackSquare);

    for (int i = 0; i < 20; i++)
    {
        if (touches[i].active)
        {
            if (touches[i].age < 500)
            {
                display.fillRect(touches[i].x - 2, touches[i].y - 2, 5, 5, 0xFFFF);
            }
            else
            {
                display.fillRect(touches[i].x - 2, touches[i].y - 2, 5, 5, 0x0000);
                touches[i].active = false;
            }
        }
    }

    x += vx; 
    y += vy;

    if (x < 0 || x + S > display.width())
        vx = -vx;
    
    if (y < 0 || y + S > display.height())
        vy = -vy;

    prevX = x; 
    prevY = y;

    display.fillRegion(x, y, S, S, redSquare);

    if (display.touched())
    {
        if (touches[19].active)
        {
            display.fillRect(touches[19].x - 2, touches[19].y - 2, 5, 5, 0x0000);
        }

        for (int i = 19; i > 0; i--)
        {
            touches[i] = touches[i - 1];
        }
        
        touches[0] = {display.touchX(), display.touchY(), 0, true};
    }

    display.swap();
    frames++;

    if (fpsTimer >= 1000)
    {
        Serial.printf("FPS: %u\n", frames);
        frames = 0; 
        fpsTimer = 0;
    }
}