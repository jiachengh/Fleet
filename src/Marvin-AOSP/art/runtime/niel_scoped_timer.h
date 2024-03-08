#ifndef ART_RUNTIME_NIEL_SCOPED_TIMER_H_
#define ART_RUNTIME_NIEL_SCOPED_TIMER_H_

#include "base/logging.h"

namespace art {

namespace niel {

class ScopedTimer {
  public:
    ScopedTimer(const std::string & name) : timerName(name) {
        LOG(INFO) << "NIEL starting " << timerName;
        startTime = NanoTime();
    }

    ~ScopedTimer() {
        uint64_t duration = NanoTime() - startTime;
        LOG(INFO) << "NIEL ending " << timerName << ": took " << PrettyDuration(duration);
    }

  private:
    uint64_t startTime;
    std::string timerName;
};

} // namespace niel
} // namespace art

#endif // ART_RUNTIME_NIEL_SCOPED_TIMER_H_