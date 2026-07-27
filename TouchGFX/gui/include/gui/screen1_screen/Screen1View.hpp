#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}

    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleClickEvent(const touchgfx::ClickEvent& evt);
    virtual void handleTickEvent();
    virtual void handleDragEvent(const touchgfx::DragEvent& evt);

protected:
    void hideAllCircles();
    void queueMouseMovement(int16_t deltaX, int16_t deltaY);
    void queueScroll(int16_t deltaY);
    void serviceUsbMouse();

private:
    bool     pressed;
    bool     releaseAnimation;
    bool     dragButtonDown;
    bool     clickReleasePending;
    bool     scrollMode;           /**< True when the touch started in the scroll zone (right edge) */

    /* animationRadius: signed so decrement never wraps below zero (was uint8_t → bug) */
    int16_t  animationRadius;

    uint8_t  requestedButtons;
    uint8_t  lastSentButtons;
    uint16_t pressTicks;
    uint16_t movementDistance;
    int16_t  touchX;
    int16_t  touchY;
    int16_t  pendingDeltaX;
    int16_t  pendingDeltaY;
    int16_t  pendingScroll;        /**< Accumulated scroll wheel delta, clamped to -127..127 */

    /**
     * Runtime pointer gain (default 1).  Higher values increase cursor speed.
     * Pointer acceleration multiplies this by an additional factor when the
     * finger moves quickly.
     */
    int16_t  pointerGain;
};

#endif // SCREEN1VIEW_HPP
