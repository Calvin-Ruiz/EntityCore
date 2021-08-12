#include "MenuCursor.hpp"
#include "MenuMgr.hpp"

MenuCursor::MenuCursor(MenuMgr &master) : MenuBase(master)
{
}

MenuCursor::~MenuCursor()
{
    if (surface)
        SDL_FreeSurface(surface);
}

void MenuCursor::draw()
{
    if (surface)
        SDL_BlitSurface(surface, NULL, master.surface, &dest);
}

bool MenuCursor::onUpdate()
{
    if (changed) {
        changed = false;
        return true;
    }
    return false;
}
