#ifndef MENU_TEXT_HPP_
#define MENU_TEXT_HPP_

#include "MenuBase.hpp"
#include <string>

class MenuText : public MenuBase {
public:
    MenuText(MenuMgr &master, const SDL_Color &color = {240, 240, 240, 255});
    ~MenuText();

    void setColor(const SDL_Color &color);
    void setString(const std::string &str);
    virtual void draw() override;
protected:
    virtual bool onUpdate() override;
private:
    SDL_Surface *surface = nullptr;
    bool changed = false;
    SDL_Color color;
    std::string str;
};

#endif
