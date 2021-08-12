#ifndef MENU_IMAGE_HPP_
#define MENU_IMAGE_HPP_

#include "MenuBase.hpp"
#include <string>

class MenuImage : public MenuBase {
public:
    MenuImage(MenuMgr &master, bool selectable = false); // note : for higher level only
    ~MenuImage();

    void load(const std::string &texture, float scale = 0, int subImage = 1);
    void setSize(float x = 0, float y = 0);
    void selectImage(int subImageIdx);
    void setRect(const SDL_Rect &newSrc);
    static void setPath(const std::string &_path) {
        path = _path;
    }
    virtual void draw() override;
protected:
    virtual bool onUpdate() override;
private:
    static std::string path;
    SDL_Surface *surface = nullptr;
    void *pixels = nullptr;
    bool changed = false;
    SDL_Rect src;
};

#endif
