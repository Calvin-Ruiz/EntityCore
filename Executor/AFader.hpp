#ifndef AFADER_HPP_
#define AFADER_HPP_

#include "Tickable.hpp"

//! Automatic fader for use with a global tick manager
template <class GlobalTickMgr, class Interp>
class AFader : public Tickable<GlobalTickMgr> {
public:
    AFader(bool state = false, float duration = 2) : duration(duration)
    {
        setNoDelay(state);
        timer = state ? duration : 0;
    }

    inline void operator=(bool b) {
        if (state != b) {
            state = b;
            this->needUpdate(GlobalTickMgr::instance);
        }
    }

    virtual bool update(float deltaTime) override {
        if (state) {
            if ((timer += deltaTime) >= duration) {
                timer = duration;
                value = interpolator.one();
                this->mgr = nullptr;
                return true;
            }
        } else {
            if ((timer -= deltaTime) <= 0) {
                timer = 0;
                value = interpolator.zero();
                this->mgr = nullptr;
                return true;
            }
        }
        value = interpolator(timer / duration);
        return false;
    }

    inline bool finalState() const {
        return state;
    }

    inline void setNoDelay(bool b) {
        if (this->mgr) {
            this->mgr->stopTicking(this);
            this->mgr = nullptr;
        }
        state = b;
        timer = b ? duration : 0;
        value = b ? interpolator.one() : interpolator.zero();
    }

    inline float getInterstate() const {
        return value;
    }

    inline operator const float&() const {
        return value;
    }

    inline float getDuration() const {
        return duration;
    }

    inline void setDuration(float duration_) {
        duration = duration_;
        if (state && !this->mgr)
            timer = duration_;
    }

    //! Return true if current value is interpolator.zero()
    inline bool isZero() const {
        return timer == 0;
    }

    //! Return true if current value is not interpolator.zero()
    inline bool isNonZero() const {
        return timer > 0;
    }

    inline bool isTransiting() const {
        return this->mgr != nullptr;
    }

    Interp interpolator;
private:
    float value; // Current value
    float timer; // Current timer state
    float duration; // Transition duration
    bool state; // Target state
};

#endif /* end of include guard: AFADER_HPP_ */
