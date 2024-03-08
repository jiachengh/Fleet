#ifndef ART_RUNTIME_NIEL_INSTRUMENTATION_H_
#define ART_RUNTIME_NIEL_INSTRUMENTATION_H_

#include "base/mutex.h"
#include "base/globals.h"

#define NIEL_INSTRUMENTATION_ENABLED true
#define NIEL_ALLOCATOR_INST_ENABLED false

/*
 * The point of the preprocessor directives below is to enable or
 * disable calls to the instrumentation functions based on the values of the macros
 * above. The INSTRUMENTATION_ENABLED macro disables all instrumentation calls
 * when set to false. The ALLOCATOR_INST_ENABLED macro disables calls to only
 * the allocator instrumentation functions when set to false.
 *
 * The second macro exists because the allocator instrumentation functions are
 * implemented in a dubious way (grabbing a lock defined by me on each
 * invocation) and also appear to produce incorrect data, so I wanted to turn
 * them off, but I also wanted to leave the function calls themselves in place
 * in case I'm able to fix the correctness issues down the line or want to be
 * able to easily identify places where allocation or freeing happens.
 */

#if NIEL_INSTRUMENTATION_ENABLED
#define NIEL_INST_START_ACCESS_COUNT(...) niel::inst::StartAccessCount(__VA_ARGS__)
#define NIEL_INST_COUNT_ACCESS(...) niel::inst::CountAccess(__VA_ARGS__)
#define NIEL_INST_FINISH_ACCESS_COUNT(...) niel::inst::FinishAccessCount(__VA_ARGS__)
#else // NIEL_INSTRUMENTATION_ENABLED
#define NIEL_INST_START_ACCESS_COUNT(...)
#define NIEL_INST_COUNT_ACCESS(...)
#define NIEL_INST_FINISH_ACCESS_COUNT(...)
#endif // NIEL_INSTRUMENTATION_ENABLED

#if NIEL_INSTRUMENTATION_ENABLED && NIEL_ALLOCATOR_INST_ENABLED
#define NIEL_INST_RECORD_ROSALLOC_ALLOC(...) niel::inst::RecordRosAllocAlloc(__VA_ARGS__)
#define NIEL_INST_RECORD_ROSALLOC_FREE(...) niel::inst::RecordRosAllocFree(__VA_ARGS__)
#define NIEL_INST_RECORD_ALLOC(...) niel::inst::RecordAlloc(__VA_ARGS__)
#define NIEL_INST_RECORD_FREE(...) niel::inst::RecordFree(__VA_ARGS__)
#else // NIEL_INSTRUMENTATION_ENABLED && NIEL_ALLOCATOR_INST_ENABLED
#define NIEL_INST_RECORD_ROSALLOC_ALLOC(...)
#define NIEL_INST_RECORD_ROSALLOC_FREE(...)
#define NIEL_INST_RECORD_ALLOC(...)
#define NIEL_INST_RECORD_FREE(...)
#endif // NIEL_INSTRUMENTATION_ENABLED && NIEL_ALLOCATOR_INST_ENABLED


namespace art {

namespace gc {
    class Heap;
    namespace collector {
        class GarbageCollector;
    }
    namespace space {
        class Space;
    }
}
namespace mirror {
    class Object;
}
class Thread;

namespace niel {

namespace inst {

enum RosAllocAllocType {
    ROSALLOC_ALLOC_THREAD_LOCAL, ROSALLOC_ALLOC_NORMAL, ROSALLOC_ALLOC_LARGE
};

enum RosAllocFreeType {
    ROSALLOC_FREE_NORMAL_OR_THREAD_LOCAL, ROSALLOC_FREE_LARGE
};

void RecordRosAllocAlloc(Thread * self, size_t size, RosAllocAllocType type);
void RecordRosAllocFree(Thread * self, size_t size, RosAllocFreeType type);

void RecordAlloc(Thread * self, gc::space::Space * space, size_t size);
void RecordFree(Thread * self, gc::space::Space * space, size_t size, int count);

void StartAccessCount(gc::collector::GarbageCollector * gc);
void CountAccess(gc::collector::GarbageCollector * gc, mirror::Object * object)
    REQUIRES_SHARED(Locks::mutator_lock_);
void FinishAccessCount(gc::collector::GarbageCollector * gc);

} // namespace inst
} // namespace niel
} // namespace art

#endif