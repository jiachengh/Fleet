// jiacheng start
#include "jiacheng_global.h"
#include "jiacheng_barrier.h"
#include "jiacheng_utils.h"

#include "mirror/object-inl.h"
#include "runtime.h"
#include "gc/heap.h"

namespace art {
namespace jiacheng {


void JiachengBarrier(uint64_t obj) {
    if (!ENABLE_ACCESS_BARRIER) return;
    if (!obj) {
        return;
    }
    Runtime* runtime = Runtime::Current();
    if (!runtime->IsStarted()) { 
        return;
    }
    if (!IsWhiteApp()) return;

    mirror::Object* object = reinterpret_cast<mirror::Object*>(obj);
    gc::Heap* heap = runtime->GetHeap();
    heap->AddWs(object);
}

void AllocationNewBarrier(uint64_t obj) {
    if (!ENABLE_ALLOCATION_BARRIER) return;
    
    #ifdef JIACHENG_DEBUG
    mirror::Object* object = reinterpret_cast<mirror::Object*>(obj);
    object->SetTargetFlag();
    object->SetDebugFlag(jiacheng::GenerateID());
    #endif

    JiachengBarrier(obj);
}

}
}

// jiacheng end