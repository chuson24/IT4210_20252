#include <gui/screen1_screen/Screen1View.hpp>

#ifndef SIMULATOR
extern "C"
{
#include "main.h"
}
#endif

namespace
{
const uint16_t HOLD_TICKS = 30;
const uint16_t TAP_MOVEMENT_LIMIT = 8;
const int16_t POINTER_GAIN = 2;

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
}

Screen1View::Screen1View() :
    pressed(false),
    releaseAnimation(false),
    dragButtonDown(false),
    clickReleasePending(false),
    animationStep(0),
    tickCount(0),
    requestedButtons(0),
    lastSentButtons(0),
    pressTicks(0),
    movementDistance(0),
    touchX(0),
    touchY(0),
    pendingDeltaX(0),
    pendingDeltaY(0)
{
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    hideAllCircles();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::hideAllCircles()
{
    touchgfx::Circle* circles[] = { &circle1, &circle2, &circle3, &circle4 };
    for (uint8_t i = 0; i < 4; i++)
    {
        if (circles[i]->isVisible())
        {
            // Invalidate while still visible so the background repaints the old area.
            circles[i]->invalidate();
            circles[i]->setVisible(false);
        }
    }
}

void Screen1View::handleClickEvent(const touchgfx::ClickEvent& evt)
{
    Screen1ViewBase::handleClickEvent(evt);

    switch(evt.getType())
    {
    case touchgfx::ClickEvent::PRESSED:
        pressed = true;
        releaseAnimation = false;
        dragButtonDown = false;
        pressTicks = 0;
        movementDistance = 0;
        touchX = evt.getX();
        touchY = evt.getY();
        hideAllCircles();
        circle1.setXY(touchX - 40, touchY - 40);
        circle1.setRadius(40);
        circle1.setVisible(true);
        circle1.invalidate();
        break;

    case touchgfx::ClickEvent::RELEASED:
        pressed = false;
        if (dragButtonDown)
        {
            requestedButtons = 0;
            dragButtonDown = false;
        }
        else if (pressTicks < HOLD_TICKS && movementDistance <= TAP_MOVEMENT_LIMIT)
        {
            requestedButtons = 1;
            clickReleasePending = true;
        }
        releaseAnimation = true;
        animationStep = 1;
        tickCount = 0;
        break;

    default:
        break;
    }
}

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
    queueMouseMovement(deltaX, deltaY);

    circle1.invalidate();
    circle1.setXY(touchX - 40, touchY - 40);
    circle1.invalidate();
}

void Screen1View::handleTickEvent()
{
    Screen1ViewBase::handleTickEvent();

    if (pressed)
    {
        if (pressTicks < 0xFFFF)
        {
            pressTicks++;
        }

        if (!dragButtonDown &&
            pressTicks >= HOLD_TICKS &&
            movementDistance <= TAP_MOVEMENT_LIMIT)
        {
            requestedButtons = 1;
            dragButtonDown = true;
        }
    }

    serviceUsbMouse();

    if (!releaseAnimation)
    {
        return;
    }

    tickCount++;

    if (tickCount < 6)
    {
        return;
    }

    tickCount = 0;

    hideAllCircles();

    switch(animationStep)
    {
    case 1:
        circle1.setRadius(30);
        circle1.setVisible(true);
        circle1.invalidate();
        break;

    case 2:
        circle1.setRadius(20);
        circle1.setVisible(true);
        circle1.invalidate();
        break;

    case 3:
        circle1.setRadius(10);
        circle1.setVisible(true);
        circle1.invalidate();
        break;

    default:
        releaseAnimation = false;
        return;
    }

    animationStep++;
}

void Screen1View::queueMouseMovement(int16_t deltaX, int16_t deltaY)
{
    int32_t accumulatedX = pendingDeltaX + static_cast<int32_t>(deltaX) * POINTER_GAIN;
    int32_t accumulatedY = pendingDeltaY + static_cast<int32_t>(deltaY) * POINTER_GAIN;

    if (accumulatedX > 1024)
    {
        accumulatedX = 1024;
    }
    else if (accumulatedX < -1024)
    {
        accumulatedX = -1024;
    }

    if (accumulatedY > 1024)
    {
        accumulatedY = 1024;
    }
    else if (accumulatedY < -1024)
    {
        accumulatedY = -1024;
    }

    pendingDeltaX = static_cast<int16_t>(accumulatedX);
    pendingDeltaY = static_cast<int16_t>(accumulatedY);
}

void Screen1View::serviceUsbMouse()
{
    if (pendingDeltaX == 0 &&
        pendingDeltaY == 0 &&
        requestedButtons == lastSentButtons)
    {
        return;
    }

    const int8_t reportX = clampMouseDelta(pendingDeltaX);
    const int8_t reportY = clampMouseDelta(pendingDeltaY);

#ifndef SIMULATOR
    if (!USB_Mouse_TrySend(requestedButtons, reportX, reportY))
    {
        return;
    }
#endif

    pendingDeltaX -= reportX;
    pendingDeltaY -= reportY;
    lastSentButtons = requestedButtons;

    if (clickReleasePending && requestedButtons == 1)
    {
        requestedButtons = 0;
        clickReleasePending = false;
    }
}
