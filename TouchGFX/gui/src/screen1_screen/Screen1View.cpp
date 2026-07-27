#include <gui/screen1_screen/Screen1View.hpp>

#ifndef SIMULATOR
extern "C"
{
#include "main.h"
#include "usb_device.h"
}
#endif

namespace
{
/* -------------------------------------------------------------------------
 * Tap / hold gesture thresholds
 * All tick-based values depend on the TouchGFX tick rate (typically 16 ms/tick
 * at 60 FPS, giving HOLD_TICKS ≈ 480 ms, MAX_TAP_TICKS ≈ 288 ms).
 * -------------------------------------------------------------------------*/
const uint16_t HOLD_TICKS         = 30;  /**< Ticks finger must be held still to enter drag mode */
const uint16_t MIN_TAP_TICKS      =  3;  /**< Minimum ticks for a tap to be recognised (debounce) */
const uint16_t MAX_TAP_TICKS      = 18;  /**< Maximum ticks — longer presses become holds, not taps */
const uint16_t TAP_MOVEMENT_LIMIT =  4;  /**< Max Manhattan distance (px) still counted as a tap */

/* -------------------------------------------------------------------------
 * Scroll zone
 * The rightmost SCROLL_ZONE_WIDTH pixels of the 240-wide screen enter scroll
 * mode: vertical finger movement is sent as HID wheel data instead of cursor
 * movement.  The visual circle is still drawn, but no cursor motion is queued.
 * -------------------------------------------------------------------------*/
const int16_t  SCROLL_ZONE_WIDTH  = 30;  /**< Width (px) of the scroll strip on the right edge */
const int16_t  SCREEN_WIDTH       = 240; /**< LCD width in pixels (portrait) */

/* -------------------------------------------------------------------------
 * Animation
 * -------------------------------------------------------------------------*/
const int16_t  ANIMATION_START_RADIUS = 40; /**< Initial radius of the touch feedback circle */
const int16_t  ANIMATION_RADIUS_STEP  =  2; /**< Radius decrease per tick during release animation */

/* -------------------------------------------------------------------------
 * Pointer acceleration
 * When a drag event exceeds the threshold, the movement is scaled up by the
 * corresponding gain factor, giving a "fast finger → fast cursor" feel.
 *
 * Speed is approximated by the Manhattan distance of the per-tick drag event
 * (in display pixels), which is proportional to actual finger velocity.
 * -------------------------------------------------------------------------*/
const int16_t  ACCEL_THRESHOLD_FAST   = 20; /**< Speed (px/tick) above which fast gain applies */
const int16_t  ACCEL_THRESHOLD_MED    = 10; /**< Speed (px/tick) above which medium gain applies */
const int16_t  ACCEL_GAIN_FAST        =  3; /**< Multiplier for fast movements */
const int16_t  ACCEL_GAIN_MED         =  2; /**< Multiplier for medium movements */
const int16_t  ACCEL_GAIN_SLOW        =  1; /**< Multiplier for slow movements (1 = no scaling) */

/* -------------------------------------------------------------------------
 * HID delta clamp
 * USB HID relative axes are int8_t (-128..127).  We clamp to -127..127 to
 * keep the range symmetric and avoid the one asymmetric value (-128) that
 * some older host drivers may interpret incorrectly.
 * -------------------------------------------------------------------------*/
int8_t clampMouseDelta(int16_t value)
{
    if (value > 127)
    {
        return 127;
    }
    if (value < -127)
    {
        return -127;
    }
    return static_cast<int8_t>(value);
}
} // anonymous namespace

/* ---------------------------------------------------------------------------
 * Constructor
 * ---------------------------------------------------------------------------*/
Screen1View::Screen1View() :
    pressed(false),
    releaseAnimation(false),
    dragButtonDown(false),
    clickReleasePending(false),
    scrollMode(false),
    animationRadius(0),
    requestedButtons(0),
    lastSentButtons(0),
    pressTicks(0),
    movementDistance(0),
    touchX(0),
    touchY(0),
    pendingDeltaX(0),
    pendingDeltaY(0),
    pendingScroll(0),
    pointerGain(1)
{
}

/* ---------------------------------------------------------------------------
 * Screen lifecycle
 * ---------------------------------------------------------------------------*/
void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
    hideAllCircles();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

/* ---------------------------------------------------------------------------
 * hideAllCircles
 *
 * Only circle1 is used in this design.  The function is kept general for easy
 * future extension, but the unused circles (circle2–4) defined in the
 * TouchGFX-generated base class are simply not shown.
 * ---------------------------------------------------------------------------*/
void Screen1View::hideAllCircles()
{
    if (circle1.isVisible())
    {
        circle1.invalidate();
        circle1.setVisible(false);
    }
}

/* ---------------------------------------------------------------------------
 * handleClickEvent
 * ---------------------------------------------------------------------------*/
void Screen1View::handleClickEvent(const touchgfx::ClickEvent& evt)
{
    Screen1ViewBase::handleClickEvent(evt);

    switch(evt.getType())
    {
    case touchgfx::ClickEvent::PRESSED:
        pressed           = true;
        releaseAnimation  = false;
        dragButtonDown    = false;
        pressTicks        = 0;
        movementDistance  = 0;
        touchX            = evt.getX();
        touchY            = evt.getY();

        /* Determine interaction mode based on initial touch X position.
         * Right-edge strip → scroll mode; rest of screen → cursor mode. */
        scrollMode = (touchX >= SCREEN_WIDTH - SCROLL_ZONE_WIDTH);

        hideAllCircles();
        circle1.setXY(touchX - ANIMATION_START_RADIUS, touchY - ANIMATION_START_RADIUS);
        circle1.setRadius(ANIMATION_START_RADIUS);
        circle1.setVisible(true);
        circle1.invalidate();
        break;

    case touchgfx::ClickEvent::RELEASED:
        pressed = false;
        if (dragButtonDown)
        {
            /* Release the held left button */
            requestedButtons = 0;
            dragButtonDown   = false;
        }
        else if (!scrollMode &&
                 pressTicks >= MIN_TAP_TICKS &&
                 pressTicks <= MAX_TAP_TICKS &&
                 movementDistance <= TAP_MOVEMENT_LIMIT)
        {
            /* Short stationary touch → left click */
            requestedButtons     = 1;
            clickReleasePending  = true;
        }
        releaseAnimation = true;
        animationRadius  = ANIMATION_START_RADIUS;
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------------------------
 * handleDragEvent
 * ---------------------------------------------------------------------------*/
void Screen1View::handleDragEvent(const touchgfx::DragEvent& evt)
{
    Screen1ViewBase::handleDragEvent(evt);

    if (!pressed)
    {
        return;
    }

    touchX = evt.getNewX();
    touchY = evt.getNewY();

    const int16_t deltaX = evt.getNewX() - evt.getOldX();
    const int16_t deltaY = evt.getNewY() - evt.getOldY();

    /* Accumulate Manhattan distance for tap/hold discrimination */
    const uint16_t distance = static_cast<uint16_t>((deltaX < 0 ? -deltaX : deltaX) +
                                                     (deltaY < 0 ? -deltaY : deltaY));
    if (movementDistance <= static_cast<uint16_t>(0xFFFF - distance))
    {
        movementDistance += distance;
    }
    else
    {
        movementDistance = 0xFFFF;
    }

    if (scrollMode)
    {
        /* In the scroll zone, vertical movement drives the scroll wheel */
        queueScroll(deltaY);
    }
    else
    {
        /* Normal touchpad mode: apply pointer acceleration then queue movement */
        queueMouseMovement(deltaX, deltaY);
    }

    /* Keep the feedback circle centred on the finger */
    circle1.invalidate();
    circle1.setXY(touchX - animationRadius, touchY - animationRadius);
    circle1.invalidate();
}

/* ---------------------------------------------------------------------------
 * handleTickEvent
 * ---------------------------------------------------------------------------*/
void Screen1View::handleTickEvent()
{
    Screen1ViewBase::handleTickEvent();

    if (pressed)
    {
        if (pressTicks < 0xFFFF)
        {
            pressTicks++;
        }

        /* Enter hold/drag state when finger has been still long enough */
        if (!scrollMode &&
            !dragButtonDown &&
            pressTicks >= HOLD_TICKS &&
            movementDistance <= TAP_MOVEMENT_LIMIT)
        {
            requestedButtons = 1;   /* Assert left button */
            dragButtonDown   = true;
        }
    }

    serviceUsbMouse();

    /* ---- Release animation ---- */
    if (!releaseAnimation)
    {
        return;
    }

    circle1.invalidate();

    if (animationRadius <= ANIMATION_RADIUS_STEP)
    {
        /* Animation complete — hide circle */
        circle1.setVisible(false);
        releaseAnimation = false;
        return;
    }

    animationRadius -= ANIMATION_RADIUS_STEP;
    circle1.setRadius(static_cast<uint8_t>(animationRadius));
    circle1.setVisible(true);
    circle1.invalidate();
}

/* ---------------------------------------------------------------------------
 * queueMouseMovement
 *
 * Applies pointer acceleration and accumulates the movement.  Overflow is
 * prevented by clamping to the range -1024..1024; the USB service drains
 * this queue at up to ±127 per HID report.
 * ---------------------------------------------------------------------------*/
void Screen1View::queueMouseMovement(int16_t deltaX, int16_t deltaY)
{
    /* Pointer acceleration: scale up fast movements so the cursor covers
     * large distances without requiring equally large finger sweeps. */
    const int16_t speed = static_cast<int16_t>((deltaX < 0 ? -deltaX : deltaX) +
                                                (deltaY < 0 ? -deltaY : deltaY));
    int16_t accelGain;
    if (speed > ACCEL_THRESHOLD_FAST)
    {
        accelGain = ACCEL_GAIN_FAST;
    }
    else if (speed > ACCEL_THRESHOLD_MED)
    {
        accelGain = ACCEL_GAIN_MED;
    }
    else
    {
        accelGain = ACCEL_GAIN_SLOW;
    }

    const int16_t effectiveGain = pointerGain * accelGain;

    int32_t accumulatedX = pendingDeltaX + static_cast<int32_t>(deltaX) * effectiveGain;
    int32_t accumulatedY = pendingDeltaY + static_cast<int32_t>(deltaY) * effectiveGain;

    /* Clamp accumulated deltas to prevent runaway accumulation */
    if (accumulatedX > 1024)       { accumulatedX =  1024; }
    else if (accumulatedX < -1024) { accumulatedX = -1024; }

    if (accumulatedY > 1024)       { accumulatedY =  1024; }
    else if (accumulatedY < -1024) { accumulatedY = -1024; }

    pendingDeltaX = static_cast<int16_t>(accumulatedX);
    pendingDeltaY = static_cast<int16_t>(accumulatedY);
}

/* ---------------------------------------------------------------------------
 * queueScroll
 *
 * Accumulates a scroll wheel delta.  Only the Y axis is used; the sign
 * convention (positive = scroll down) matches the USB HID specification.
 * ---------------------------------------------------------------------------*/
void Screen1View::queueScroll(int16_t deltaY)
{
    int32_t accumulated = pendingScroll + static_cast<int32_t>(deltaY);

    if (accumulated > 127)       { accumulated =  127; }
    else if (accumulated < -127) { accumulated = -127; }

    pendingScroll = static_cast<int16_t>(accumulated);
}

/* ---------------------------------------------------------------------------
 * serviceUsbMouse
 *
 * Converts the pending state into a HID report and attempts to send it.
 * On success the sent amount is subtracted from the pending deltas so
 * large movements are delivered across multiple consecutive reports.
 * The function is non-blocking: if USB is busy it returns immediately and
 * will retry on the next tick.
 * ---------------------------------------------------------------------------*/
void Screen1View::serviceUsbMouse()
{
    const bool noMovement    = (pendingDeltaX == 0 && pendingDeltaY == 0);
    const bool noScroll      = (pendingScroll == 0);
    const bool buttonsUnchanged = (requestedButtons == lastSentButtons);

    if (noMovement && noScroll && buttonsUnchanged)
    {
        return;  /* Nothing to send */
    }

    const int8_t reportX      = clampMouseDelta(pendingDeltaX);
    const int8_t reportY      = clampMouseDelta(pendingDeltaY);
    const int8_t reportScroll = clampMouseDelta(pendingScroll);

#ifndef SIMULATOR
    if (!USB_Mouse_TrySend(requestedButtons, reportX, reportY, reportScroll))
    {
        return;  /* USB busy — will retry next tick */
    }
#endif

    /* Report accepted: subtract the transmitted portion from the pending queue */
    pendingDeltaX -= reportX;
    pendingDeltaY -= reportY;
    pendingScroll -= reportScroll;
    lastSentButtons = requestedButtons;

    /* Auto-release left button one tick after a click */
    if (clickReleasePending && requestedButtons == 1)
    {
        requestedButtons    = 0;
        clickReleasePending = false;
    }
}
