# TeensyTFT

**Get a 320 × 240 color display running on your Teensy, with optional stylus touch.**

Draw a shape or copy a pixmap into memory, then call `swap()` to put it on screen. TeensyTFT handles the SPI transfer with DMA and can send just the parts of the frame that changed.

[Quick start](#quick-start) · [Add touch](#add-touch) · [Drawing and pixmaps](#drawing-and-pixmaps) · [Troubleshooting](#troubleshooting)

## Quick start

### 1. Install the software

You'll need a **Teensy 4.0, 4.1, or MicroMod** and an **SPI ILI9341 display module**. The library uses the display in landscape orientation. An XPT2046 controller is optional for touch.

1. Install Teensy board support in the Arduino IDE. [PJRC setup guide](https://www.pjrc.com/teensy/td_download.html)
2. Download TeensyTFT and its ILI9341_T4 dependency as ZIP files. In the Arduino IDE, use **Sketch → Include Library → Add .ZIP Library…** for each one. If you have local copies, put both library folders directly in your sketchbook's `libraries` folder. [ILI9341_T4](https://github.com/vindar/ILI9341_T4) · [Arduino library installation guide](https://support.arduino.cc/hc/en-us/articles/5145457742236-Install-libraries-in-the-Arduino-IDE)

### 2. Wire the display

Disconnect USB power before wiring. Use 3.3 V logic and a common ground. Check your module's supply, reset, and backlight requirements. [PJRC module wiring reference](https://www.pjrc.com/store/display_ili9341_touch.html)

| Display label | Connect to | What it does |
| --- | --- | --- |
| `GND` | Teensy GND | Common ground |
| `VCC` | Module's specified supply | Powers the module |
| `SCK` or `SCLK` | Pin **13** | SPI clock |
| `MOSI` or `SDI` | Pin **11** | Data from the Teensy |
| `MISO` or `SDO` | Pin **12** | Data back to the Teensy |
| `CS` or `TFT_CS` | Pin **10** | Selects the display |
| `DC`, `D/C`, or `RS` | Pin **9** | Selects data or commands |
| `RST` or `RESET` | Hold high as specified by the module | Keeps the display out of reset |
| `LED` or `BL` | Connect as specified by the module | Powers or enables the backlight |

The sketch below leaves reset under the module's wiring. To drive reset from a Teensy GPIO, pass that pin as the third constructor argument. The value `255` marks an optional pin as unused by the library.

### 3. Upload your first sketch

Choose your Teensy model under **Tools → Board**, select its port, and upload. A **green rectangle on a black screen** confirms that the display is working.

```cpp
#include <TeensyTFT.h>

// Display CS, D/C, reset, touch CS, touch IRQ.
// 255 means the optional pin is unused.
TeensyTFT display(10, 9, 255, 255, 255);

void setup()
{
    Serial.begin(115200);
    if (!display.begin())
    {
        for (;;)
        {
            Serial.println("Display initialization failed. Check wiring and RAM2.");
            delay(1000);
        }
    }

    display.clear();                         // Fill the drawing buffer with black.
    display.fillRect(20, 20, 80, 40, 0x07E0); // x, y, width, height, green.
    display.swap();                          // Send the drawing buffer to the panel.
}

void loop()
{
}
```

If the rectangle doesn't appear, open **Serial Monitor** and check the [troubleshooting table](#troubleshooting).

## Add touch

Wire the XPT2046 alongside the display. Both controllers share the SPI data and clock pins, and each has its own chip-select pin.

| Touch label | Teensy pin |
| --- | --- |
| `T_CLK` | **13** |
| `T_DIN` | **11** |
| `T_DO` | **12** |
| `T_CS` | **8** |
| `T_IRQ` | **2**, or leave it unwired and pass `255` as the last constructor argument |

### Try the touch example

1. Open **File → Examples → TeensyTFT → TeensyTFT_Test**, or open the [example sketch](examples/TeensyTFT_Test/TeensyTFT_Test.ino).
2. Upload the sketch and open **Serial Monitor**. If there's no saved calibration, nine crosshairs appear one at a time. Press each intersection with a stylus, hold briefly, then lift.
3. Watch for a moving red square and white touch marks. The example saves a successful calibration to EEPROM and reloads it at startup.

| Send in Serial Monitor | What happens |
| --- | --- |
| `c` | Recalibrates the panel and saves the new fit on success. |
| `v` | Checks eight targets near the edges. Press and lift twice at each crosshair. The report shows RMS error, the worst error and where it occurred, and the largest distance between repeated taps at one target, all in pixels. |

For your own sketch, change the constructor to `TeensyTFT display(10, 9, 255, 8, 2);` with the wiring above. After a successful `begin()`, add this inside `loop()`.

```cpp
if (display.touched())
{
    Serial.printf("x=%d y=%d pressure=%d\n",
                  display.touchX(), display.touchY(), display.touchZ());
}
```

`touched()` reports whether contact is active. It can return `true` repeatedly during one press. To trigger an action once per press, compare the current result with the previous result and act when it changes from `false` to `true`.

## Drawing and pixmaps

The top-left pixel is **(0, 0)** and the bottom-right pixel is **(319, 239)**. X increases to the right and Y increases downward. The drawing helpers clip shapes to the screen.

Colors are 16-bit **RGB565** values. Try black `0x0000`, white `0xFFFF`, red `0xF800`, green `0x07E0`, or blue `0x001F`.

| To… | Call… |
| --- | --- |
| Fill the drawing buffer with one color | `clear(color)` |
| Draw pixels, lines, or rectangles | `drawPixel()`, `drawHLine()`, `drawVLine()`, `fillRect()` |
| Copy a pixmap into the drawing buffer | `fillRegion(x, y, w, h, pixels)` |
| Send the drawing buffer to the display | `swap()` |
| Send an existing full-screen pixmap | `present(pixels)` |
| Wait for display DMA to finish | `waitForDisplay()` |

**Pixmaps.** Supply packed RGB565 pixels, one row after another with no padding. An optional `bool` mask for `fillRegion()` has one entry per pixel. A `true` entry copies that pixel. `present()` takes a full 320 × 240 frame and leaves the drawing buffer intact. A later `swap()` displays the drawing buffer. You can reuse the source array after either `fillRegion()` or `present()` returns.

**Shared SPI.** Call `waitForDisplay()` before starting a transaction with another device on the bus. `swap()` may wait for the previous transfer before submitting the next frame. Keep one TeensyTFT object at sketch scope and call its methods outside interrupt handlers.

## Troubleshooting

| What you see | What to check |
| --- | --- |
| Screen stays dark | Check power, ground, and the backlight. |
| Backlight comes on, but no rectangle appears | Verify the panel is an SPI ILI9341. Check reset, CS, D/C, and all three SPI wires. Look for an initialization error in Serial Monitor. |
| Compiler can't find `ILI9341_T4.h` | Install the ILI9341_T4 dependency and reopen the sketch. |
| Drawing changes stay invisible | Call `swap()` after your drawing calls. |
| Display works, but touch does nothing | Verify the XPT2046 wiring and the touch CS argument. Pass `255` for IRQ if it is unwired. |
| Touch is offset, especially near the edges | In the example, run `c` to recalibrate and `v` to check edge accuracy. |
| `begin()` fails in a larger sketch | Leave 150 KiB of contiguous RAM2 free for the DMA buffer. The display object also contains a 150 KiB drawing buffer and two differential buffers. |

## Advanced settings

- **SPI speed.** `begin()` defaults to a 30 MHz write clock. Pass a frequency in hertz if you've verified that speed with your module and wiring.
- **Touch pressure.** `setTouchPressureThresholds(press, release)` adjusts the raw pressure thresholds. The defaults are 400 to press and 250 to release. The library samples at most once every 5 ms and filters brief pressure dips and coordinate spikes.
- **Calibration in your sketch.** Call `calibrateTouch()` after `begin()`. The nine-point affine fit corrects scale, offset, rotation, and skew. The outer targets sit six pixels from the display edges. Call `checkTouchEdgeAccuracy(result)` to measure eight independent targets three pixels from the edges. Both routines default to a 45-second timeout, accept a timeout in milliseconds, and accept `0` to wait indefinitely. They restore the last submitted frame, preserve unsubmitted drawing, and require a temporary 150 KiB RAM2 buffer. Failed calibration keeps the previous settings or the built-in defaults.
- **Persistence.** Read the full fit with `touchCalibration(AffineCalibration&)` and restore it with `setTouchCalibration(const AffineCalibration&)`. The library keeps the fit in memory. See the [example's EEPROM code](examples/TeensyTFT_Test/TeensyTFT_Test.ino) to save it across restarts. The older four-endpoint API remains available.

[Complete API](src/TeensyTFT.h) · [License](LICENSE)
