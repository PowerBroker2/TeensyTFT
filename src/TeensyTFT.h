#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <ILI9341_T4.h>

/**
 * @class TeensyTFT
 * @brief Provides a high-performance TFT and Touchscreen interface for Teensy 4.1
 */
class TeensyTFT
{
private:
elapsedMillis _touchIdleTimer; // Tracks time since last touch
    float _filteredX = 0;
    float _filteredY = 0;
    
    static constexpr int16_t RESET_THRESHOLD_MS = 200; // Reset filter after 200ms
    static constexpr float   SMOOTHING_FACTOR = 0.2f;

    static constexpr int16_t TOUCH_CAL_MIN            = 200;
    static constexpr int16_t TOUCH_CAL_MAX            = 3800;
    static constexpr int16_t TOUCH_PRESSURE_THRESHOLD = 100;

    ILI9341_T4::ILI9341Driver tft;

    static const int16_t _width  = 320;
    static const int16_t _height = 240;

    int _touchX = 0;
    int _touchY = 0;
    int _touchZ = 0;

    uint16_t _frameBuffer[_width * _height];

    inline bool _outOfBounds(int16_t x, int16_t y)
    {
        return (x < 0 || x >= _width || y < 0 || y >= _height);
    }

public:
    /**
     * @brief Constructs a new TeensyTFT object.
     * @param cs Physical Chip Select pin for the TFT display.
     * @param dc Physical Data/Command pin for the TFT display.
     * @param rst Physical Reset pin for the TFT display (defaults to 255).
     * @param tcs Physical Chip Select pin for the XPT2046 touch controller (defaults to 8).
     * @param tirq Physical Interrupt pin for the XPT2046 touch controller (defaults to 2).
     */
    TeensyTFT(uint8_t cs, uint8_t dc, uint8_t rst = 255, uint8_t tcs = 8, uint8_t tirq = 2);

    /**
     * @brief Initializes the driver, configures SPI, and sets up display orientation.
     * @param spi_clock Target SPI bus speed in Hertz (defaults to 80MHz).
     */
    void begin(uint32_t spi_clock = 80000000);
    
    /**
     * @brief Fills the internal frame buffer with a solid color.
     * @param color 16-bit RGB565 color value (defaults to 0x0000/Black).
     */
    void clear(uint16_t color = 0x0000);
    
    /**
     * @brief Pushes the contents of the internal frame buffer to the physical display via DMA.
     */
    void swap();
    
    /**
     * @brief Draws an individual pixel at the specified coordinates.
     * @param x Horizontal screen position.
     * @param y Vertical screen position.
     * @param color 16-bit RGB565 color value.
     */
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    
    /**
     * @brief Draws a horizontal line of a specified width.
     * @param x Starting horizontal position.
     * @param y Vertical position.
     * @param w Width of the line in pixels.
     * @param color 16-bit RGB565 color value.
     */
    void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    
    /**
     * @brief Draws a vertical line of a specified height.
     * @param x Horizontal position.
     * @param y Starting vertical position.
     * @param h Height of the line in pixels.
     * @param color 16-bit RGB565 color value.
     */
    void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    
    /**
     * @brief Draws a filled rectangle at the specified coordinates and dimensions.
     * @param x Starting horizontal position.
     * @param y Starting vertical position.
     * @param w Width of the rectangle.
     * @param h Height of the rectangle.
     * @param color 16-bit RGB565 color value.
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    /**
     * @brief Copies an array of pixel data into a defined rectangular region.
     * @param x Starting horizontal position.
     * @param y Starting vertical position.
     * @param w Width of the region.
     * @param h Height of the region.
     * @param pixels Pointer to the array of 16-bit RGB565 color data.
     * @param mask Pointer to an optional transparency mask of which pixels to draw.
     */
    void fillRegion(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels, const bool* mask = nullptr);

    /**
     * @brief Returns the width of the display in pixels.
     * @return Screen width (int16_t).
     */
    int16_t width()  { return _width; }
    
    /**
     * @brief Returns the height of the display in pixels.
     * @return Screen height (int16_t).
     */
    int16_t height() { return _height; }

    /**
     * @brief Polls the touchscreen controller for a valid touch event.
     * @return True if a touch event is detected and within valid bounds.
     */
    bool touched();
    
    /**
     * @brief Polls the touchscreen controller for a valid touch event.
     * * This method performs a hardware read on the XPT2046 controller and maps the 
     * coordinates to the display dimensions. It features state-aware exponential 
     * smoothing:
     * - If the touch is new (idle for >200ms), the filter resets to the raw 
     * position to ensure an instant, jump-free response.
     * - During continuous contact (holding/dragging), it applies a weighted 
     * moving average to eliminate sensor jitter while maintaining responsiveness.
     * * @return True if a valid touch event is detected with pressure above 
     * TOUCH_PRESSURE_THRESHOLD, false otherwise.
     */
    int16_t touchX() { return (int16_t)_touchX; }
    
    /**
     * @brief Returns the Y-coordinate of the last recorded valid touch event.
     * @return Mapped Y position (0-239).
     */
    int16_t touchY() { return (int16_t)_touchY; }
    
    /**
     * @brief Returns the Z-pressure value of the last recorded valid touch event.
     * @return Pressure intensity.
     */
    int16_t touchZ() { return (int16_t)_touchZ; }
};