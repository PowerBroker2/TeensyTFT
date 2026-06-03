#include "TeensyTFT.h"

TeensyTFT::TeensyTFT(uint8_t cs, uint8_t dc, uint8_t rst, uint8_t tcs, uint8_t tirq) 
    : tft(cs, dc, 13, 11, 12, rst, tcs, tirq) 
{
    clear(0x0000);
}

void TeensyTFT::begin(uint32_t spi_clock)
{
    tft.begin(spi_clock);
    tft.setRotation(1); 
    tft.setVSyncSpacing(0); 

    tft.update(_frameBuffer, true); 
}

void TeensyTFT::clear(uint16_t color) {
    for (int i = 0; i < (_width * _height); i++)
    {
        _frameBuffer[i] = color;
    }
}

void TeensyTFT::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (_outOfBounds(x, y))
        return;

    _frameBuffer[x + (_width * y)] = color;
}

void TeensyTFT::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (y < 0 || y >= _height || x >= _width)
        return;
    
    if (x < 0)
    {
        w += x;
        x = 0;
    }

    if (x + w > _width)
        w = _width - x;
    
    int index = x + (_width * y);

    for (int16_t i = 0; i < w; i++)
    {
        _frameBuffer[index + i] = color;
    }
}

void TeensyTFT::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (x < 0 || x >= _width || y >= _height)
        return;
    
    if (y < 0)
    {
        h += y;
        y = 0;
    }

    if (y + h > _height)
        h = _height - y;
    
    int index = x + (_width * y);

    for (int16_t i = 0; i < h; i++)
    {
        _frameBuffer[index + (i * _width)] = color;
    }
}

void TeensyTFT::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (x >= _width || y >= _height)
        return;
    
    if (x < 0)
    {
        w += x;
        x = 0;
    }

    if (y < 0)
    {
        h += y;
        y = 0;
    }

    if (x + w > _width)
        w = _width - x;

    if (y + h > _height)
        h = _height - y;

    for (int16_t row = y; row < (y + h); row++)
    {
        int index = x + (_width * row);

        for (int16_t col = 0; col < w; col++)
        {
            _frameBuffer[index + col] = color;
        }
    }
}

void TeensyTFT::fillRegion(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels, const bool* mask)
{
    int idx = 0;

    for (int row = y; row < (y + h); row++)
    {
        if (row >= 0 && row < _height)
        {
            for (int col = x; col < (x + w); col++)
            {
                if (col >= 0 && col < _width)
                {
                    if (mask == nullptr || mask[idx] == true)
                    {
                        _frameBuffer[col + (_width * row)] = pixels[idx];
                    }
                }

                idx++;
            }
        }
    }
}

void TeensyTFT::swap()
{
    tft.update(_frameBuffer);
}

bool TeensyTFT::touched()
{
    if (tft.readTouch(_touchX, _touchY, _touchZ))
    {
        if (_touchZ > TOUCH_PRESSURE_THRESHOLD)
        {
            if (_touchIdleTimer > RESET_THRESHOLD_MS)
            {
                int16_t initialX = (_width  - 1) - map(_touchY, TOUCH_X_CAL_MIN, TOUCH_X_CAL_MAX, 0, _width);
                int16_t initialY = (_height - 1) - map(_touchX, TOUCH_Y_CAL_MIN, TOUCH_Y_CAL_MAX, 0, _height);
                
                _filteredX = initialX;
                _filteredY = initialY;
            }

            int16_t rawX = _touchY;
            int16_t rawY = _touchX;
            
            int16_t mappedX = (_width  - 1) - map(rawX, TOUCH_X_CAL_MIN, TOUCH_X_CAL_MAX, 0, _width);
            int16_t mappedY = (_height - 1) - map(rawY, TOUCH_Y_CAL_MIN, TOUCH_Y_CAL_MAX, 0, _height);
            
            // Apply Exponential Smoothing
            _filteredX = (_filteredX * (1.0f - SMOOTHING_FACTOR)) + (mappedX * SMOOTHING_FACTOR);
            _filteredY = (_filteredY * (1.0f - SMOOTHING_FACTOR)) + (mappedY * SMOOTHING_FACTOR);
            
            _touchX = constrain((int16_t)_filteredX, 0, _width - 1);
            _touchY = constrain((int16_t)_filteredY, 0, _height - 1);
            
            _touchIdleTimer = 0;

            return true;
        }
    }
    
    return false;
}