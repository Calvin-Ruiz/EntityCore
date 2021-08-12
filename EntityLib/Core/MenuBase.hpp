#ifndef MENUBASE_HPP_
#define MENUBASE_HPP_

class MenuMgr;
#include <SDL2/SDL.h>
#include <memory>
#include <vector>
#include <list>

enum Align : unsigned char {
    // Note : The combination of LEFT and RIGHT or UP and DOWN result in a CENTER alignment for this axis
    LEFT = 0x01,
    RIGHT = 0x02,
    UP = 0x04,
    DOWN = 0x08,
    CENTER = LEFT | RIGHT | UP | DOWN
};

class MenuBase;

struct Dependency {
    MenuBase *master;
    float relX;
    float relY;
    unsigned char alignFrom;
    unsigned char alignTo;
};

class MenuBase {
public:
    MenuBase(MenuMgr &master, float ratio = 0, bool selectable = false);
    virtual ~MenuBase();

    int addDependency(Dependency dep);
    // note : returned dependency can be accessed (read/write) between this call and the first call to any of addDependency, update and resized
    Dependency &modifyDependency(int id);
    // return true if redraw is needed
    bool update();
    virtual void draw() {}
    // ratio must be positive for width depending to height, negative for height depending to width
    // note : the ratio use screen coordinates, so a ratio of 1 or -1 is always a square
    void setRatio(float ratio = 0);
protected:
    virtual bool onUpdate() {return false;}
    void resized();
    // Normalized coordinates
    float x1 = 0;
    float y1 = 0;
    float x2 = 1;
    float y2 = 1;
    // Destination rectangle for draw
    SDL_Rect dest;
    // Is this element hovered ? By who ?
    unsigned char hoverMask = 0;
    // Menu
    MenuMgr &master;
private:
    std::list<MenuBase *>::iterator handle;
    std::vector<Dependency> dependency;
    std::vector<MenuBase *> dependants;
    float ratio;
    const int hoverID;
    bool isResized = false;
};

#endif
