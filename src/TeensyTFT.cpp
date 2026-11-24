#include "TeensyTFT.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

namespace
{
// The driver can cache a valid hardware read for up to 3 ms.
constexpr unsigned TouchSampleMs = 5;
constexpr unsigned TouchResetMs = 200;
constexpr uint8_t TouchPressSamples = 2;
constexpr uint8_t TouchReleaseSamples = 3;

constexpr int CalibrationInset = 6;
constexpr int EdgeCheckInset = 3;
constexpr int TargetRadius = 10;
constexpr int TargetSize = 2 * TargetRadius + 1;
constexpr int CalibrationSamples = 7;
constexpr int MaxSampleSpread = 140;
constexpr int MaxCalibrationError = 18;

int medianOfThree(const int16_t (&samples)[3])
{
    const int low = std::min(samples[0], samples[1]);
    const int high = std::max(samples[0], samples[1]);
    return std::max(low, std::min(high, int(samples[2])));
}

struct ClippedRect
{
    int left, top, right, bottom;

    bool empty() const { return left >= right || top >= bottom; }
};

ClippedRect clipRect(int x, int y, int w, int h)
{
    return {std::max(x, 0), std::max(y, 0),
            std::min(x + w, int(TeensyTFT::Width)),
            std::min(y + h, int(TeensyTFT::Height))};
}

int mapTouch(int raw, int first, int last, int extent)
{
    const int mapped = (raw - first) * (extent - 1) / (last - first);
    return std::min(std::max(mapped, 0), extent - 1);
}

float clampPosition(float value, int extent)
{
    return std::max(0.0f, std::min(value, float(extent - 1)));
}
}

TeensyTFT::TeensyTFT(uint8_t cs, uint8_t dc, uint8_t rst, uint8_t tcs, uint8_t tirq)
    : _driver(cs, dc, 13, 11, 12, rst, tcs, tcs == 255 ? 255 : tirq),
      _touchEnabled(tcs != 255)
{
}

TeensyTFT::~TeensyTFT()
{
    waitForDisplay();
    std::free(_dmaBuffer);
}

bool TeensyTFT::begin(uint32_t spi_clock)
{
    if (_ready)
        return true;
    if (spi_clock == 0 || spi_clock > INT32_MAX)
        return false;

    // The Teensy heap lives in DMA-accessible RAM2. Align to a cache line.
    _dmaBuffer = static_cast<uint16_t*>(memalign(32, sizeof(_frameBuffer)));
    if (!_dmaBuffer)
        return false;

    if (!_driver.begin(spi_clock))
    {
        std::free(_dmaBuffer);
        _dmaBuffer = nullptr;
        return false;
    }

    _driver.setRotation(1);
    _driver.setVSyncSpacing(0);
    _driver.setTouchThreshold(_touchReleaseThreshold);
    _driver.setFramebuffer(_dmaBuffer);
    _driver.setDiffBuffers(&_diff1, &_diff2);
    _driver.update(_frameBuffer, true);
    _touchSampleTimer = TouchSampleMs;
    _ready = true;
    return true;
}

void TeensyTFT::clear(uint16_t color)
{
    std::fill_n(_frameBuffer, PixelCount, color);
}

void TeensyTFT::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (unsigned(x) < unsigned(Width) && unsigned(y) < unsigned(Height))
        _frameBuffer[y * Width + x] = color;
}

void TeensyTFT::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    fillRect(x, y, w, 1, color);
}

void TeensyTFT::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    const auto rect = clipRect(x, y, 1, h);
    if (rect.empty())
        return;

    for (int row = rect.top; row < rect.bottom; ++row)
        _frameBuffer[row * Width + rect.left] = color;
}

void TeensyTFT::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    const auto rect = clipRect(x, y, w, h);
    if (rect.empty())
        return;

    const int columns = rect.right - rect.left;
    if (columns == Width)
    {
        std::fill_n(_frameBuffer + rect.top * Width, (rect.bottom - rect.top) * Width, color);
        return;
    }

    for (int row = rect.top; row < rect.bottom; ++row)
        std::fill_n(_frameBuffer + row * Width + rect.left, columns, color);
}

void TeensyTFT::fillRegion(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint16_t* pixels, const bool* mask)
{
    const auto rect = clipRect(x, y, w, h);
    if (!pixels || rect.empty())
        return;

    const int columns = rect.right - rect.left;
    const int leftOffset = rect.left - x;
    for (int row = rect.top; row < rect.bottom; ++row)
    {
        const int offset = (row - y) * w + leftOffset;
        const uint16_t* source = pixels + offset;
        uint16_t* destination = _frameBuffer + row * Width + rect.left;
        if (mask)
        {
            const bool* rowMask = mask + offset;
            for (int column = 0; column < columns; ++column)
            {
                if (rowMask[column])
                    destination[column] = source[column];
            }
        }
        else
        {
            std::memcpy(destination, source, columns * sizeof(uint16_t));
        }
    }
}

void TeensyTFT::swap()
{
    present(_frameBuffer);
}

void TeensyTFT::present(const uint16_t* pixels)
{
    if (_ready && pixels)
        _driver.update(pixels);
}

void TeensyTFT::waitForDisplay()
{
    if (_ready)
        _driver.waitUpdateAsyncComplete();
}

bool TeensyTFT::setTouchCalibration(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom)
{
    if (left == right || top == bottom || left > 4095 || right > 4095 ||
        top > 4095 || bottom > 4095)
        return false;

    _touchCalibration = {left, right, top, bottom};
    _hasAffineCalibration = false;
    _touchActive = false;
    _touchStreak = 0;
    return true;
}

bool TeensyTFT::setTouchCalibration(const AffineCalibration& calibration)
{
    const float coefficients[] = {calibration.xx, calibration.xy, calibration.yx, calibration.yy};
    for (float coefficient : coefficients)
    {
        if (!std::isfinite(coefficient) || std::abs(coefficient) > 2.0f)
            return false;
    }
    if (!std::isfinite(calibration.xOffset) || !std::isfinite(calibration.yOffset) ||
        std::abs(calibration.xOffset) > 16384 || std::abs(calibration.yOffset) > 16384)
        return false;

    const float det = calibration.xx * calibration.yy - calibration.xy * calibration.yx;
    if (std::abs(det) < 0.00001f)
        return false;

    const auto rawAt = [&](float x, float y, bool rawX) {
        x -= calibration.xOffset;
        y -= calibration.yOffset;
        const float value = rawX ? (calibration.yy * x - calibration.xy * y) / det
                                 : (calibration.xx * y - calibration.yx * x) / det;
        return static_cast<uint16_t>(clampPosition(value, 4096) + 0.5f);
    };
    const TouchCalibration endpoints{rawAt(0, Height / 2, false),
        rawAt(Width - 1, Height / 2, false), rawAt(Width / 2, 0, true),
        rawAt(Width / 2, Height - 1, true)};
    if (endpoints.left != endpoints.right && endpoints.top != endpoints.bottom)
        _touchCalibration = endpoints;

    _affineCalibration = calibration;
    _hasAffineCalibration = true;
    _touchActive = false;
    _touchStreak = 0;
    return true;
}

bool TeensyTFT::touchCalibration(AffineCalibration& calibration) const
{
    if (!_hasAffineCalibration)
        return false;
    calibration = _affineCalibration;
    return true;
}

void TeensyTFT::mapTouchPosition(int rawX, int rawY, float& x, float& y) const
{
    if (_hasAffineCalibration)
    {
        const auto& fit = _affineCalibration;
        x = fit.xx * rawX + fit.xy * rawY + fit.xOffset;
        y = fit.yx * rawX + fit.yy * rawY + fit.yOffset;
        return;
    }
    x = mapTouch(rawY, _touchCalibration.left, _touchCalibration.right, Width);
    y = mapTouch(rawX, _touchCalibration.top, _touchCalibration.bottom, Height);
}

bool TeensyTFT::sampleTouchTargets(const TouchPosition* targets, TouchPosition* readings,
                                   size_t count, unsigned tapsPerTarget, uint32_t timeoutMs)
{
    if (!_ready || !_touchEnabled)
        return false;

    auto* savedFrame = static_cast<uint16_t*>(std::malloc(sizeof(_frameBuffer)));
    if (!savedFrame)
        return false;

    waitForDisplay();
    // ILI9341_T4 stores the DMA mirror in native portrait order, even in landscape.
    // Save the submitted frame separately from any unsubmitted drawing.
    for (int y = 0; y < Height; ++y)
        for (int x = 0; x < Width; ++x)
            savedFrame[y * Width + x] = _dmaBuffer[x * Height + Height - 1 - y];

    // Blocking region writes are sufficient for the small calibration targets.
    _driver.setFramebuffer(nullptr);
    _driver.clear(0);
    const elapsedMillis timer = 0;
    const auto hasTime = [&]() { return timeoutMs == 0 || timer < timeoutMs; };

    const auto waitForRelease = [&]() {
        int released = 0;
        while (hasTime())
        {
            int x, y, pressure;
            if (_driver.readTouch(x, y, pressure))
                released = 0;
            else if (++released == TouchReleaseSamples)
                return true;
            delay(TouchSampleMs);
        }
        return false;
    };

    const auto sampleTarget = [&](TouchPosition& reading) {
        int samplesX[CalibrationSamples], samplesY[CalibrationSamples];
        int count = 0;
        while (hasTime())
        {
            int rawX, rawY, pressure;
            if (!_driver.readTouch(rawX, rawY, pressure) ||
                (count == 0 && pressure < _touchPressThreshold))
            {
                count = 0;
            }
            else
            {
                samplesX[count] = rawX;
                samplesY[count] = rawY;
                if (++count == CalibrationSamples)
                {
                    std::sort(samplesX, samplesX + count);
                    std::sort(samplesY, samplesY + count);
                    if (samplesX[count - 1] - samplesX[0] <= MaxSampleSpread &&
                        samplesY[count - 1] - samplesY[0] <= MaxSampleSpread)
                    {
                        reading = {samplesX[count / 2], samplesY[count / 2]};
                        return true;
                    }
                    count = 0;
                }
            }
            delay(TouchSampleMs);
        }
        return false;
    };

    uint16_t target[TargetSize * TargetSize];
    const uint16_t blankTarget[TargetSize * TargetSize]{};
    bool completed = true;
    for (size_t i = 0; i < count && completed; ++i)
    {
        const auto center = targets[i];
        const int left = std::max(0, center.x - TargetRadius);
        const int right = std::min(int(Width) - 1, center.x + TargetRadius);
        const int top = std::max(0, center.y - TargetRadius);
        const int bottom = std::min(int(Height) - 1, center.y + TargetRadius);
        uint16_t* pixel = target;
        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                const int dx = x - center.x;
                const int dy = y - center.y;
                const int squared = dx * dx + dy * dy;
                *pixel++ = dx == 0 || dy == 0 || (squared >= 36 && squared <= 56) ? 0xFFFF : 0;
            }
        }
        _driver.updateRegion(true, target, left, right, top, bottom);
        for (unsigned tap = 0; tap < tapsPerTarget && completed; ++tap)
            completed = waitForRelease() && sampleTarget(readings[i * tapsPerTarget + tap]);
        if (completed && i + 1 < count)
            _driver.updateRegion(true, blankTarget, left, right, top, bottom);
    }

    completed = completed && waitForRelease();

    _driver.setFramebuffer(_dmaBuffer);
    _driver.update(savedFrame, true);
    waitForDisplay();
    std::free(savedFrame);
    _touchActive = false;
    _touchStreak = 0;
    _touchSampleTimer = 0;
    return completed;
}

bool TeensyTFT::calibrateTouch(uint32_t timeoutMs)
{
    constexpr TouchPosition targets[] = {
        {CalibrationInset, CalibrationInset}, {Width / 2, CalibrationInset},
        {Width - 1 - CalibrationInset, CalibrationInset},
        {CalibrationInset, Height / 2}, {Width / 2, Height / 2},
        {Width - 1 - CalibrationInset, Height / 2},
        {CalibrationInset, Height - 1 - CalibrationInset},
        {Width / 2, Height - 1 - CalibrationInset},
        {Width - 1 - CalibrationInset, Height - 1 - CalibrationInset}
    };
    constexpr size_t count = sizeof(targets) / sizeof(targets[0]);
    TouchPosition raw[count];
    if (!sampleTouchTargets(targets, raw, count, 1, timeoutMs))
        return false;

    // Solve the centered least-squares system for both screen axes.
    double meanRawX = 0, meanRawY = 0, meanScreenX = 0, meanScreenY = 0;
    for (size_t i = 0; i < count; ++i)
    {
        meanRawX += raw[i].x;
        meanRawY += raw[i].y;
        meanScreenX += targets[i].x;
        meanScreenY += targets[i].y;
    }
    meanRawX /= count;
    meanRawY /= count;
    meanScreenX /= count;
    meanScreenY /= count;

    double xx = 0, xy = 0, yy = 0;
    double rawXScreenX = 0, rawYScreenX = 0, rawXScreenY = 0, rawYScreenY = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const double rx = raw[i].x - meanRawX;
        const double ry = raw[i].y - meanRawY;
        const double sx = targets[i].x - meanScreenX;
        const double sy = targets[i].y - meanScreenY;
        xx += rx * rx;
        xy += rx * ry;
        yy += ry * ry;
        rawXScreenX += rx * sx;
        rawYScreenX += ry * sx;
        rawXScreenY += rx * sy;
        rawYScreenY += ry * sy;
    }
    const double det = xx * yy - xy * xy;
    if (xx < 250000 || yy < 250000 || det <= 0.01 * xx * yy)
        return false;

    AffineCalibration fit;
    fit.xx = (yy * rawXScreenX - xy * rawYScreenX) / det;
    fit.xy = (xx * rawYScreenX - xy * rawXScreenX) / det;
    fit.yx = (yy * rawXScreenY - xy * rawYScreenY) / det;
    fit.yy = (xx * rawYScreenY - xy * rawXScreenY) / det;
    fit.xOffset = meanScreenX - fit.xx * meanRawX - fit.xy * meanRawY;
    fit.yOffset = meanScreenY - fit.yx * meanRawX - fit.yy * meanRawY;

    for (size_t i = 0; i < count; ++i)
    {
        const float dx = fit.xx * raw[i].x + fit.xy * raw[i].y + fit.xOffset - targets[i].x;
        const float dy = fit.yx * raw[i].x + fit.yy * raw[i].y + fit.yOffset - targets[i].y;
        if (dx * dx + dy * dy > MaxCalibrationError * MaxCalibrationError)
            return false;
    }
    return setTouchCalibration(fit);
}

bool TeensyTFT::checkTouchEdgeAccuracy(EdgeAccuracy& result, uint32_t timeoutMs)
{
    constexpr int right = Width - 1 - EdgeCheckInset;
    constexpr int bottom = Height - 1 - EdgeCheckInset;
    constexpr TouchPosition targets[] = {
        {Width / 4, EdgeCheckInset}, {3 * Width / 4, EdgeCheckInset},
        {right, Height / 4}, {right, 3 * Height / 4},
        {3 * Width / 4, bottom}, {Width / 4, bottom},
        {EdgeCheckInset, 3 * Height / 4}, {EdgeCheckInset, Height / 4}
    };
    constexpr size_t count = sizeof(targets) / sizeof(targets[0]);
    TouchPosition raw[count * 2];
    if (!sampleTouchTargets(targets, raw, count, 2, timeoutMs))
        return false;

    float totalSquared = 0, maxSquared = 0, maxSeparationSquared = 0;
    TouchPosition worst = targets[0];
    for (size_t i = 0; i < count; ++i)
    {
        float x[2], y[2];
        for (int tap = 0; tap < 2; ++tap)
        {
            const auto reading = raw[2 * i + tap];
            mapTouchPosition(reading.x, reading.y, x[tap], y[tap]);
            const float dx = x[tap] - targets[i].x;
            const float dy = y[tap] - targets[i].y;
            const float squared = dx * dx + dy * dy;
            totalSquared += squared;
            if (squared > maxSquared)
            {
                maxSquared = squared;
                worst = targets[i];
            }
        }
        const float dx = x[0] - x[1];
        const float dy = y[0] - y[1];
        maxSeparationSquared = std::max(maxSeparationSquared, dx * dx + dy * dy);
    }
    result = {std::sqrt(totalSquared / (2 * count)), std::sqrt(maxSquared),
        std::sqrt(maxSeparationSquared), static_cast<int16_t>(worst.x), static_cast<int16_t>(worst.y)};
    return true;
}

bool TeensyTFT::setTouchPressureThresholds(uint16_t press, uint16_t release)
{
    if (release == 0 || release >= press || press > 4095)
        return false;

    _touchPressThreshold = press;
    _touchReleaseThreshold = release;
    _driver.setTouchThreshold(release);
    _touchActive = false;
    _touchStreak = 0;
    return true;
}

bool TeensyTFT::touched()
{
    if (!_ready || !_touchEnabled)
        return false;

    const unsigned elapsed = _touchSampleTimer;
    if (elapsed < TouchSampleMs)
        return _touchActive;
    if (elapsed >= TouchResetMs)
    {
        _touchActive = false;
        _touchStreak = 0;
    }

    int rawX, rawY, pressure;
    const bool sampled = _driver.readTouch(rawX, rawY, pressure);
    _touchSampleTimer = 0;

    if (_touchActive)
    {
        if (!sampled)
        {
            if (++_touchStreak == TouchReleaseSamples)
            {
                _touchActive = false;
                _touchStreak = 0;
            }
            return _touchActive;
        }
        _touchStreak = 0;
    }
    else
    {
        if (!sampled || pressure < _touchPressThreshold)
        {
            _touchStreak = 0;
            return false;
        }
        if (++_touchStreak < TouchPressSamples)
            return false;
        _touchStreak = 0;
    }

    float mappedX, mappedY;
    mapTouchPosition(rawX, rawY, mappedX, mappedY);
    const int16_t x = static_cast<int16_t>(clampPosition(mappedX, Width) + 0.5f);
    const int16_t y = static_cast<int16_t>(clampPosition(mappedY, Height) + 0.5f);
    if (!_touchActive)
    {
        std::fill_n(_touchHistoryX, 3, x);
        std::fill_n(_touchHistoryY, 3, y);
        _touchHistoryIndex = 0;
        _filteredX = x;
        _filteredY = y;
        _touchActive = true;
    }
    else
    {
        _touchHistoryX[_touchHistoryIndex] = x;
        _touchHistoryY[_touchHistoryIndex] = y;
        _touchHistoryIndex = (_touchHistoryIndex + 1) % 3;
        _filteredX += 0.3f * (medianOfThree(_touchHistoryX) - _filteredX);
        _filteredY += 0.3f * (medianOfThree(_touchHistoryY) - _filteredY);
    }

    _touchX = static_cast<int16_t>(_filteredX + 0.5f);
    _touchY = static_cast<int16_t>(_filteredY + 0.5f);
    _touchZ = static_cast<int16_t>(pressure);
    return true;
}
