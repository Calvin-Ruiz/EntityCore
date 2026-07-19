#ifndef TICKABLE_HPP_
#define TICKABLE_HPP_

template <class TickMgr>
class Tickable {
public:
    Tickable() = default;
    virtual ~Tickable() {
        if (mgr)
            mgr->stopTicking(this);
    }
    //! This method is called every tick as long as it return false
    virtual bool update(float deltaTime) = 0;
    //! Sever the manager back-reference. The tick manager calls this from its
    //! own destructor when it is going away, so a Tickable that outlives its
    //! manager (e.g. quitting while a transition is still active) will not call
    //! stopTicking() on freed manager memory from ~Tickable. `mgr` stays private
    //! to Tickable: the manager drives this state transition through this method,
    //! it never reaches into Tickable's internals.
    void detach() noexcept { mgr = nullptr; }
protected:
    //! Inform about a new ticking dependency
    inline void needUpdate(TickMgr *_mgr) {
        if (!mgr) {
            mgr = _mgr;
            _mgr->startTicking(this);
        }
    }
    //! This member MUST be set to nullptr before returning true in update()
    TickMgr *mgr = nullptr;
};

#endif /* end of include guard: TICKABLE_HPP_ */
