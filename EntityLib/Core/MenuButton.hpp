#ifndef MENU_BUTTON_HPP_
#define MENU_BUTTON_HPP_

#include "MenuImage.hpp"

class MenuText;

class MenuButton : public MenuImage {
public:
    MenuButton(MenuMgr &master, void *data, void (*onClic)(void *data, MenuButton *), const std::string &str = "\0", const std::string &name = "textures/button.png", float offset = 0);
    ~MenuButton();

    void setColor(const SDL_Color &color);
    void setString(const std::string &str);
protected:
    virtual bool onUpdate() override;
private:
    MenuText *text;
    void *data;
    void (*onClic)(void *data, MenuButton *);
    unsigned char clicking = 0;
};

#endif
