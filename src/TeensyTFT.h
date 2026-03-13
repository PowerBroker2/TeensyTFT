#pragma once
#include <Arduino.h>
#include <ILI9341_t3n.h>

/**
 * @brief TeensyTFT class for Teensy 4.x using ILI9341_t3n.
 *
 * This class provides a high-performance, double-buffered interface to
 * ILI9341 TFT displays on Teensy 4.x boards. It uses DMA-safe memory
 * for framebuffers to allow asynchronous screen updates.
 *
 * Features include:
 * - Double-buffered drawing for flicker-free animations.
 * - Per-pixel drawing and rectangular region updates.
 * - Automatic handling of logical width/height for different rotations.
 * - Single-instance enforcement due to DMA buffer constraints.
 *
 * @note Only one instance of this class may exist at runtime.
 * @note All drawing operations affect the back buffer; call swap() to display changes.
 */
class TeensyTFT {
public:
    /// Physical display width (pixels)
    static const int PHYS_WIDTH  = 320;
    /// Physical display height (pixels)
    static const int PHYS_HEIGHT = 240;

    /**
     * @brief Construct a new TeensyTFT object.
     *
     * Initializes pins for SPI communication with the TFT. Does not
     * initialize the display; call begin() after construction.
     *
     * @param cs  SPI Chip Select pin
     * @param dc  SPI Data/Command pin
     * @param rst Optional Reset pin for the display (default = 255 / unused)
     *
     * @note Only one instance is allowed. Creating a second instance will halt execution.
     */
    TeensyTFT(uint8_t cs, uint8_t dc, uint8_t rst = 255);

    /**
     * @brief Initialize the display and DMA framebuffers.
     *
     * Configures the ILI9341 display, sets rotation, and prepares
     * DMA-safe framebuffers for double-buffered drawing.
     *
     * @param spiSpeed SPI clock speed in Hz (default 80 MHz)
     * @param rotation Screen rotation (0-3):
     *   - 0 = Portrait
     *   - 1 = Landscape
     *   - 2 = Portrait upside-down
     *   - 3 = Landscape inverted
     *
     * @note Must be called before any drawing operations.
     */
    void begin(uint32_t spiSpeed = 80000000, uint8_t rotation = 1);

    /**
     * @brief Get the logical display width after rotation.
     *
     * @return int Logical width (pixels)
     *
     * @note May differ from PHYS_WIDTH if rotation is portrait.
     */
    int width() const { return _width; }

    /**
     * @brief Get the logical display height after rotation.
     *
     * @return int Logical height (pixels)
     *
     * @note May differ from PHYS_HEIGHT if rotation is portrait.
     */
    int height() const { return _height; }

    /**
     * @brief Clear the back buffer with a single color.
     *
     * @param color 16-bit RGB565 color to fill the screen with (default = black)
     *
     * @note Changes are not visible until swap() is called.
     */
    void clear(uint16_t color = ILI9341_BLACK);

    /**
     * @brief Draw a single pixel to the back buffer.
     *
     * @param x Horizontal coordinate (0 to width()-1)
     * @param y Vertical coordinate (0 to height()-1)
     * @param color 16-bit RGB565 color
     *
     * @note Coordinates outside the logical screen bounds are ignored.
     * @note Changes are not visible until swap() is called.
     */
    void pixel(int x, int y, uint16_t color);

    /**
     * @brief Copy a rectangular region from a user buffer into the back buffer.
     *
     * Efficiently fills a rectangular area using a DMA-safe buffer.
     *
     * @param x Top-left X coordinate in logical screen space
     * @param y Top-left Y coordinate in logical screen space
     * @param w Width of the region in pixels
     * @param h Height of the region in pixels
     * @param buf Pointer to a buffer of size w*h containing RGB565 pixels
     *
     * @note Partial clipping is automatically handled if the region exceeds screen bounds.
     * @note Changes are not visible until swap() is called.
     */
    void fillRegion(int x, int y, int w, int h, const uint16_t* buf);

    /**
     * @brief Swap the back buffer to the front and start DMA screen update.
     *
     * This performs a flicker-free buffer swap. Waits for any ongoing
     * DMA transfer to complete before starting a new one.
     */
    void swap();

    /**
     * @brief Get direct access to the back buffer.
     *
     * Allows the user to draw directly to the back buffer for custom graphics.
     * Must call swap() to make changes visible on the screen.
     *
     * @return uint16_t* Pointer to back buffer (size PHYS_WIDTH*PHYS_HEIGHT)
     */
    uint16_t* buffer() { return back; }

private:
    uint8_t csPin; ///< SPI Chip Select pin
    uint8_t dcPin; ///< SPI Data/Command pin
    uint8_t rstPin; ///< Optional Reset pin

    ILI9341_t3n tft; ///< Internal ILI9341 object (required as object for DMA)

    static DMAMEM uint16_t fbA[PHYS_WIDTH*PHYS_HEIGHT]; ///< Front framebuffer (DMA-safe)
    static DMAMEM uint16_t fbB[PHYS_WIDTH*PHYS_HEIGHT]; ///< Back framebuffer (DMA-safe)

    uint16_t* front; ///< Pointer to current front buffer
    uint16_t* back;  ///< Pointer to current back buffer

    int _width;  ///< Logical screen width after rotation
    int _height; ///< Logical screen height after rotation

    static bool instanceExists; ///< Runtime flag to enforce single instance
};