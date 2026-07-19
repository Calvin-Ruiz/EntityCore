#ifndef TICK_MGR_HPP_
#define TICK_MGR_HPP_

#include "Tickable.hpp"
#include <list>
//! Simple synchronous ticking engine. CRTP: a concrete manager gains the
//! registry, the tick and the teardown by deriving TickMgr<Self> - its
//! tickables are then Tickable<Self> and register through Self::instance.
template <class Derived>
class TickMgr {
public:
    void update(float deltaTime) {
        updateList.remove_if([deltaTime](auto *obj){return obj->update(deltaTime);});
    }

    inline void startTicking(Tickable<Derived> *arg) {
        updateList.push_back(arg);
    }

    inline void stopTicking(Tickable<Derived> *arg) {
        updateList.remove(arg);
    }

    //! On manager destruction, sever every still-registered Tickable so one
    //! that outlives its manager (e.g. quitting while a transition is active)
    //! will not call stopTicking() on freed manager memory from ~Tickable.
    ~TickMgr() {
        for (auto *t : updateList)
            t->detach();
    }
private:
    std::list<Tickable<Derived> *> updateList;
};

#endif /* end of include guard: TICK_MGR_HPP_ */
