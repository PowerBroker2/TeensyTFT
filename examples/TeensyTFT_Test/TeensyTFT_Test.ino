#include "TeensyTFT.h"
#include <EEPROM.h>
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

struct SavedCalibration
{
    TeensyTFT::TouchCalibration values;
    uint32_t checksum;
};

struct SavedAffineCalibration
{
    TeensyTFT::AffineCalibration values;
    uint32_t checksum;
};

constexpr int CalibrationAddress = 0;
constexpr int AffineCalibrationAddress = CalibrationAddress + sizeof(SavedCalibration);

uint32_t calibrationChecksum(const TeensyTFT::TouchCalibration& values)
{
    uint32_t hash = 2166136261u;
    for (uint16_t value : {values.left, values.right, values.top, values.bottom})
        hash = (hash ^ value) * 16777619u;
    return hash;
}

uint32_t affineChecksum(const TeensyTFT::AffineCalibration& values)
{
    static_assert(sizeof(values) == 6 * sizeof(float), "Affine record contains padding");
    const auto* bytes = reinterpret_cast<const unsigned char*>(&values);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(values); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

void runCalibration()
{
    Serial.println("Press each crosshair with a stylus, hold briefly, then lift.");
    TeensyTFT::AffineCalibration values;
    if (display.calibrateTouch() && display.touchCalibration(values))
    {
        EEPROM.put(AffineCalibrationAddress, SavedAffineCalibration{values, affineChecksum(values)});
        Serial.println("Touch calibration saved.");
    }
    else
    {
        Serial.println("Calibration did not finish. Continuing with the current settings.");
    }
}

void runEdgeCheck()
{
    Serial.println("At each edge crosshair, press briefly and lift twice.");
    TeensyTFT::EdgeAccuracy accuracy;
    if (display.checkTouchEdgeAccuracy(accuracy))
    {
        Serial.printf("Edge RMS error: %.1f px. Worst: %.1f px at (%d, %d).\n",
                      accuracy.rmsError, accuracy.maxError, accuracy.worstX, accuracy.worstY);
        Serial.printf("Largest separation between repeated taps: %.1f px.\n",
                      accuracy.maxTapSeparation);
    }
    else
    {
        Serial.println("Edge check did not finish.");
    }
}

void setup()
{
    Serial.begin(115200);
    if (!display.begin())
    {
        while (true)
        {
            Serial.println("Display initialization failed. Check wiring and available RAM2.");
            delay(1000);
        }
    }

    SavedAffineCalibration affine;
    EEPROM.get(AffineCalibrationAddress, affine);
    if (affine.checksum != affineChecksum(affine.values) || !display.setTouchCalibration(affine.values))
    {
        SavedCalibration saved;
        EEPROM.get(CalibrationAddress, saved);
        const auto& values = saved.values;
        if (saved.checksum != calibrationChecksum(values) ||
            !display.setTouchCalibration(values.left, values.right, values.top, values.bottom))
            runCalibration();
    }

    Serial.println("Send c to recalibrate touch or v to check edge accuracy.");

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
    const int command = Serial.read();
    if (command == 'c' || command == 'v')
    {
        if (command == 'c')
            runCalibration();
        else
            runEdgeCheck();
        frames = 0;
        fpsTimer = 0;
    }

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
    {
        x = constrain(x, 0.0f, float(display.width() - S));
        vx = -vx;
    }

    if (y < 0 || y + S > display.height())
    {
        y = constrain(y, 0.0f, float(display.height() - S));
        vy = -vy;
    }

    prevX = x;
    prevY = y;

    display.fillRegion(x, y, S, S, redSquare);

    if (display.touched())
    {
        const int16_t touchX = display.touchX();
        const int16_t touchY = display.touchY();
        if (touches[0].active && touches[0].x == touchX && touches[0].y == touchY)
        {
            touches[0].age = 0;
        }
        else
        {
            if (touches[19].active)
                display.fillRect(touches[19].x - 2, touches[19].y - 2, 5, 5, 0x0000);

            for (int i = 19; i > 0; i--)
                touches[i] = touches[i - 1];

            touches[0] = {touchX, touchY, 0, true};
        }
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
