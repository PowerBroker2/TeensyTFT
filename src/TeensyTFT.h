#pragma once
#include <Arduino.h>
#include <ILI9341_T4.h>

/** Landscape RGB565 display and optional XPT2046 touch on Teensy 4.x. */
class TeensyTFT
{
public:
    static constexpr int16_t Width = 320;
    static constexpr int16_t Height = 240;
    static constexpr size_t PixelCount = size_t(Width) * Height;

    struct TouchCalibration
    {
        uint16_t left, right, top, bottom;
    };

    struct AffineCalibration
    {
        // Screen X = xx * rawX + xy * rawY + xOffset, likewise for screen Y.
        float xx, xy, xOffset;
        float yx, yy, yOffset;
    };

    struct EdgeAccuracy
    {
        float rmsError;
        float maxError;
        float maxTapSeparation;
        int16_t worstX, worstY;
    };

    // SPI uses SCK 13, MOSI 11, MISO 12. Use 255 for an unconnected optional pin.
    TeensyTFT(uint8_t cs, uint8_t dc, uint8_t rst = 255, uint8_t tcs = 8, uint8_t tirq = 2);
    ~TeensyTFT();

    TeensyTFT(const TeensyTFT&) = delete;
    TeensyTFT& operator=(const TeensyTFT&) = delete;

    // Allocates a DMA framebuffer and initializes the display. Safe to call again.
    // Returns false on allocation or driver initialization failure.
    bool begin(uint32_t spi_clock = ILI9341_T4_DEFAULT_SPICLOCK);

    void clear(uint16_t color = 0);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    // Source and optional mask are tightly packed, row-major arrays of w * h elements.
    // A true mask element copies the corresponding pixel.
    void fillRegion(int16_t x, int16_t y, int16_t w, int16_t h,
                    const uint16_t* pixels, const bool* mask = nullptr);

    // Submit the retained drawing buffer. Drawing may resume immediately.
    void swap();

    // Submit a packed full-screen pixmap without copying it into the drawing buffer.
    // The input may be reused immediately. A later swap() displays the drawing buffer.
    void present(const uint16_t* pixels);

    // Wait for outstanding display DMA before using the shared SPI bus.
    void waitForDisplay();

    int16_t width() const { return Width; }
    int16_t height() const { return Height; }

    // Raw values corresponding to the left, right, top and bottom screen edges.
    // Raw Y drives screen X and raw X drives screen Y in landscape orientation.
    // Either endpoint order is valid. Equal endpoints are rejected.
    bool setTouchCalibration(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom);
    // Returns the endpoint calibration or a centerline approximation of the affine map.
    TouchCalibration touchCalibration() const { return _touchCalibration; }
    bool setTouchCalibration(const AffineCalibration& calibration);
    bool touchCalibration(AffineCalibration& calibration) const;

    // Show nine crosshairs and wait for stylus presses. Zero timeout waits indefinitely.
    // Restores the displayed frame and keeps the current calibration on failure.
    bool calibrateTouch(uint32_t timeoutMs = 45000);

    // Measure two taps at each of eight independent edge targets, in pixels.
    // Leaves calibration unchanged. Result is populated only on success.
    bool checkTouchEdgeAccuracy(EdgeAccuracy& result, uint32_t timeoutMs = 45000);

    // Raw pressure thresholds. Release must be positive and below press.
    bool setTouchPressureThresholds(uint16_t press, uint16_t release);

    // Poll for touch. Calls between samples return the current contact state.
    // Accessors retain the last valid sample after release.
    bool touched();
    int16_t touchX() const { return _touchX; }
    int16_t touchY() const { return _touchY; }
    int16_t touchZ() const { return _touchZ; }

private:
    struct TouchPosition { int x, y; };
    bool sampleTouchTargets(const TouchPosition* targets, TouchPosition* readings,
                            size_t count, unsigned tapsPerTarget, uint32_t timeoutMs);
    void mapTouchPosition(int rawX, int rawY, float& x, float& y) const;

    ILI9341_T4::ILI9341Driver _driver;
    ILI9341_T4::DiffBuffStatic<6000> _diff1;
    ILI9341_T4::DiffBuffStatic<6000> _diff2;
    uint16_t* _dmaBuffer = nullptr;
    bool _ready = false;
    const bool _touchEnabled;

    elapsedMillis _touchSampleTimer;
    bool _touchActive = false;
    uint8_t _touchStreak = 0;
    uint8_t _touchHistoryIndex = 0;
    uint16_t _touchPressThreshold = 400;
    uint16_t _touchReleaseThreshold = 250;
    int16_t _touchHistoryX[3]{};
    int16_t _touchHistoryY[3]{};
    float _filteredX = 0;
    float _filteredY = 0;
    int16_t _touchX = 0;
    int16_t _touchY = 0;
    int16_t _touchZ = 0;
    TouchCalibration _touchCalibration{3800, 500, 3800, 200};
    AffineCalibration _affineCalibration{};
    bool _hasAffineCalibration = false;

    alignas(4) uint16_t _frameBuffer[PixelCount]{};
};
