#include "MenuImage.hpp"
#include "MenuMgr.hpp"
#include "stb_image.h"

std::string MenuImage::path = "textures/";

MenuImage::MenuImage(MenuMgr &master, bool selectable) : MenuBase(master, 0, selectable)
{
    addDependency({this, 0, 0, Align::LEFT | Align::UP, 0});
}

MenuImage::~MenuImage()
{
    if (surface)
        SDL_FreeSurface(surface);
    if (pixels)
        stbi_image_free(pixels);
}

void MenuImage::load(const std::string &texture, float scale, int subImage)
{
    int height = 0, width = 0, channels = 0;

    changed = true;
    if (surface) {
        SDL_FreeSurface(surface);
        surface = nullptr;
    }
    if (pixels) {
        stbi_image_free(pixels);
        pixels = nullptr;
    }
    pixels = stbi_load((path + texture).c_str(), &width, &height, &channels, 4);
    if (!pixels)
        return;
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        const unsigned int rmask = 0xff000000;
        const unsigned int gmask = 0x00ff0000;
        const unsigned int bmask = 0x0000ff00;
        const unsigned int amask = 0x000000ff;
    #else // little endian, like x86
        const unsigned int rmask = 0x000000ff;
        const unsigned int gmask = 0x0000ff00;
        const unsigned int bmask = 0x00ff0000;
        const unsigned int amask = 0xff000000;
    #endif
    SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, 4*width, rmask, gmask, bmask, amask);
    setSize(scale * width / master.width, scale * height / master.height);
    src = SDL_Rect({0, 0, width, height / subImage});
}

void MenuImage::setSize(float x, float y)
{
    auto &dep = modifyDependency(0);

    if (x * y == 0) {
        dep.alignTo = 0;
    } else {
        dep.alignTo = Align::RIGHT | Align::DOWN;
        dep.relX = x;
        dep.relY = y;
    }
    changed = true;
}

void MenuImage::selectImage(int subImageIdx)
{
    if (src.y == src.w * subImageIdx)
        return;
    src.y = src.w * subImageIdx;
    changed = true;
}

void MenuImage::setRect(const SDL_Rect &newSrc)
{
    src = newSrc;
    changed = true;
}

void MenuImage::draw()
{
    if (surface)
        SDL_BlitSurface(surface, &src, master.surface, &dest);
}

bool MenuImage::onUpdate()
{
    if (changed) {
        changed = false;
        return true;
    }
    return false;
}
