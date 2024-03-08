// jiacheng start
#include <string>
#include <chrono>
#include <thread>

#include "runtime.h"
#include "gc/heap.h"
#include "gc/space/region_space.h"
#include "gc/space/dlmalloc_space.h"
#include "gc/space/large_object_space.h"
#include "gc/collector/garbage_collector.h"
#include "base/utils.h"
#include "mirror/object-inl.h"

#include <sys/mman.h>

#include "jiacheng_hack.h"
#include "jiacheng_utils.h"

namespace art{
namespace jiacheng {

typedef void(*WalkCallback)(void *start, void *end, size_t num_bytes, void* callback_arg);

bool HandleFault(int sig ATTRIBUTE_UNUSED, siginfo_t* info, void* context ATTRIBUTE_UNUSED) {
    if (!IsWhiteApp()) {
        return false;
    }
    mirror::Object *ref = reinterpret_cast<mirror::Object *>(info->si_addr);
    bool result = false;
    
    gc::Heap* heap = Runtime::Current()->GetHeap();
    gc::space::RegionSpace* region_space = heap->GetRegionSpace();
    gc::space::DlMallocSpace* non_moving_space = reinterpret_cast<gc::space::DlMallocSpace*>(heap->GetNonMovingSpace());
    gc::space::LargeObjectSpace* large_object_space = heap->GetLargeObjectsSpace();

    if (region_space->Contains(ref)) {
        result = region_space->HandleFault(ref);
    } else if (non_moving_space->Contains(ref)) {
        result = non_moving_space->HandleFault(ref);
    } else if (large_object_space->Contains(ref)) {
        result = large_object_space->HandleFault(ref);
    }
    return result;
}

void OnAppStart() {
    if (!IsWhiteApp()) {
        return;
    }
    LOG(INFO) << "jiacheng OnAppStart()";

    auto func = [](){
        constexpr uint64_t defer_time = 3;
        gc::Heap* heap = Runtime::Current()->GetHeap();
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(defer_time));
            LOG(INFO) << "jiacheng"
                    << " MutatorWS= " << heap->GetMutatorWsSize()
                    << " GcWS= " << heap->GetGcWsSize();
            heap->ClearMutatorWs();
            heap->ClearGcWs();
        }
    };
    std::thread t(func);
    t.detach();
}

} // namespace jiacheng
} // namespace art

// jiacheng end