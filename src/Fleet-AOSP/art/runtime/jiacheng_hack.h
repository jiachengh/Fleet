// jiacheng start

#ifndef ART_RUNTIME_JIACHENG_HACK_H_
#define ART_RUNTIME_JIACHENG_HACK_H_

#include "jiacheng_global.h"
#include "process_state.h"
#include "signal.h"

namespace art{

namespace gc {
namespace collector {
class GarbageCollector;
} // namespace collector
} // namespace gc

namespace mirror {
class Object;
} // namespace mirror

namespace jiacheng {

bool HandleFault(int sig, siginfo_t* info, void* context);

void OnAppStart();

} // namespace jiacheng
} // namespace art
#endif

// jiacheng end