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
    //! This method is called every tic as long as it return true
    virtual bool update(float deltaTime) = 0;
protected:
    //! Inform about a new ticking dependency
    inline void needUpdate(TickMgr *_mgr) {
        if (!mgr) {
            mgr = _mgr;
            _mgr->startTicking(this);
        }
    }
    //! This member MUST be set to nullptr before returning false in update()
    TickMgr *mgr = nullptr;
};

#endif /* end of include guard: TICKABLE_HPP_ */
