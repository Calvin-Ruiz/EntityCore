#include "MenuButton.hpp"
#include "MenuText.hpp"
#include "MenuMgr.hpp"

MenuButton::MenuButton(MenuMgr &master, void *data, void (*onClic)(void *data, MenuButton *), const std::string &str, const std::string &name, float offset) : MenuImage(master, true), data(data), onClic(onClic)
{
    text = new MenuText(master);
    text->addDependency({this, 0, offset, Align::UP, Align::UP});
    text->addDependency({this, 0, -offset, Align::DOWN, Align::DOWN});
    text->addDependency({this, 0, 0, Align::LEFT | Align::RIGHT, Align::LEFT | Align::RIGHT});
    text->setString(str);
    load(name, 0, 3);
}

MenuButton::~MenuButton()
{
    // if (surface)
    //     SDL_FreeSurface(surface);
    delete text;
}

void MenuButton::setColor(const SDL_Color &color)
{
    text->setColor(color);
}

void MenuButton::setString(const std::string &str)
{
    text->setString(str);
}

bool MenuButton::onUpdate()
{
    clicking &= hoverMask;
    if (clicking & ~master.clicmask) {
        onClic(data, this);
    }
    clicking = master.clicmask & hoverMask;
    if (clicking) {
        selectImage(2);
    } else if (hoverMask) {
        selectImage(1);
    } else {
        selectImage(0);
    }
    return MenuImage::onUpdate();
}
