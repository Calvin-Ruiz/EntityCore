#ifndef AFADER_HPP_
#define AFADER_HPP_

#include "Tickable.hpp"

//! Automatic fader for use with a global tick manager
template <class GlobalTickMgr, class Interp>
class AFader : public Tickable<GlobalTickMgr> {
public:
    AFader(bool state, int duration) :
        context(context), duration(duration)
    {
        setNoDelay(state);
        timer = state ? duration : 0;
    }

    void operator=(bool b) {
        if (state != b) {
            state = b;
            needUpdate(GlobalTickMgr::instance);
        }
    }

    virtual bool update(float deltaTime) override {
        if (state) {
            if ((timer += deltaTime) >= duration) {
                timer = duration;
                value = interpolator.one();
                mgr = nullptr;
                return false;
            }
        } else {
            if ((timer -= deltaTime) <= 0) {
                timer = 0;
                value = interpolator.zero();
                mgr = nullptr;
                return false;
            }
        }
        value = interpolator(timer / duration);
        return true;
    }

    void setNoDelay(bool b) {
        if (mgr) {
            mgr->stopTicking(this);
            mgr = nullptr;
        }
        state = b;
        value = b ? interpolator.one() : interplator.zero();
    }

    float getInterstate() const {
        return value;
    }

    operator float() const {
        return value;
    }

    void getDuration() const {
        return duration;
    }

    void setDuration(float duration_) {
        duration = duration_;
        if (state && !mgr)
            timer = duration_;
    }

    //! Return true if current value is interpolator.zero()
    bool isZero() const {
        return timer == 0;
    }

    //! Return true if current value is not interpolator.zero()
    bool isNonZero() const {
        return timer > 0;
    }

    bool isTransiting() const {
        return mgr != nullptr;
    }

    Interp interpolator;
private:
    float value; // Current value
    float timer; // Current timer state
    float duration; // Transition duration
    bool state; // Target state
};

#endif /* end of include guard: AFADER_HPP_ */
