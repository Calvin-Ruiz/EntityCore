#ifndef ASMOOTH_HPP_
#define ASMOOTH_HPP_

#include "Tickable.hpp"

// A value smoothly changing, using optimal acceleration and deceleration
// Size in memory in octect is (float: 40, double: 56, long double: 80)
template <class GlobalTickMgr, class T = float, float implicitDuration=0.5>
class ASmooth : public Tickable<GlobalTickMgr> {
public:
    ASmooth(T value) : c(value)
    {
    }

    inline void set(T dst, float minDuration) {
        if (this->mgr) {
            {
                const float tmp = duration + nextDuration;
                if (minDuration < tmp)
                    minDuration = tmp;
            }
            // Froze current movement
            c += timer * (b + duration * a);
            dst -= c; // Dst now represent the delta
            b += timer * a*2;
            // Calculate new coefficients to preserve both position and velocity
            const T tmp = sqrt(dst*dst + b*minDuration*(b * minDuration / 2 - dst));
            T accelerationTime = (dst - tmp)/b;
            if (accelerationTime < 0 || accelerationTime > minDuration)
                accelerationTime = (dst + tmp)/b;
            a = -b / (accelerationTime * 2 - minDuration * 4);
            timer = 0;
            duration = accelerationTime;
            nextDuration = minDuration - duration;
        } else if (minDuration) {
            duration = nextDuration = minDuration / 2;
            dst -= c; // Dst now represent the delta
            a = (dst/2) / (duration*duration);
            b = 0;
            this->needUpdate(GlobalTickMgr::instance);
        } else {
            c = dst;
        }
    }

    inline void operator=(T dst) {
        set(dst, implicitDuration);
    }

    inline bool isTransiting() const {
        return this->mgr != nullptr;
    }

    virtual bool update(float deltaTime) override {
        if ((timer += deltaTime) >= duration) {
            c += duration * (b + duration * a);
            if (nextDuration) { // Compose next movement
                b += duration * a*2;
                a = -a;
                duration = nextDuration;
                deltaTime = timer - duration;
                nextDuration = 0;
                timer = 0;
                return update(deltaTime);
            }
            this->mgr = nullptr;
            return true;
        }
        return false;
    }

    inline operator T() const {
        return (this->mgr) ? (((a * timer) + b * timer) + c) : c;
    }
private:
    T a, b, c; // Coefficients of the current equation
    float timer;
    float duration;
    float nextDuration;
};

#endif /* end of include guard: ASMOOTH_HPP_ */
