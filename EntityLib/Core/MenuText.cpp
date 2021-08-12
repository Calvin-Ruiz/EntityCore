#include "MenuText.hpp"
#include "MenuMgr.hpp"

MenuText::MenuText(MenuMgr &master, const SDL_Color &color) : MenuBase(master), color(color)
{
}

MenuText::~MenuText()
{
    if (surface)
        SDL_FreeSurface(surface);
}

void MenuText::setColor(const SDL_Color &_color)
{
    if (memcmp(&color, &_color, sizeof(color))) {
        color = _color;
        changed = true;
    }
}

void MenuText::setString(const std::string &_str)
{
    if (str != _str) {
        str = _str;
        changed = true;
    }
}

void MenuText::draw()
{
    if (surface)
        SDL_BlitSurface(surface, NULL, master.surface, &dest);
}

bool MenuText::onUpdate()
{
    if (changed) {
        if (surface)
            SDL_FreeSurface(surface);
        surface = TTF_RenderUTF8_Blended(master.myFont, str.c_str(), color);
        setRatio(surface->w / (float) surface->h);
        changed = false;
        return true;
    }
    return false;
}
