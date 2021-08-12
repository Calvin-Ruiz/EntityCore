#include "MenuBase.hpp"
#include "MenuMgr.hpp"

MenuBase::MenuBase(MenuMgr &master, float ratio, bool selectable) :
    master(master), handle(master.attach(this)), ratio(ratio), hoverID(selectable ? master.lastHoverID++ : -1) {}

MenuBase::~MenuBase()
{
    master.detach(handle);
}

int MenuBase::addDependency(Dependency dep)
{
    dependency.push_back(dep);
    isResized = true;
    if (dep.master == this)
        return (dependency.size() - 1);
    for (auto &d : dependants)
        if (d == dep.master)
            return (dependency.size() - 1);
    if (dep.master)
        dep.master->dependants.push_back(this);
    return (dependency.size() - 1);
}

Dependency &MenuBase::modifyDependency(int id)
{
    isResized = true;
    return dependency[id];
}

void MenuBase::resized()
{
    unsigned char restriction = 0;
    for (auto &d : dependency) {
        float tmp = d.relX;
        switch (d.alignFrom & (Align::LEFT | Align::RIGHT)) {
            case 0:
                break;
            case Align::LEFT:
                tmp += d.master->x1;
                break;
            case Align::RIGHT:
                tmp += d.master->x2;
                break;
            case (Align::LEFT | Align::RIGHT):
                tmp += (d.master->x1 + d.master->x2) / 2;
                break;
        };
        switch (d.alignTo & (Align::LEFT | Align::RIGHT)) {
            case 0:
                break;
            case Align::LEFT:
                if (d.master != this)
                    restriction |= Align::LEFT;
                x1 = tmp;
                break;
            case Align::RIGHT:
                if (d.master != this)
                    restriction |= Align::RIGHT;
                x2 = tmp;
                break;
            case (Align::LEFT | Align::RIGHT):
                switch (restriction & (Align::LEFT | Align::RIGHT)) {
                    case Align::LEFT:
                        x2 = tmp * 2 - x1;
                        break;
                    case Align::RIGHT:
                        x1 = tmp * 2 - x2;
                        break;
                    default:
                        tmp -= (x1 + x2) / 2;
                        x1 += tmp;
                        x2 += tmp;
                }
        };
        tmp = d.relY;
        switch (d.alignFrom & (Align::UP | Align::DOWN)) {
            case 0:
                break;
            case Align::UP:
                tmp += d.master->y1;
                break;
            case Align::DOWN:
                tmp += d.master->y2;
                break;
            case (Align::UP | Align::DOWN):
                tmp += (d.master->y1 + d.master->y2) / 2;
                break;
        };
        switch (d.alignTo & (Align::UP | Align::DOWN)) {
            case 0:
                break;
            case Align::UP:
                if (d.master != this)
                    restriction |= Align::UP;
                y1 = tmp;
                break;
            case Align::DOWN:
                if (d.master != this)
                    restriction |= Align::DOWN;
                y2 = tmp;
                break;
            case (Align::UP | Align::DOWN):
                switch (restriction & (Align::UP | Align::DOWN)) {
                    case Align::UP:
                        y2 = tmp * 2 - y1;
                        break;
                    case Align::DOWN:
                        y1 = tmp * 2 - y2;
                        break;
                    default:
                        tmp -= (y1 + y2) / 2;
                        y1 += tmp;
                        y2 += tmp;
                }
        };
        if (ratio == 0)
            continue;
        if (ratio > 0) {
            // width depend to height
            const float size = (y2 - y1) * ratio;// * height / width;
            switch (restriction & (Align::LEFT | Align::RIGHT)) {
                case Align::LEFT:
                    x2 = x1 + size;
                    break;
                case Align::RIGHT:
                    x1 = x2 - size;
                    break;
                default:
                    x1 = (x1 + x2 - size) / 2;
                    x2 = x1 + size;
            }
        } else {
            // height depend to width
            const float size = (x2 - x1) * -ratio;// * width / height;
            switch (restriction & (Align::UP | Align::DOWN)) {
                case Align::UP:
                    y2 = y1 + size;
                    break;
                case Align::DOWN:
                    y1 = y2 - size;
                    break;
                default:
                    y1 = (y1 + y2 - size) / 2;
                    y2 = y1 + size;
            }
        }
    }
    dest.x = x1 * master.width;
    dest.y = y1 * master.height;
    dest.w = (x2 - x1) * master.width;
    dest.h = (y2 - y1) * master.height;
    isResized = false;
    for (auto &d : dependants)
        d->resized();
}

bool MenuBase::update()
{
    if (isResized) {
        resized();
        if (hoverID)
            hoverMask = master.getHoverMask(hoverID, x1, y1, x2, y2);
        onUpdate();
        return true;
    } else {
        if (hoverID)
            hoverMask = master.getHoverMask(hoverID, x1, y1, x2, y2);
        return onUpdate();
    }
}

void MenuBase::setRatio(float _ratio)
{
    ratio = _ratio;
    isResized = true;
}
