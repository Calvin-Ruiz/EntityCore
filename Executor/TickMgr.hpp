#ifndef TICK_MGR_HPP_
#define TICK_MGR_HPP_

#include "Tickable.hpp"
#include <list>
//! Simple synchronous ticking engine
class TickMgr {
public:
    void update(float deltaTime) {
        updateList.remove_if([deltaTime](auto *obj){return obj->update(deltaTime);});
    }

    inline void startTicking(Tickable<TickMgr> *arg) {
        updateList.push_back(arg);
    }

    inline void stopTicking(Tickable<TickMgr> *arg) {
        updateList.remove(arg);
    }
private:
    std::list<Tickable<TickMgr> *> updateList;
};

#endif /* end of include guard: TICK_MGR_HPP_ */
