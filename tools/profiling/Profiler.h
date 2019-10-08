#pragma once
#include <stdint.h>
#include <string>
#include <vector>
namespace MAI {
namespace Profiling {

class ProfileEvent {
public:
    uint64_t beginTime;
    uint64_t endTime;
    std::string name;
    std::string type;
};

class EventHub {
public:
    EventHub(bool enable = true)
        : mEnabled(enable) {
    }

    // return unqiue handle for this event
    int32_t beginEvent(const ProfileEvent& event);
    // handle refer to the specified event
    void endEvent(int32_t handle);
    inline void setEnable(bool enable) {
        mEnabled = enable;
    }

    inline void reset() {
        mEvents.clear();
    }

    std::vector<ProfileEvent>& getProfileEvents() {
        return mEvents;
    }

    uint64_t nowMicros();
private:
    bool mEnabled;
    std::vector<ProfileEvent> mEvents;
    static const int32_t kInvalidHandle = -1;
};

class ScopedOperatorProfiler;
class Profiler {
public:
    Profiler();
    virtual ~Profiler();

    void startProfiling();
    void stopProfiling();
    void reset();
    inline std::vector<ProfileEvent>& getProfileEvents() const {
        return mEventHub->getProfileEvents();
    }
private:
    friend class ScopedOperatorProfiler;
    EventHub* mEventHub;
};

class ScopedOperatorProfiler {
public:
    ScopedOperatorProfiler(Profiler* profiler, const std::string& opName, const std::string& opType)
        : mEventHub(NULL), mEventHandle(-1) {
        if (profiler) {
            mEventHub = profiler->mEventHub;
            ProfileEvent event;
            event.name = opName;
            event.type = opType;
            mEventHandle = mEventHub->beginEvent(event);
        }
    }

    ~ScopedOperatorProfiler() {
        if (mEventHub) {
            mEventHub->endEvent(mEventHandle);
        }
    }

private:
    EventHub* mEventHub;
    int32_t mEventHandle;
};

#define NAME_UNIQ(name, ctr) name##ctr

#define SCOPED_OPERATOR_PROFILE(profiler, name, type) \
    MAI::Profiling::ScopedOperatorProfiler \
        NAME_UNIQ(_profile_, __COUNTER__)((profiler), (name), (type))

} // namespace Profiling
} // namespace MAI