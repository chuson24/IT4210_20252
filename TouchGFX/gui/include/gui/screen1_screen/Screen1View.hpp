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
    void serviceUsbMouse();

private:
    bool pressed;
    bool releaseAnimation;
    bool dragButtonDown;
    bool clickReleasePending;
    uint8_t animationRadius;
    uint8_t requestedButtons;
    uint8_t lastSentButtons;
    uint16_t pressTicks;
    uint16_t movementDistance;
    int16_t touchX;
    int16_t touchY;
    int16_t pendingDeltaX;
    int16_t pendingDeltaY;
};

#endif // SCREEN1VIEW_HPP
