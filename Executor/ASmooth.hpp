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
            // p(timer) = c + timer*(b + timer*a) - was timer*(b + duration*a),
            // exact only at timer==0 or timer==duration, wrong mid-phase
            // (corrupted the frozen position a retarget then plans from).
            // Analysis + replay: INTENT.md 11.18 / harness/asmooth_sim.py.
            c += timer * (b + timer * a);
            dst -= c; // Dst now represent the delta
            b += timer * a*2;
            if (b == 0) {
                // Zero current velocity (e.g. retarget within the same tick,
                // timer==0 in the acceleration phase): the general formulas
                // below divide by b -> 0/0 = NaN, permanently poisoning the
                // value (observed: double setScaling at init -> Moon
                // scaledRadius NaN -> frozen main loop once the Moon became
                // the camera reference). Same math as a fresh movement from
                // the frozen position.
                timer = 0;
                duration = nextDuration = minDuration / 2;
                a = (dst/2) / (duration*duration);
                return;
            }
            // Calculate new coefficients to preserve both position and velocity
            const T tmp = sqrt(dst*dst + b*minDuration*(b * minDuration / 2 - dst));
            T accelerationTime = (dst - tmp)/b;
            if (accelerationTime < 0 || accelerationTime > minDuration)
                accelerationTime = (dst + tmp)/b;
            // Derivation forces a = -b/(4*t1 - 2*T); the 2/4 swap made
            // mid-transit retargets land off-target with residual velocity
            // (replayed worst error 42 units -> 2.5e-7 with this form).
            a = -b / (accelerationTime * 4 - minDuration * 2);
            timer = 0;
            duration = accelerationTime;
            nextDuration = minDuration - duration;
        } else if (minDuration) {
            // A FRESH movement starts at phase time 0 - forced by the same
            // p(t) = c + t*(b + t*a) the coefficients below are solved for,
            // whose "now" is t==0. This branch is the ONLY writer that leaves
            // the object ticking, so anything it does not write is what every
            // later read and every later retarget starts from: without this
            // line `timer` is (i) INDETERMINATE on the first movement - the
            // constructor never wrote it - and (ii) STALE (>= duration) on
            // every movement after a completed one, since update() leaves the
            // finished phase's timer in place when it clears mgr.
            // Measured on the shipped config (INTENT 11.152 / 5.102): the
            // Moon's scaling read -nan immediately after its first setScaling,
            // and the following retarget turned the whole state to NaN through
            // `c += timer*(b + timer*a)`, so scaledRadius/scaledDatumRadius/
            // scaledGroundRadius/boundingRadius were NaN for the whole session.
            // That also defeated 11.16's b==0 guard: `b += timer*a*2` is NaN,
            // never 0, so the analytic-limit branch could not fire.
            timer = 0;
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
                // Carry-over must use the FINISHED phase's duration - it was
                // computed after the overwrite (off by d1-d2 wall-time after
                // a retarget; zero for fresh movements where d1==d2).
                deltaTime = timer - duration;
                duration = nextDuration;
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
        // p(t) = (a*t + b)*t + c - the previous parenthesization
        // ((a*t) + b*t) + c collapsed the quadratic term to linear
        // (display-only: committed state was unaffected, exact at phase ends).
        return (this->mgr) ? (((a * timer) + b) * timer + c) : c;
    }
private:
    // Every member is defined at construction: an ASmooth is a VALUE, and a
    // value has no indeterminate state. `c` is the one the constructor sets
    // (it IS the value); the rest describe "no movement in progress", which is
    // what mgr == nullptr already says - they simply must not contradict it.
    // The class-wide initializer is the defect's class fix; `set`'s own
    // `timer = 0` above is the per-movement invariant (a second movement
    // starts fresh too, and no constructor runs in between).
    T a {}, b {}, c; // Coefficients of the current equation
    float timer = 0;
    float duration = 0;
    float nextDuration = 0;
};

#endif /* end of include guard: ASMOOTH_HPP_ */
