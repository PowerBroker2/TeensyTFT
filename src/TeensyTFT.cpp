#include "TeensyTFT.h"

DMAMEM uint16_t TeensyTFT::fbA[TeensyTFT::PHYS_WIDTH*TeensyTFT::PHYS_HEIGHT];
DMAMEM uint16_t TeensyTFT::fbB[TeensyTFT::PHYS_WIDTH*TeensyTFT::PHYS_HEIGHT];
bool TeensyTFT::instanceExists = false;

TeensyTFT::TeensyTFT(uint8_t cs, uint8_t dc, uint8_t rst)
    : csPin(cs), dcPin(dc), rstPin(rst),
      tft(cs, dc, rst),
      front(fbA), back(fbB),
      _width(PHYS_WIDTH), _height(PHYS_HEIGHT)
{
    if (instanceExists) {
        Serial.println("ERROR: Only one TeensyTFT instance allowed!");
        while(true);
    }
    instanceExists = true;

    memset(front, 0, sizeof(fbA));
    memset(back , 0, sizeof(fbB));
}

void TeensyTFT::begin(uint32_t spiSpeed, uint8_t rotation)
{
    tft.begin(spiSpeed);
    tft.setRotation(rotation);

    // Update logical width and height based on rotation
    if (rotation & 1) { // rotations 1 or 3 -> landscape
        _width  = PHYS_WIDTH;
        _height = PHYS_HEIGHT;
    } else {           // rotations 0 or 2 -> portrait
        _width  = PHYS_HEIGHT;
        _height = PHYS_WIDTH;
    }

    tft.setFrameBuffer(front);
    tft.useFrameBuffer(true);

    tft.updateScreen();
}

void TeensyTFT::clear(uint16_t color)
{
    for(int i=0;i<PHYS_WIDTH*PHYS_HEIGHT;i++) back[i] = color;
}

void TeensyTFT::pixel(int x,int y,uint16_t color)
{
    if((x < 0) || (x >= _width) || (y < 0) || (y >= _height))
        return;
    
    back[(y * PHYS_WIDTH) + x] = color; // always use physical stride
}

void TeensyTFT::fillRegion(int x,
                           int y,
                           int w,
                           int h,
                           const uint16_t* buf)
{
    if(!buf)
        return;
    
    for(int row = 0; row < h; row++)
    {
        int destY = y + row;

        if((destY < 0) || (destY >= _height))
            continue;

        memcpy(back + (destY * PHYS_WIDTH) + x,
               buf + (row * w),
               w * sizeof(uint16_t));
    }
}

void TeensyTFT::fillRegion(int x,
                           int y,
                           int w,
                           int h,
                           const uint16_t* buf,
                           const bool*     mask)
{
    if(!buf)
        return;
    
    for(int row = 0; row < h; row++)
    {
        int destY = y + row;

        if (destY < 0)
            continue;
        
        if (destY >= _height)
            return;
        
        for(int col = 0; col < w; col++)
        {
            int destX = x + col;
            int i     = row*w + col;

            if (destX < 0)
                continue;
            
            if (destX >= _width)
                break;

            if (mask)
            {
                if (!mask[i])
                    continue;
            }

            back[destY*_width + destX] = buf[i];
        }
    }
}

void TeensyTFT::swap()
{
    while(tft.asyncUpdateActive());

    uint16_t* tmp = front;
    front = back;
    back = tmp;

    tft.setFrameBuffer(front);
    tft.updateScreenAsync();
}