#ifndef ART_RUNTIME_NIEL_COMMON_H_
#define ART_RUNTIME_NIEL_COMMON_H_

/*
 * Common utility methods shared across niel_swap and niel_instrumentation.
 */

#include <fstream>
#include <string>

#include "base/mutex.h"
#include "gc/heap-inl.h"
#include "mirror/object.h"
#include "runtime.h"

namespace art {

namespace niel {

void openFile(const std::string & path, std::fstream & stream);

void openFileAppend(const std::string & path, std::fstream & stream);

bool checkStreamError(const std::ios & stream, const std::string & msg);

std::string getPackageName();

bool appOnCommonBlacklist(const std::string & packageName);

// jiacheng start
bool appOnCommonWhitelist();
// jiacheng end

/*
 * Returns true if the object is cold enough to be swapped based on the given
 * data.
 */
inline bool objectIsCold(uint8_t writeShiftRegVal, bool wasWritten) {
    // jiacheng start
    // return writeShiftRegVal < 2 && !wasWritten;

    (void)writeShiftRegVal;
    (void)wasWritten;
    return writeShiftRegVal < 2;
    // jiacheng end
}

inline bool objectIsLarge(size_t objectSize) {
    // jiacheng start
    // return objectSize >= 2 * 1024;

    return objectSize >= 64;

    // (void)objectSize;
    // return true;
    // jiacheng end
}

/*
 * Check if an object is of a type for which swapping is enabled.
 *
 * NOTE: This function performs a read on the object and does not set or clear
 * the object's IgnoreReadFlag; you should set and clear that flag if you want
 * to avoid counting the read.
 */
inline bool objectIsSwappableType(mirror::Object * obj)
        REQUIRES_SHARED(Locks::mutator_lock_) {
            // jiacheng start
            return !obj->IsClass() && !obj->IsClassLoader() && !obj->IsDexCache();
    // return (
    //            (obj->IsArrayInstance() && !obj->IsObjectArray())
    //         || (!obj->IsArrayInstance() && !obj->IsClass() && !obj->IsClassLoader()
    //              && !obj->IsDexCache() && !obj->IsString() && !obj->IsReferenceInstance())
    //         );
            // jiacheng end
}

inline gc::Heap * getHeapChecked() {
    Runtime * runtime = Runtime::Current();
    if (runtime == nullptr) {
        return nullptr;
    }
    return runtime->GetHeap();
}

} // namespace niel
} // namespace art

#endif // ART_RUNTIME_NIEL_COMMON_H_