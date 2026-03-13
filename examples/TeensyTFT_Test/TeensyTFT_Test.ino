#include "TeensyTFT.h"
#include <elapsedMillis.h>

const int S = 32;
uint16_t redSquare[S*S];

float x = 50, y = 50;
float vx = 1.2, vy = 0.9;

// Single instance
TeensyTFT display(10, 9, 255);

elapsedMillis fpsTimer;
uint32_t frames = 0;

void setup() {
    Serial.begin(115200);
    // Begin with default SPI speed, landscape rotation
    display.begin(80000000, 1);

    // Fill the user buffer with red
    for(int i = 0; i < S*S; i++) redSquare[i] = ILI9341_RED;
}

void loop() {
    display.clear();

    x += vx;
    y += vy;

    // Bounce off logical screen boundaries
    if(x < 0 || x + S > display.width())  vx = -vx;
    if(y < 0 || y + S > display.height()) vy = -vy;

    // Draw the red square at the new position
    display.fillRegion(x, y, S, S, redSquare);

    display.swap();
    frames++;

    // Print FPS every second
    if(fpsTimer >= 1000) {
        Serial.print("FPS: ");
        Serial.println(frames);
        frames = 0;
        fpsTimer = 0;
    }
}