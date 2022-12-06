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

    void operator=(bool b) {
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

    bool finalState() const {
        return state;
    }

    void setNoDelay(bool b) {
        if (this->mgr) {
            this->mgr->stopTicking(this);
            this->mgr = nullptr;
        }
        state = b;
        value = b ? interpolator.one() : interpolator.zero();
    }

    float getInterstate() const {
        return value;
    }

    operator const float&() const {
        return value;
    }

    float getDuration() const {
        return duration;
    }

    void setDuration(float duration_) {
        duration = duration_;
        if (state && !this->mgr)
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
