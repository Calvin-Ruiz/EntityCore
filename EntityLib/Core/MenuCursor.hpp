#ifndef MENU_CURSOR_HPP_
#define MENU_CURSOR_HPP_

#include "MenuBase.hpp"

class MenuCursor : public MenuBase {
public:
    MenuCursor(MenuMgr &master);
    ~MenuCursor();

    virtual void draw() override;
protected:
    virtual bool onUpdate() override;
private:
    SDL_Surface *surface = nullptr;
    bool changed = false;
};

#endif
