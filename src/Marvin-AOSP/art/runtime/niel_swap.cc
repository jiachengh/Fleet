#include "niel_swap.h"

#include "gc/collector/garbage_collector.h"
#include "gc/heap.h"
#include "gc/space/space-inl.h"
#include "gc/task_processor.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "niel_common.h"
#include "niel_reclamation_table.h"
#include "niel_scoped_timer.h"
#include "niel_stub-inl.h"
#include "runtime.h"
#include "thread_list.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <vector>

// jiacheng start
#include "mirror/reference-inl.h"
#include "gc/heap-visit-objects-inl.h"
// jiacheng end

namespace art {

namespace niel {

namespace swap {

// Commercial app compatibility mode excludes all objects from being swapped
// and makes the swapped-in space smaller (to avoid colliding with other mmap'd
// regions). This mode is used to evaluate the overhead of Marvin without the
// inexplicable crashes that occur when objects are moved/reclaimed in
// commercial apps.
// jiacheng start
// #define COMMERCIAL_APP_COMPAT_MODE false
#define COMMERCIAL_APP_COMPAT_MODE false
// jiacheng end

// When this flag is set to true, the runtime will swap in all swapped-out
// objects when the app moves to the foreground.
// jiacheng start
#define SWAP_IN_ON_FOREGROUND true
// #define SWAP_IN_ON_FOREGROUND false
// jiacheng end

/* Constants */
const double COMPACT_THRESHOLD = 0.25;
const int BG_MARK_SWEEPS_BEFORE_SEMI_SPACE = 6;
const uint64_t WRITE_TASK_FG_MAX_DURATION = 1000000000; // ns
const uint64_t WRITE_TASK_BG_MAX_DURATION = 1000000000; // ns
const uint64_t WRITE_TASK_FG_WAIT_TIME = 5000000000; // ns
const uint64_t WRITE_TASK_BG_WAIT_TIME = 30000000000; // ns
const uint64_t WRITE_TASK_STARTUP_DELAY = 6000000000; // ns
const uint64_t REC_TABLE_NUM_ENTRIES = 60000;
// jiacheng start
// const uintptr_t SWAPPED_IN_SPACE_START = 0xc0000000;
uintptr_t SWAPPED_IN_SPACE_START = 0;
// jiacheng end
#if COMMERCIAL_APP_COMPAT_MODE
const uint64_t SWAPPED_IN_SPACE_SIZE = 1 * 1024 * 1024; // bytes
#else
const uint64_t SWAPPED_IN_SPACE_SIZE = 512 * 1024 * 1024; // bytes
#endif

// Return values of writeToSwapFile().
const int SWAPFILE_WRITE_OK = 0;
const int SWAPFILE_WRITE_GARBAGE = 1;
const int SWAPFILE_WRITE_NULL_CLASS = 2;
const int SWAPFILE_WRITE_RESIZED = 3;
const int SWAPFILE_WRITE_NO_SWAP = 4;

/* Internal functions */
void scheduleNextTask(Thread * self, bool ioError);
gc::TaskProcessor * getTaskProcessorChecked();
void FreeFromRosAllocSpace(Thread * self, gc::Heap * heap, mirror::Object * obj)
        REQUIRES_SHARED(Locks::mutator_lock_);
void FreeFromLargeObjectSpace(Thread * self, gc::Heap * heap, mirror::Object * obj)
        REQUIRES_SHARED(Locks::mutator_lock_);
void debugPrintDataStructureInfo(Thread * self, const std::string & message);
void dumpObject(mirror::Object * obj);

/*
 * Writes a snapshot of the given object to the swap file, overwriting the object's
 * previous snapshot in the swap file if one exists. Clears the object's dirty bit.
 * Grabs the swapFileMutex and swapFileMapsMutex during execution.
 */
int writeToSwapFile(Thread * self, gc::Heap * heap, mirror::Object * object, bool * ioError)
        REQUIRES_SHARED(Locks::mutator_lock_);

/*
 * Allocates space for the object in memory and copies the object from the swap
 * file into memory.
 *
 * NOTE: The caller of this method is responsible for locking and unlocking the
 * stub's RTE.
 */
mirror::Object * swapInObject(Thread * self, Stub * stub, std::streampos objOffset,
                              size_t objSize)
        REQUIRES_SHARED(Locks::mutator_lock_);

/*
 * Updates swap data structures based on a mapping from "old pointers" to "new
 * pointers" so that any old pointers in the data structures are replaced with
 * the corresponding new pointers.
 *
 * More precisely, for each key-value pair (a,b) in addrTable, this function
 * replaces any key-value pair (a,c) in objectOffsetMap or objectSizeMap with
 * the new entry (b,c).
 *
 * If removeUntouchedRosAlloc is true, this function removes all entries in the
 * data structures corresponding to pointers that are not in the given mapping
 * and not in the large object space. The purpose of this parameter is to allow
 * this function to clean up entries corresponding to objects freed by the
 * SemiSpace garbage collector.
 *
 * More precisely, given that object a is not in the large object space, this
 * function:
 *
 * 1) Removes all pairs (a,c) from objectOffsetMap and objectSizeMap that did
 * not have a corresponding pair (a,b) in addrTable.
 *
 * 2) If object a is a stub and there is no pair (a,b) in addrTable, frees the
 * object associated with the stub.
 */
void replaceDataStructurePointers(Thread * self, const std::map<void *, void *> & addrTable,
                                  bool removeUntouchedRosAlloc);

/*
 * Copies a file. Returns true on success and false on error. The caller is
 * responsible for making sure that all open file descriptors to both the
 * source and destination files are closed.
 */
bool copyFile(const std::string & fromFileName, const std::string & toFileName);

/*
 * Checks to make sure that the swap file contains every object at the correct
 * position. This function exclusively holds the swapFileMutex for a long time!
 * Use it only in debug builds.
 */
void validateSwapFile(Thread * self);

/*
 * Walk all of the memory spaces and replace any references to swapped-out
 * objects with references to their corresponding stubs.
 */
void patchStubReferences(Thread * self, gc::Heap * heap) REQUIRES(Locks::mutator_lock_);

/*
 * Perform the checks that the kernel would need to perform when reclaiming an
 * object. If the kernel lock bit is set on the RTE and this method returns
 * true, then there is a valid object associated with this RTE, and it is safe
 * to reclaim it.
 */
bool doKernelReclamationChecks(TableEntry * rte);

/*
 * Determines whether an object and the objects referenced by it should be
 * excluded from being swapped out. The caller is responsible for setting and
 * clearing the object's IgnoreReadFlag before and after calling this function.
 *
 * Note: currently, this method is used to exclude all objects from being
 * swapped when running in "commercial app compatibility mode." Otherwise, it
 * does not exclude any objects.
 */
bool shouldExcludeObjectAndMembers(mirror::Object * obj) REQUIRES_SHARED(Locks::mutator_lock_);

/* Variables */
Mutex writeQueueMutex("WriteQueueMutex", kLoggingLock);

ReaderWriterMutex swapFileMapsMutex("SwapFileMapsMutex", kDefaultMutexLevel);
// Holding the swapFileMutex while reading the swap file always guarantees that
// reads return valid data, and it guarantees that reads reflect the latest
// writes iff the WriteTask always runs on the same thread as swap file
// compaction (as it does for marlin builds running on a Pixel XL).
Mutex swapFileMutex("SwapFileMutex", kLoggingLock);

/*
 * Not locked but assumed to be only touched in one thread (because all Tasks,
 * including GC, run on the same fixed thread)
 */
uint32_t pid = 0;
std::map<void *, void *> remappingTable;
std::map<void *, void *> semiSpaceRemappingTable; //TODO: combine with normal remapping table?
// Holds the set of "swap candidate" objects that do not already have stubs.
// On the next call to CreateStubs(), stubs will be created for these objects.
std::set<mirror::Object *> createStubSet;
// Used during semi-space GC to track which stubs are associated with which
// RTEs. If a stub was freed by the semi-space GC instead of being
// forwarded, this map is used to free its corresponding RTE/object in
// replaceDataStructurePointers(). At the beginning of a semi-space GC run,
// this map should contain every live stub as a key, and at the end of the run,
// this map should be empty.
std::map<Stub *, TableEntry *> stubRTEMap;
bool creatingStubs = false;
bool doingSwapInCleanup = false;
int bgMarkSweepCounter = 0;
int freedObjects = 0;
long freedSize = 0;
int swapfileObjects = 0;
int swapfileSize = 0;

// jiacheng start
std::atomic<bool> duringGC(false);
// jiacheng end

/*
 * Not locked but only modified in Heap::UpdateProcessState() and read in Tasks
 */
bool appInForeground = true;

/*
 * Not locked but will only ever switch from false to true, and it shouldn't
 * cause any correctness issues if an app thread incorrectly reads it as false
 * for a little while after it has changed
 */
bool swapEnabled = false;

/*
 * Not locked, but should only be touched by a thread holding the mutator_lock_
 *
 * TODO: make locked by mutator_lock_
 */
gc::space::LargeObjectSpace * swappedInSpace = nullptr;

/*
 * Not locked. This object's CreateEntry() method should only be called by the
 * heap task thread.
 */
ReclamationTable recTable;

/*
 * Locked by swapFileMutex
 */
std::fstream swapfile;

/*
 * Locked by swapFileMapsMutex
 */
// These maps describe the position and size of checkpointed objects in the
// swap file. If a stub has been created for an object, the key associated with
// the object's information is the stub pointer. If no stub has been created
// yet, the key is a pointer to the object itself.
std::map<void *, std::streampos> objectOffsetMap;
std::map<void *, size_t> objectSizeMap;

/*
 * Locked by writeQueueMutex
 */
// These data structures are used to track which objects need to be
// checkpointed to the swap file. They can hold either stub pointers (if an
// object has a corresponding stub) or pointers to real objects (if a stub has
// not yet been created for the object).
std::vector<mirror::Object *> writeQueue;
std::set<mirror::Object *> writeSet; // prevents duplicate entries in writeQueue

class WriteTask : public gc::HeapTask {
  public:
    WriteTask(uint64_t target_time) : gc::HeapTask(target_time) { }

    virtual void Run(Thread * self) {
        CHECK(swapEnabled);

        bool done = false;
        uint64_t startTime = NanoTime();
        bool ioError = false;

        int numGarbageObjects = 0;
        int numNullClasses = 0;
        int numResizedObjects = 0;

        gc::Heap * heap = getHeapChecked(); 

        int queueSize = 0;

        Locks::mutator_lock_->ReaderLock(self);
        // jiacheng start
        // while (!done) {
        while (!done && !IfDuringGC()) {
        // jiacheng end
            mirror::Object * object = nullptr;

            writeQueueMutex.ExclusiveLock(self);
            queueSize = writeQueue.size();
            if (queueSize == 0) {
                done = true;
            }
            else {
                object = writeQueue.front();
                writeQueue.erase(writeQueue.begin());
                writeSet.erase(object);
            }
            writeQueueMutex.ExclusiveUnlock(self);

            if (object != nullptr) {
                int writeResult = writeToSwapFile(self, heap, object, &ioError);
                if (writeResult == SWAPFILE_WRITE_GARBAGE) {
                    numGarbageObjects++;
                }
                if (writeResult == SWAPFILE_WRITE_NULL_CLASS) {
                    numNullClasses++;
                }

                if (writeResult == SWAPFILE_WRITE_RESIZED) {
                    numResizedObjects++;
                }
            }

            if (ioError) {
                done = true;
            }

            uint64_t currentTime = NanoTime();
            uint64_t maxDuration = appInForeground ? WRITE_TASK_FG_MAX_DURATION
                                                   : WRITE_TASK_BG_MAX_DURATION;
            if (currentTime - startTime > maxDuration) {
                done = true;
            }
        }
        Locks::mutator_lock_->ReaderUnlock(self);

        LOG(INFO) << "NIEL done writing objects in WriteTask; " << queueSize
                  << " objects still in queue; swap file has " << swapfileObjects
                  << " objects, size " << swapfileSize;
        if (numGarbageObjects > 0 || numNullClasses > 0 || numResizedObjects > 0) {
            LOG(ERROR) << "NIELERROR WriteTask irregularities: " << numGarbageObjects
                       << " garbage objects, " << numNullClasses << " null classes, "
                       << numResizedObjects << " resized objects";
        }

        if ((double)freedSize / swapfileSize > COMPACT_THRESHOLD) {
            CompactSwapFile(self);
            validateSwapFile(self);
        }

        scheduleNextTask(self, ioError);
    }
};

// jiacheng end
void GCStart() {
    LOG(INFO) << "jiacheng debug niel_swap.cc GCStart() 352"
              << " createStubSet.size()= " << createStubSet.size()
              ;
    duringGC.store(true);
}

void GCEnd() {
    duringGC.store(false);
    LOG(INFO) << "jiacheng debug niel_swap.cc GCEnd() 360"
              << " createStubSet.size()= " << createStubSet.size()
              ;
}

bool IfDuringGC() {
    return duringGC.load();
}

ReclamationTable * GetReclamationTable() {
    return &recTable;
}
// jiacheng end


bool objectInSwappableSpace(gc::Heap * heap, mirror::Object * obj) {
    // jiacheng start
    // return (   heap->GetRosAllocSpace()->Contains(obj)
    //         || heap->GetLargeObjectsSpace()->Contains(obj)
    //         || swappedInSpace->Contains(obj)
    //        );
    gc::space::RosAllocSpace* rosalloc_space = heap->GetRosAllocSpace();
    gc::space::LargeObjectSpace* large_object_space = heap->GetLargeObjectsSpace();
    gc::space::RegionSpace* region_space = heap->GetRegionSpace();
    return  (large_object_space && large_object_space->Contains(obj)) || 
            (region_space && region_space->Contains(obj)) ||
            (rosalloc_space && rosalloc_space->Contains(obj)) || 
            (swappedInSpace && swappedInSpace->Contains(obj));
    // jiacheng end
}

void SetInForeground(bool inForeground) {
    appInForeground = inForeground;
    if (inForeground) {
        bgMarkSweepCounter = 0;
    }
}

void scheduleNextTask(Thread * self, bool ioError) {
    uint64_t nanoTime = NanoTime();
    uint64_t waitTime = appInForeground ? WRITE_TASK_FG_WAIT_TIME : WRITE_TASK_BG_WAIT_TIME;
    uint64_t targetTime = nanoTime + waitTime;
    gc::Heap * curHeap = getHeapChecked();
    gc::TaskProcessor * taskProcessor = getTaskProcessorChecked();
    if (taskProcessor != nullptr && taskProcessor->IsRunning()) {
        if (ioError) {
            LOG(INFO) << "NIEL not scheduling WriteTask again due to IO error";
        }
        else {
            if (appInForeground) {
                // jiacheng start
                // curHeap->RequestConcurrentGC(self, true);
                curHeap->RequestConcurrentGC(self, gc::kGcCauseBackground, true);
                // jiacheng end
            }
            else {
                if (bgMarkSweepCounter >= BG_MARK_SWEEPS_BEFORE_SEMI_SPACE) {
                    // jiacheng start
                    // curHeap->PerformHomogeneousSpaceCompact();
                    if (kUseReadBarrier) {
                        curHeap->RequestConcurrentGC(self, gc::kGcCauseBackground, true);
                        CreateStubsAndSwapOut();
                    } else {
                        curHeap->PerformHomogeneousSpaceCompact();
                    }
                    // jiacheng end
                    bgMarkSweepCounter = 0;
                }
                else {
                    // jiacheng start
                    // curHeap->RequestConcurrentGC(self, true);
                    curHeap->RequestConcurrentGC(self, gc::kGcCauseBackground, true);
                    // jiacheng end
                    bgMarkSweepCounter++;
                }
            }
            taskProcessor->AddTask(self, new WriteTask(targetTime));
        }
    }
}

int writeToSwapFile(Thread * self, gc::Heap * heap, mirror::Object * object, bool * ioError) {
    int result = SWAPFILE_WRITE_OK;
    *ioError = false;

    Stub * stub = nullptr;
    if (object->GetStubFlag()) {
        stub = (Stub *)object;
        stub->LockTableEntry();
        object = stub->GetObjectAddress();

        // An object being written to the swap file is dirty and therefore
        // should never be reclaimed before writeToSwapFile() finishes running
        // on it.
        CHECK(object != nullptr);
    }

    CHECK(objectInSwappableSpace(heap, object)) << " addr " << object;

    bool validObject = false;
    if (object != nullptr) {
        object->SetIgnoreReadFlag();
        if (object->GetClass() != nullptr) {
            validObject = true;
        }
        else {
            result = SWAPFILE_WRITE_NULL_CLASS;
        }
        object->ClearIgnoreReadFlag();
    }

    bool noSwap = false;
    if (object->GetNoSwapFlag()) {
        noSwap = true;
        result = SWAPFILE_WRITE_NO_SWAP;
    }

    if (validObject && !noSwap) {
        object->SetIgnoreReadFlag();
        size_t objectSize = object->SizeOf();
        object->ClearIgnoreReadFlag();

        char * objectData = (char *)malloc(objectSize);
        object->ClearDirtyBit();
        std::memcpy(objectData, object, objectSize);

        /*
         * The code below is a bit convoluted because the key used to look up
         * an object's data in the objectOffsetMap and objectSizeMap differs
         * depending on whether the object has a stub. If the object has a
         * stub, its stub is the key; if the object does not have a stub (i.e.,
         * it was selected for swapping since the last background transition),
         * the object itself is the key.
         */
        bool objectInSwapFileMaps = false;

        swapFileMapsMutex.SharedLock(self);
        objectInSwapFileMaps = (objectOffsetMap.find(object) != objectOffsetMap.end());
        swapFileMapsMutex.SharedUnlock(self);

        CHECK(!(stub != nullptr && objectInSwapFileMaps));

        void * swapStateKey = nullptr;
        bool inSwapFile = false;

        if (stub != nullptr) {
            swapStateKey = stub;
            inSwapFile = true;
        }
        else if (objectInSwapFileMaps) {
            swapStateKey = object;
            inSwapFile = true;
        }
        else {
            swapStateKey = object;
            inSwapFile = false;
        }

        if (!inSwapFile) {
            swapFileMutex.ExclusiveLock(self);
            std::streampos offset = swapfile.tellp();
            swapfile.write(objectData, objectSize);
            bool writeError = checkStreamError(swapfile,
                    "writing object to swapfile for the first time in WriteTask");
            if (writeError) {
                *ioError = true;
            }
            swapFileMutex.ExclusiveUnlock(self);

            swapFileMapsMutex.ExclusiveLock(self);
            objectOffsetMap[swapStateKey] = offset;
            objectSizeMap[swapStateKey] = objectSize;
            swapFileMapsMutex.ExclusiveUnlock(self);

            swapfileObjects++;
            swapfileSize += objectSize;
        }
        else {
            swapFileMapsMutex.SharedLock(self);
            std::streampos offset = objectOffsetMap[swapStateKey];
            size_t size = objectSizeMap[swapStateKey];
            swapFileMapsMutex.SharedUnlock(self);

            if (size != objectSize) {
                result = SWAPFILE_WRITE_RESIZED;
            }
            else {
                swapFileMutex.ExclusiveLock(self);
                std::streampos curpos = swapfile.tellp();
                swapfile.seekp(offset);
                swapfile.write(objectData, objectSize);
                bool writeError = checkStreamError(swapfile,
                        "writing object to swapfile again in WriteTask");
                if (writeError) {
                    *ioError = true;
                }
                swapfile.seekp(curpos);
                swapFileMutex.ExclusiveUnlock(self);
            }
        }
        free(objectData);
    }
    if (stub != nullptr) {
        stub->UnlockTableEntry();
    }
    return result;
}

bool shouldExcludeObjectAndMembers(mirror::Object * obj ATTRIBUTE_UNUSED) {
#if COMMERCIAL_APP_COMPAT_MODE
    return true;
#else
    return false;
#endif
}

class ExcludeVisitor {
  public:
    void operator()(mirror::Object * obj,
                    MemberOffset offset,
                    bool is_static ATTRIBUTE_UNUSED) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        mirror::Object * ref = obj->GetFieldObject<mirror::Object>(offset);
        if (ref != nullptr) {
            if (ref->GetStubFlag()) {
                LOG(ERROR) << "NIELERROR ref " << ref << " of object " << obj
                           << " that should be excluded from swapping was already swapped out";
            }
            else {
                ref->SetNoSwapFlag();
            }
        }
    }

    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}
};

class DummyReferenceVisitor {
  public:
    void operator()(ObjPtr<art::mirror::Class> klass ATTRIBUTE_UNUSED,
                    ObjPtr<art::mirror::Reference> ref ATTRIBUTE_UNUSED) const { }
};

// Based on MarkVisitor in runtime/gc/collector/mark_sweep.cc
class PatchVisitor {
  public:
    void operator()(mirror::Object * obj,
                    MemberOffset offset,
                    bool is_static ATTRIBUTE_UNUSED) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        mirror::Object * ref = obj->GetFieldObject<mirror::Object>(offset);
        if (remappingTable.count(ref)) {
            obj->SetFieldObject<false>(offset, (mirror::Object *)remappingTable[ref]);
        }
    }

    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}
};


// Method signature from MarkSweep::DelayReferenceReferentVisitor in
// runtime/gc/collector/mark_sweep.cc.
class PatchReferenceVisitor {
  public:
    void operator()(ObjPtr<art::mirror::Class> klass ATTRIBUTE_UNUSED,
                    ObjPtr<art::mirror::Reference> ref) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        mirror::Object * referent = ref->GetReferent();
        if (remappingTable.count(referent)) {
            ref->SetReferent<false>((mirror::Object *)remappingTable[referent]);
        }
    }
};

// jiacheng start
// void PatchCallback(void * start, void * end ATTRIBUTE_UNUSED, size_t num_bytes,
//                   void * callback_arg ATTRIBUTE_UNUSED)
void PatchCallback(void * start, void * end, size_t num_bytes,
                  void * callback_arg ATTRIBUTE_UNUSED)
// jiacheng end
        REQUIRES(Locks::mutator_lock_) {
    if (start == nullptr || num_bytes == 0) {
        return;
    }

    // jiacheng start
    if (start >= end) {
        return;
    }
    // jiacheng end

    mirror::Object * obj = (mirror::Object *)start;
    if (obj->GetStubFlag()) {
        Stub * stub = (Stub *)obj;
        for (int i = 0; i < stub->GetNumRefs(); i++) {
            mirror::Object * ref = stub->GetReference(i);
            if (remappingTable.count(ref)) {
                stub->SetReference(i, (mirror::Object *)remappingTable[ref]);
            }
        }
    }
    else {
        PatchVisitor visitor;
        PatchReferenceVisitor referenceVisitor;
        obj->VisitReferences(visitor, referenceVisitor);
    }
}

// jiacheng start
class PatchCallbackJiachengVisitor {
public:
    void operator() (mirror::Object* obj) {
        if (obj == nullptr) {
            return;
        }
        Thread* self = Thread::Current();
        Locks::mutator_lock_->AssertExclusiveHeld(self);
        if (obj->GetStubFlag()) {
            Stub * stub = (Stub *)obj;
            for (int i = 0; i < stub->GetNumRefs(); i++) {
                mirror::Object * ref = stub->GetReference(i);
                if (remappingTable.count(ref)) {
                    stub->SetReference(i, (mirror::Object *)remappingTable[ref]);
                }
            }
        }
        else {
            PatchVisitor visitor;
            PatchReferenceVisitor referenceVisitor;
            obj->VisitReferences(visitor, referenceVisitor);
        }
    }
};
// jiacheng end

class GlobalRefRootVisitor : public RootVisitor {
    void VisitRoots(mirror::Object*** roots, size_t count,
                    const RootInfo & info ATTRIBUTE_UNUSED)
            REQUIRES_SHARED(Locks::mutator_lock_) {
        for (size_t i = 0; i < count; i++) {
            mirror::Object * oldRef = *roots[i];
            if (remappingTable.count(oldRef)) {
                *roots[i] = (mirror::Object *)remappingTable[oldRef];
                LOG(INFO) << "NIEL VisitRoots patching ref " << oldRef << " to " << remappingTable[oldRef];
            }
        }
    }

    void VisitRoots(mirror::CompressedReference<mirror::Object>** roots, size_t count,
                    const RootInfo & info ATTRIBUTE_UNUSED)
            REQUIRES_SHARED(Locks::mutator_lock_) {
        for (size_t i = 0; i < count; i++) {
            mirror::Object * oldRef = roots[i]->AsMirrorPtr();
            if (remappingTable.count(oldRef)) {
                roots[i]->Assign((mirror::Object *)remappingTable[oldRef]);
                LOG(INFO) << "NIEL VisitRoots patching ref " << oldRef << " to " << remappingTable[oldRef];
            }
        }
    }
};

void patchStubReferences(Thread * self, gc::Heap * heap) {
    ScopedTimer timer("patching stub references");
    // jiacheng start
    LOG(INFO) << "jiacheng debug niel_swap.cc patchStubReferences() 711";
    // jiacheng end

    // jiacheng start
    // 原始代码中没有处理zygote space, image space, non-moving-space 中的reference
    // gc::space::LargeObjectSpace * largeObjectSpace = heap->GetLargeObjectsSpace();
    // largeObjectSpace->Walk(&PatchCallback, nullptr);

    // gc::space::RosAllocSpace * rosAllocSpace = heap->GetRosAllocSpace();
    // rosAllocSpace->Walk(&PatchCallback, nullptr); // 因为stub全部在rosalloc space中，在这里重定向了stub中的引用

    PatchCallbackJiachengVisitor patchCallbackJiachengVisitor;
    heap->PatchStubVisitObjects(patchCallbackJiachengVisitor, &PatchCallback);
    // jiacheng end

    // "Walk" the swappedInSpace and do the same thing that the PatchCallback
    // does in the other spaces.
    for (TableEntry * rte = recTable.Begin(); rte < recTable.End(); rte++) {
        rte->LockFromAppThread();
        if (rte->GetOccupiedBit() && rte->GetResidentBit()) {
            mirror::Object * obj = rte->GetObjectAddress();
            CHECK(obj != nullptr);
            PatchVisitor visitor;
            PatchReferenceVisitor referenceVisitor;
            obj->VisitReferences(visitor, referenceVisitor);
        }
        rte->UnlockFromAppThread();
    }

    GlobalRefRootVisitor visitor;
    Runtime::Current()->VisitRoots(&visitor, kVisitRootFlagAllRoots);

    // jiacheng start
    (void)self;
    // replaceDataStructurePointers(self, remappingTable, false);
    // jiacheng end
}

//TODO: figure out whether it's better to grab the lock once or keep grabbing
//      and releasing
void replaceDataStructurePointers(Thread * self, const std::map<void *, void *> & addrTable,
                                  bool removeUntouchedRosAlloc) {
    // jiacheng start
    LOG(INFO) << "jiacheng debug niel_swap.cc replaceDataStructurePointers() 764" 
              << " createStubSet.size()= " << createStubSet.size()
              << " objectOffsetMap.size()= " << objectOffsetMap.size();
    // jiacheng end
    swapFileMapsMutex.ExclusiveLock(self);

    std::set<void *> swapFileMapsRemoveSet;
    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        if (!addrTable.count(it->first)) {
            // Assumes that a pointer not in the large object space or
            // SwappedInSpace was in the old RosAlloc space.
            if (   !getHeapChecked()->GetLargeObjectsSpace()
                                    ->Contains((mirror::Object *)it->first)
                && !swappedInSpace->Contains((mirror::Object *)it->first)
                // jiacheng start
                && !getHeapChecked()->GetRegionSpace()->Contains((mirror::Object *)it->first)
                // jiacheng end
                ) {
              swapFileMapsRemoveSet.insert(it->first);
            }
        }
    }

    for (auto it = addrTable.begin(); it != addrTable.end(); it++) {
        void * originalPtr = it->first;
        void * newPtr = it->second;

        CHECK_EQ(objectOffsetMap.count(originalPtr), objectSizeMap.count(originalPtr));

        if (objectOffsetMap.count(originalPtr)) {
            auto oomIt = objectOffsetMap.find(originalPtr);
            // jiacheng start
            objectOffsetMap[newPtr] = oomIt->second;
            if (newPtr != originalPtr) {
                objectOffsetMap.erase(oomIt);
            }
            // jiacheng end

            auto osmIt = objectSizeMap.find(originalPtr);
            // jiacheng start
            objectSizeMap[newPtr] = osmIt->second;
            if (newPtr != originalPtr) {
                objectSizeMap.erase(osmIt);
            }
            // jiacheng end
        }
    }

    if (removeUntouchedRosAlloc) {
        for (auto it = swapFileMapsRemoveSet.begin(); it != swapFileMapsRemoveSet.end(); it++) {
            objectOffsetMap.erase(*it);
            objectSizeMap.erase(*it);
        }

        for (auto it = stubRTEMap.begin(); it != stubRTEMap.end(); it++) {
            Stub * stub = it->first;
            TableEntry * rte = it->second;
            if (!addrTable.count(stub)) {
                rte->LockFromAppThread();
                mirror::Object * obj = rte->GetObjectAddress();
                if (obj != nullptr) {
                    swappedInSpace->Free(self, obj);
                }
                rte->ClearOccupiedBit();
                rte->UnlockFromAppThread();
            }
        }
        stubRTEMap.clear();

        // No need to remove pointers from these data structures, since they
        // should be empty during a semi-space GC.
        CHECK_EQ(createStubSet.size(), 0u);
        CHECK_EQ(writeQueue.size(), 0u);
        CHECK_EQ(writeSet.size(), 0u);
    }
    swapFileMapsMutex.ExclusiveUnlock(self);

    // jiacheng start
    for (auto it = addrTable.begin(); it != addrTable.end(); it++) {
        mirror::Object * originalPtr = static_cast<mirror::Object *>(it->first);
        mirror::Object * newPtr = static_cast<mirror::Object *>(it->second);

        if (createStubSet.count(originalPtr)) {
            createStubSet.erase(originalPtr);
            createStubSet.insert(newPtr);
        }
    }
    // jiacheng end
}

void FreeFromRosAllocSpace(Thread * self, gc::Heap * heap, mirror::Object * obj) {
    obj->SetIgnoreReadFlag();
    size_t objSize = obj->SizeOf();
    obj->ClearIgnoreReadFlag();

    heap->GetRosAllocSpace()->GetLiveBitmap()->Clear(obj);
    heap->GetRosAllocSpace()->FreeList(self, 1, &obj);
    heap->RecordFree(1, objSize);
}

void FreeFromLargeObjectSpace(Thread * self, gc::Heap * heap, mirror::Object * obj) {
    obj->SetIgnoreReadFlag();
    size_t objSize = obj->SizeOf();
    obj->ClearIgnoreReadFlag();

    heap->GetLargeObjectsSpace()->GetLiveBitmap()->Clear(obj);
    heap->GetLargeObjectsSpace()->Free(self, obj);
    heap->RecordFree(1, objSize);
}

void UnlockAllReclamationTableEntries() {
    recTable.UnlockAllEntries();
}

void CreateStubs(Thread * self, gc::Heap * heap) {
    if (!swapEnabled) {
        return;
    }

    ScopedTimer timer("creating stubs");

    CHECK(remappingTable.size() == 0);

    // Remove any object from createStubSet whose NoSwapFlag was set since it was
    // added (currently, this only happens because the object was marked for
    // exclusion)
    std::set<mirror::Object *> removeSet;
    
    for (auto it = createStubSet.begin(); it != createStubSet.end(); it++) {
        mirror::Object * obj = *it;
        // jiacheng start
        // if (obj->GetNoSwapFlag()) {
        //     removeSet.insert(obj);
        // }
        if (!obj || obj->GetNoSwapFlag() || obj->SizeOf()==0) {
            removeSet.insert(obj);
        }
        // jiacheng end
    }
    for (auto it = removeSet.begin(); it != removeSet.end(); it++) {
        createStubSet.erase(*it);
    }

    // jiacheng start
    LOG(INFO) << "jiacheng niel_swap.cc 785 CreateStubs() "
            << " removeSet.size()= " << removeSet.size()
            << " createStubSet.size()= " << createStubSet.size()
            ;
    // jiacheng end

    creatingStubs = true;
    // jiacheng start
    size_t jiacheng_stub_size = 0;
    size_t jiacheng_object_size = 0;
    size_t jiacheng_page_size = 0;
    // jiacheng end
    for (auto it = createStubSet.begin(); it != createStubSet.end(); it++) {
        mirror::Object * obj = *it;

        CHECK(!obj->GetStubFlag());

        CHECK(objectInSwappableSpace(heap, obj));

        // Create the stub for this object
        size_t stubSize = Stub::GetStubSize(obj);
        size_t bytes_allocated;
        size_t usable_size;
        size_t bytes_tl_bulk_allocated;
        // jiacheng start
        // mirror::Object * stubData = heap->GetRosAllocSpace()
        //                                  ->Alloc(self,
        //                                          stubSize,
        //                                          &bytes_allocated,
        //                                          &usable_size,
        //                                          &bytes_tl_bulk_allocated);
        mirror::Object * stubData = nullptr;
        if (kUseReadBarrier) {
            stubData = heap->GetRegionSpace()->Alloc(self,
                                                        stubSize,
                                                        &bytes_allocated,
                                                        &usable_size,
                                                        &bytes_tl_bulk_allocated);
        } else {
            stubData = heap->GetRosAllocSpace()->Alloc(self,
                                                    stubSize,
                                                    &bytes_allocated,
                                                    &usable_size,
                                                    &bytes_tl_bulk_allocated);
        }
        // jiacheng end
        CHECK(stubData != nullptr);
        Stub * stub = (Stub *)stubData;
        TableEntry * entry = recTable.CreateEntry();
        stub->SetTableEntry(entry);
        stub->LockTableEntry();
        stub->PopulateFrom(obj);

        // Copy the object into the swappedInSpace
        obj->SetIgnoreReadFlag();
        size_t objSize = obj->SizeOf();
        obj->ClearIgnoreReadFlag();
        mirror::Object * objCopy = swappedInSpace->Alloc(self,
                                                         objSize,
                                                         &bytes_allocated,
                                                         &usable_size,
                                                         &bytes_tl_bulk_allocated);
        CHECK(objCopy != nullptr);
        std::memcpy(objCopy, obj, objSize);
        // Free the original copy of the object

        if (heap->GetLargeObjectsSpace()->Contains(obj)) {
            FreeFromLargeObjectSpace(self, heap, obj);
        }
        // jiacheng start
        // else if (heap->GetRosAllocSpace()->Contains(obj)) {
        //     FreeFromRosAllocSpace(self, heap, obj);
        // }
        // else {
        //     LOG(FATAL) << "NIELERROR object " << obj << " not in RosAlloc space or LOS";
        // }
        // jiacheng end

        // Do bookkeeping
        remappingTable[obj] = stub;
        stub->SetObjectAddress(objCopy);
        stub->GetTableEntry()->SetResidentBit();
        int numPages = objSize / kPageSize;
        if (objSize % kPageSize > 0) {
            numPages += 1;
        }
        // jiacheng start
        jiacheng_stub_size += stubSize;
        jiacheng_object_size += objSize;
        jiacheng_page_size += numPages * kPageSize;
        // jiacheng end

        stub->GetTableEntry()->SetNumPages(numPages);
        stub->UnlockTableEntry();

        // jiacheng start
        // LOG(INFO) << "jiacheng debug niel_swap.cc CreateStubs() 1013 "
        //           << " to_obj= " << objCopy
        //           << " from_obj= " << obj
        //           << " objSize= " << objSize
        //           << " stub= " << stub
        //           << " entry= " << entry
        //           << " stub->GetTableEntry()= " << stub->GetTableEntry();
        // jiacheng end
    }

    // jiacheng start
    LOG(INFO) << "NIEL jiacheng niel_swap.cc 871 CreateStubs() After "
            << " jiacheng_stub_size= " << jiacheng_stub_size
            << " jiacheng_object_size= " << jiacheng_object_size
            << " jiacheng_page_size= " << jiacheng_page_size
            ;
    // jiacheng end

    // jiacheng start
    // patchStubReferences(self, heap);    
    replaceDataStructurePointers(self, remappingTable, false);
    // jiacheng end

    // jiacheng start
    remappingTable.clear();
    createStubSet.clear();
    creatingStubs = false;

    // Clear the write queue (and write set), since after the semi-space GC
    // runs, the pointers in the write queue will be invalid
    // jiacheng start
    writeQueueMutex.ExclusiveLock(self);
    writeQueue.clear();
    writeSet.clear();
    writeQueueMutex.ExclusiveUnlock(self);


    // jiacheng end
}

bool doKernelReclamationChecks(TableEntry * rte) {
    // jiacheng start
    mirror::Object * obj = rte->GetObjectAddress();
    if (!obj) return false;
    // return true;
    // jiacheng end

    if (rte->GetOccupiedBit() == 0) {
        return false;
    }
    // if (rte->GetAppLockCounter() > 0) {
    //     return false;
    // }

    if (!rte->GetResidentBit()) {
        return false;
    }

    // // jiacheng start
    // // mirror::Object * obj = rte->GetObjectAddress();
    // // CHECK(obj != nullptr);
    // // jiacheng end

    // bool isDirty = obj->GetDirtyBit();
    // if (isDirty) {
    //     return false;
    // }

    // uint8_t wsrVal = obj->GetWriteShiftRegister();
    // bool wasWritten = obj->GetWriteBit();
    // if (!objectIsCold(wsrVal, wasWritten)) {
    //     return false;
    // }

    return true;
}

// NOTE: This method implements the memory-reclaiming functionality that would
// be performed by the OS according to our design.
void SwapObjectsOut() {
    if (!swapEnabled) {
        return;
    }

    ScopedTimer timer("swapping objects out");

    for (TableEntry * rte = recTable.Begin(); rte < recTable.End(); rte++) {
        bool firstCheck = doKernelReclamationChecks(rte);
        if (firstCheck) {
            rte->SetKernelLockBit();
            bool secondCheck = doKernelReclamationChecks(rte);
            if (secondCheck) {
                mirror::Object * obj = rte->GetObjectAddress();
                CHECK(swappedInSpace->Contains(obj)) << " obj= " << obj;
                size_t range = rte->GetNumPages() * kPageSize;
                madvise(obj, range, MADV_DONTNEED);
                rte->ClearResidentBit();
                // jiacheng start
                // LOG(INFO) << "jiacheng debug niel_swap.cc SwapObjectsOut() 1087"
                //           << " obj= " << obj
                //           << " range= " << range;
                // jiacheng end
            }
            rte->ClearKernelLockBit();
        }
    }
}

void RecordForwardedObject(Thread * self, mirror::Object * obj, mirror::Object * forwardAddress) {
    if (!swapEnabled) {
        return;
    }

    swapFileMapsMutex.SharedLock(self);
    // jiacheng start
    // if (objectOffsetMap.count(obj)) {
    if (objectOffsetMap.count(obj) || createStubSet.count(obj)) {        
    // jiacheng end
        semiSpaceRemappingTable[obj] = forwardAddress;
    }
    swapFileMapsMutex.SharedUnlock(self);
}

void SemiSpaceRecordStubMappings(Thread * self) {
    if (!swapEnabled) {
        return;
    }

    // This code assumes that all stubs appear as a key in
    // objectOffsetMap/objectSizeMap, so we can identify all live stubs by
    // iterating over objectOffsetMap.
    //
    // TODO: Think about whether this assumption is correct.
    swapFileMapsMutex.SharedLock(self);
    CHECK_EQ(stubRTEMap.size(), 0u);
    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        mirror::Object * obj = (mirror::Object *)it->first;
        if (obj->GetStubFlag()) {
            Stub * stub = (Stub *)obj;
            stubRTEMap[stub] = stub->GetTableEntry();
        }
    }
    swapFileMapsMutex.SharedUnlock(self);
}

void SemiSpaceUpdateDataStructures(Thread * self) {
    if (!swapEnabled) {
        return;
    }

    /*
     * We call replaceDataStructurePointers() with removeUntouchedRosAlloc set
     * to true because we assume that if an object/stub in the old RosAlloc
     * space is present in the objectOffsetMap/objectSizeMap
     * but missing from the semiSpaceRemappingTable, it is missing because it
     * was freed by the SemiSpace GC.
     */
    replaceDataStructurePointers(self, semiSpaceRemappingTable, false);
    semiSpaceRemappingTable.clear();
}

// jiacheng start
void ConcurrentCopyingRecordStubMappings(Thread * self) {
    if (!swapEnabled) {
        return;
    }

    swapFileMapsMutex.SharedLock(self);
    CHECK_EQ(stubRTEMap.size(), 0u);
    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        mirror::Object * obj = (mirror::Object *)it->first;
        if (obj->GetStubFlag()) {
            Stub * stub = (Stub *)obj;
            stubRTEMap[stub] = stub->GetTableEntry();
        }
    }
    swapFileMapsMutex.SharedUnlock(self);
}

void ConcurrentCopyingUpdateDataStructures(Thread * self) {
    if (!swapEnabled) {
        return;
    }

    replaceDataStructurePointers(self, semiSpaceRemappingTable, false);
    semiSpaceRemappingTable.clear();
}

// jiacheng end

mirror::Object * swapInObject(Thread * self, Stub * stub, std::streampos objOffset,
                              size_t objSize) {
    // jiacheng start
    // LOG(INFO) << "jiacheng debug niel_swap.cc swapInObject() 1141"
    //           << " stub= " << stub 
    //           << " objSize= " << objSize;
    if (objSize == 0) {
        return nullptr;
    }
    // jiacheng end

    CHECK(stub->GetTableEntry()->GetResidentBit() == 0);
    CHECK(stub->GetObjectAddress() != nullptr);

    mirror::Object * destObj = stub->GetObjectAddress();


    swapFileMutex.ExclusiveLock(self);
    std::streampos curPos = swapfile.tellp();
    swapfile.seekg(objOffset);
    swapfile.read((char *)destObj, objSize);
    swapfile.seekg(curPos);
    swapFileMutex.ExclusiveUnlock(self);

    CHECK(destObj->GetDirtyBit() == 0);

    // jiacheng start
    stub->CopyMetaData(destObj);
    // jiacheng end
    stub->CopyRefsInto(destObj);
    stub->GetTableEntry()->SetResidentBit();

    return destObj;
}

//TODO: make sure obj doesn't get freed during GC as long as stub isn't freed,
//      but is freed when stub is freed
void SwapInOnDemand(Stub * stub) {
    // jiacheng start
    LOG(INFO) << "jiacheng debug SwapInOnDemand() stub= " << stub;
    // jiacheng end
    CHECK(swapEnabled);

    gc::Heap * heap = getHeapChecked();
    CHECK(heap->GetRegionSpace()->Contains((mirror::Object *)stub));

    Thread * self = Thread::Current();

    swapFileMapsMutex.SharedLock(self);
    std::streampos objOffset = objectOffsetMap[stub];
    size_t objSize = objectSizeMap[stub];
    swapFileMapsMutex.SharedUnlock(self);

    swapInObject(self, stub, objOffset, objSize);
}


void SwapObjectsIn(gc::Heap * heap) {
    if (!swapEnabled) {
        return;
    }

    if (!SWAP_IN_ON_FOREGROUND) {
        return;
    }
    // jiacheng start
    size_t all_swap_in_size = 0;
    ScopedTimer timer("swapping objects back in");
    LOG(INFO) << "jiacheng debug niel_swap.cc SwapObjectsIn() Start 1242"
              << " objectOffsetMap.size()= " << objectOffsetMap.size();
    // jiacheng end

    Thread * self = Thread::Current();

    swapFileMapsMutex.SharedLock(self);
    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        mirror::Object * obj = (mirror::Object *)it->first;
        if (obj->GetStubFlag()) {
            Stub * stub = (Stub *)obj;
            CHECK(heap->GetRegionSpace()->Contains((mirror::Object *)stub));

            std::streampos objOffset = objectOffsetMap[stub];
            size_t objSize = objectSizeMap[stub];

            // Only swap in object if it wasn't already swapped in on-demand
            stub->LockTableEntry();
            if (!stub->GetTableEntry()->GetResidentBit()) {
                swapInObject(self, stub, objOffset, objSize);
                all_swap_in_size += objSize;
            }
            stub->UnlockTableEntry();
        }
    }
    swapFileMapsMutex.SharedUnlock(self);

    // jiacheng start
    LOG(INFO) << "jiacheng debug niel_swap.cc SwapObjectsIn() End 1253"
              << " objectOffsetMap.size()= " << objectOffsetMap.size()
              << " all_swap_in_size= " << all_swap_in_size;
    // jiacheng end
}

gc::TaskProcessor * getTaskProcessorChecked() {
    gc::Heap * heap = getHeapChecked();
    if (heap == nullptr) {
        return nullptr;
    }
    return heap->GetTaskProcessor();
}

void GcRecordFree(Thread * self, mirror::Object * object) {
    if (!swapEnabled) {
        return;
    }

    /*
     * The correctness of this check depends on upon several assumptions:
     * 1) Only the garbage collectors and my swapping code call Free() on the
     *    RosAlloc space and large object space.
     * 2) The garbage collectors never run concurrently with any swapping
     *    methods that free objects or stubs as part of swapping (currently,
     *    the only such method is SwapObjectsOut()).
     */
    if (creatingStubs) {
        return;
    }

    // jiacheng start
    // if (swappedInSpace->Contains(object)) {
    if (swappedInSpace && swappedInSpace->Contains(object)) {
    // jiacheng end
        return;
    }
    writeQueueMutex.ExclusiveLock(self);
    auto writeQueuePos = std::find(writeQueue.begin(), writeQueue.end(), object);
    if (writeQueuePos != writeQueue.end()) {
        writeQueue.erase(writeQueuePos);
        writeSet.erase(object);
    }
    writeQueueMutex.ExclusiveUnlock(self);
    swapFileMapsMutex.ExclusiveLock(self);
    if (objectOffsetMap.count(object) && objectSizeMap.count(object)) {
        freedObjects++;
        freedSize += objectSizeMap[object];
        objectOffsetMap.erase(object);
        objectSizeMap.erase(object);
    }
    else if (objectOffsetMap.count(object) || objectSizeMap.count(object)) {
        LOG(ERROR) << "NIELERROR: object in one of the object maps but not the other";
    }
    swapFileMapsMutex.ExclusiveUnlock(self);

    if (createStubSet.count(object)) {
        createStubSet.erase(object);
    }
    if (object->GetStubFlag()) {
        Stub * stub = (Stub *)object;
        stub->LockTableEntry();
        mirror::Object * swappedInObj = stub->GetObjectAddress();
        if (swappedInObj != nullptr) {
            CHECK(swappedInSpace->Contains(swappedInObj));
            swappedInSpace->Free(self, swappedInObj);
        }
        stub->GetTableEntry()->ClearOccupiedBit();
        stub->UnlockTableEntry();
    }
}

// jiacheng start
void GcRecordFreeRegion(Thread * self, size_t begin, size_t end, size_t region_idx) {
    if (!swapEnabled) {
        return;
    }
    CHECK(!creatingStubs);

    std::vector<mirror::Object *> retainQueue;
    writeQueueMutex.ExclusiveLock(self);
    for (auto it = writeQueue.begin(); it != writeQueue.end(); it++) {
        size_t obj = reinterpret_cast<size_t>(*it);
        if (obj<begin || obj>=end) {
            retainQueue.push_back(*it);
        }
    }
    writeQueue.clear();
    writeSet.clear();
    for (auto it = retainQueue.begin(); it != retainQueue.end(); it++) {
        writeQueue.push_back(*it);
        writeSet.insert(*it);
    }
    writeQueueMutex.ExclusiveUnlock(self);

    std::set<void *> removeSet;
    swapFileMapsMutex.ExclusiveLock(self);
    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        size_t obj = reinterpret_cast<size_t>(it->first);
        if (obj>=begin && obj<end) {
            removeSet.insert(it->first);
        }
    }
    for (auto it = removeSet.begin(); it != removeSet.end(); it++) {
        mirror::Object* object = static_cast<mirror::Object*>(*it);
        if (!objectSizeMap.count(object)) {
            LOG(ERROR) << "NIELERROR: object in one of the object maps but not the other";
        } else {
            freedObjects++;
            freedSize += objectSizeMap[object];
            objectOffsetMap.erase(object);
            objectSizeMap.erase(object);
        }
    }
    swapFileMapsMutex.ExclusiveUnlock(self);

    removeSet.clear();
    for (auto it = createStubSet.begin(); it != createStubSet.end(); it++) {
        size_t obj = reinterpret_cast<size_t>(*it);
        if (obj>=begin && obj<end) {
            removeSet.insert(*it);
        }
    }
    for (auto it = removeSet.begin(); it != removeSet.end(); it++) {
        mirror::Object* obj = static_cast<mirror::Object *>(*it);
        createStubSet.erase(obj);
    }
    // jiacheng start
    // jiacheng TODO
    (void)region_idx;
    // gc::Heap* heap = Runtime::Current()->GetHeap();
    // gc::space::RegionSpace* region_space = heap->GetRegionSpace();
    // region_space->JiachengFreeStubByRegion(region_idx, swappedInSpace);
    // jiacheng end

}
// jiacheng end

void InitIfNecessary(Thread * self) {
    uint32_t curPid = getpid();

    // jiacheng start
    LOG(INFO) << "jiacheng debug niel_swap.cc InitIfNecessary() 1413 "
              << " curPid= " << curPid
              << " pid= " << pid
              << " objectOffsetMap.size()= " << objectOffsetMap.size();
    // jiacheng end

    if (curPid == pid) {
        return;
    }

    gc::TaskProcessor * taskProcessor = getTaskProcessorChecked();
    if (taskProcessor == nullptr || !taskProcessor->IsRunning()) {
        LOG(ERROR) << "NIELERROR failed to init swap since heap's TaskProcessor is null or not "
                   << "ready (or heap is null)";
        return;
    }

    // Once we reach this point, we will not try to init swap again
    // until the next time the PID changes

    pid = curPid;

    swapFileMapsMutex.ExclusiveLock(self);
    objectOffsetMap.clear();
    objectSizeMap.clear();
    swapFileMapsMutex.ExclusiveUnlock(self);

    writeQueueMutex.ExclusiveLock(self);
    writeQueue.clear();
    writeSet.clear();
    writeQueueMutex.ExclusiveUnlock(self);

    std::string packageName = getPackageName();

    // jiacheng start
    if (!appOnCommonWhitelist()) {
        return;
    }
    
    std::string swapfilePath("/data/data/" + packageName + "/swapfile");
    // std::string swapfilePath("/sdcard/zram/" + packageName + "-swapfile");
    // jiacheng end

    // jiacheng start
    // if (appOnCommonBlacklist(packageName)) {
    //     LOG(ERROR) << "NIELERROR stopping swap initialization due to blacklisted app"
    //                << " (package name " << packageName << ")";
    //     return;
    // }

    // jiacheng end

    swapFileMutex.ExclusiveLock(self);
    openFile(swapfilePath, swapfile);
    swapfile.write((char *)&pid, 4);
    swapfile.flush();
    bool ioError = checkStreamError(swapfile, "after opening swapfile");
    swapFileMutex.ExclusiveUnlock(self);

    if (ioError) {
        LOG(ERROR) << "NIELERROR not scheduling first WriteTask due to IO error (package name "
                   << packageName << ")";
        return;
    }

    {
        ScopedSuspendAll ssa("niel_init_swap");
        // jiacheng start
        uint8_t * start = (uint8_t *)SWAPPED_IN_SPACE_START;

        CHECK(!swappedInSpace);
        
        swappedInSpace = gc::space::FreeListSpace::JiachengCreate("SwappedInSpace",
                                                          start,
                                                          SWAPPED_IN_SPACE_SIZE);
        // swappedInSpace = gc::space::LargeObjectMapSpace::JiachengCreate("SwappedInSpace",
        //                                                   start,
        //                                                   SWAPPED_IN_SPACE_SIZE);
        SWAPPED_IN_SPACE_START = reinterpret_cast<uintptr_t>(swappedInSpace->Begin());
        // jiacheng end

        getHeapChecked()->AddSpace(swappedInSpace);

        if (swappedInSpace == nullptr) {
            LOG(ERROR) << "NIELERROR SwappedInSpace is null";
            return;
        }
        if (swappedInSpace->Begin() != (uint8_t *)SWAPPED_IN_SPACE_START) {
            LOG(ERROR) << "NIELERROR SwappedInSpace begins at wrong address: "
                       << (void *)swappedInSpace->Begin();
            return;
        }
    }

    recTable = ReclamationTable::CreateTable(REC_TABLE_NUM_ENTRIES);
    if (!recTable.IsValid()) {
        LOG(ERROR) << "NIELERROR error creating reclamation table";
        return;
    }

    swapEnabled = true;
    uint64_t targetTime = NanoTime() + WRITE_TASK_STARTUP_DELAY;
    taskProcessor->AddTask(Thread::Current(), new WriteTask(targetTime));

    LOG(INFO) << "NIEL successfully initialized swap for package " << packageName;
    LOG(INFO) << "NIEL commercial app compat mode is "
              << (COMMERCIAL_APP_COMPAT_MODE ? "enabled" : "disabled");
    LOG(INFO) << "NIEL preemptive swap-in on foreground transitions is "
              << (SWAP_IN_ON_FOREGROUND ? "enabled" : "disabled");
}

bool copyFile(const std::string & fromFileName, const std::string & toFileName) {
    bool copyingError = false;
    int removeRet = remove(toFileName.c_str());
    if (removeRet < 0) {
        LOG(INFO) << "NIEL error removing file (maybe expected): " << toFileName;
    }

    struct stat statBuf;
    int ret = stat(fromFileName.c_str(), &statBuf);
    if (ret < 0) {
        copyingError = true;
    }
    mode_t mode = statBuf.st_mode;
    int fileSize = statBuf.st_size;

    int fromFileFd = open(fromFileName.c_str(), O_RDONLY);
    if (fromFileFd < 0) {
        copyingError = true;
    }
    // jiacheng start
    // int toFileFd = open(toFileName.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    int toFileFd = open(toFileName.c_str(), O_WRONLY|O_CREAT|O_EXCL, S_IRWXU|S_IRWXG|S_IRWXO);
    // jiacheng end
    if (toFileFd < 0) {
        copyingError = true;
    }
    ret = sendfile(toFileFd, fromFileFd, NULL, fileSize);
    if (ret < 0) {
        copyingError = true;
    }
    ret = close(fromFileFd);
    if (ret < 0) {
        copyingError = true;
    }
    ret = close(toFileFd);
    if (ret < 0) {
        copyingError = true;
    }

    ret = chmod(toFileName.c_str(), mode);
    if (ret < 0) {
        copyingError = true;
    }

    return copyingError;
}

void CompactSwapFile(Thread * self) {
    CHECK(swapEnabled);

    LOG(INFO) << "NIEL compacting swap file";

// jiacheng start
    std::string swapfilePath("/data/data/" + getPackageName() + "/swapfile");
    std::string oldSwapfilePath("/data/data/" + getPackageName() + "/oldswapfile");
    std::string newSwapfilePath("/data/data/" + getPackageName() + "/newswapfile");
    // std::string swapfilePath("/sdcard/zram/" + getPackageName() + "-swapfile");
    // std::string oldSwapfilePath("/sdcard/zram/" + getPackageName() + "-oldswapfile");
    // std::string newSwapfilePath("/sdcard/zram/" + getPackageName() + "-newswapfile");
// jiacheng end
    bool ioError = false;

    freedObjects = 0;
    freedSize = 0;
    swapfileObjects = 0;
    swapfileSize = 0;

    swapFileMutex.ExclusiveLock(self);
    swapfile.close();
    bool copyingError = copyFile(swapfilePath, oldSwapfilePath);
    if (copyingError) {
        LOG(ERROR) << "NIELERROR error copying swap file";
    }
    openFileAppend(swapfilePath, swapfile);
    swapFileMutex.ExclusiveUnlock(self);

    std::fstream oldSwapfile;
    oldSwapfile.open(oldSwapfilePath, std::ios::binary | std::ios::in);

    std::fstream newSwapfile;
    openFile(newSwapfilePath, newSwapfile);

    if (checkStreamError(oldSwapfile, "in oldSwapfile before compaction")) {
        ioError = true;
    }
    if (checkStreamError(newSwapfile, "in newSwapfile before compaction")) {
        ioError = true;
    }

    std::map<void *, std::streampos> newObjectOffsetMap;
    swapFileMapsMutex.SharedLock(self);

    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        if (ioError == true) {
            break;
        }

        void * object = it->first;
        std::streampos oldPos = it->second;
        size_t objectSize = objectSizeMap[object];
        char * objectData = new char[objectSize];

        oldSwapfile.seekg(oldPos);
        oldSwapfile.read(objectData, objectSize);

        std::streampos newPos = newSwapfile.tellp();
        newSwapfile.write(objectData, objectSize);
        newObjectOffsetMap[object] = newPos;
        delete[] objectData;

        swapfileObjects++;
        swapfileSize += objectSize;

        if (checkStreamError(oldSwapfile, "in oldSwapfile during compaction")) {
            ioError = true;
        }
        if (checkStreamError(newSwapfile, "in newSwapfile during compaction")) {
            ioError = true;
        }
    }

    swapFileMapsMutex.SharedUnlock(self);

    oldSwapfile.close();
    newSwapfile.close();

    swapFileMapsMutex.ExclusiveLock(self);
    objectOffsetMap = newObjectOffsetMap;
    swapFileMutex.ExclusiveLock(self);
    swapfile.close();
    remove(swapfilePath.c_str());
    rename(newSwapfilePath.c_str(), swapfilePath.c_str());
    openFileAppend(swapfilePath, swapfile);
    if (checkStreamError(swapfile, "reopening swapfile after compaction")) {
        ioError = true;
    }
    swapFileMutex.ExclusiveUnlock(self);
    swapFileMapsMutex.ExclusiveUnlock(self);

    if (ioError) {
        LOG(INFO) << "NIEL detected errors while compacting swap file";
    }
    else {
        LOG(INFO) << "NIEL finished compacting swap file; new swap file has " << swapfileObjects
                  << " objects, size " << swapfileSize;
    }
}

void CheckAndUpdate(gc::collector::GarbageCollector * gc, mirror::Object * object) {
    if (!swapEnabled) {
        return;
    }
    // jiacheng start
    (void)gc;
    // if (gc->GetGcType() != gc::collector::kGcTypePartial) {
    //     return;
    // }
    // jiacheng end

    Stub * stub = nullptr;
    if (object->GetStubFlag()) {
        stub = (Stub *)object;
        stub->LockTableEntry();
        if (!stub->GetTableEntry()->GetResidentBit()) {
            stub->UnlockTableEntry();
            return;
        }
        object = stub->GetObjectAddress();
    }

    mirror::Object * bookkeepingKey = object;
    if (stub != nullptr) {
        bookkeepingKey = (mirror::Object *)stub;
    }
    CHECK(bookkeepingKey != nullptr);

    object->SetIgnoreReadFlag();
    CHECK(!object->GetStubFlag());
    size_t objectSize = object->SizeOf();
    bool isSwappableType = objectIsSwappableType(object);
    object->ClearIgnoreReadFlag();

    bool wasRead = object->GetReadBit();
    bool wasWritten = object->GetWriteBit();

    //uint8_t rsrVal = object->GetReadShiftRegister();
    uint8_t wsrVal = object->GetWriteShiftRegister();

    gc::Heap * heap = getHeapChecked();
    bool shouldSwapPreliminary =
        objectIsLarge(objectSize)
        && objectIsCold(wsrVal, wasWritten)
        && !object->GetNoSwapFlag()
        && objectInSwappableSpace(heap, object)
        && isSwappableType
    ;

    bool shouldSwap = false;
    if (shouldSwapPreliminary) {
        object->SetIgnoreReadFlag();
        bool isExcluded = shouldExcludeObjectAndMembers(object);
        if (isExcluded) {
            object->SetNoSwapFlag();
            ExcludeVisitor visitor;
            DummyReferenceVisitor referenceVisitor;
            object->VisitReferences(visitor, referenceVisitor);
        }
        object->ClearIgnoreReadFlag();
        shouldSwap = shouldSwapPreliminary && !isExcluded;
    }

    if (shouldSwap) {
        // jiacheng start
        // if (stub == nullptr) {
        //     createStubSet.insert(object);
        // }
        // if (object->GetDirtyBit()) {
        //     Thread * self = Thread::Current();
        //     writeQueueMutex.ExclusiveLock(self);
        //     if (!writeSet.count(bookkeepingKey)) {
        //         writeSet.insert(bookkeepingKey);
        //         writeQueue.push_back(bookkeepingKey);
        //     }
        //     writeQueueMutex.ExclusiveUnlock(self);
        // }

        if (stub == nullptr) {
            createStubSet.insert(object);
            Thread * self = Thread::Current();
            writeQueueMutex.ExclusiveLock(self);
            if (!writeSet.count(bookkeepingKey)) {
                writeSet.insert(bookkeepingKey);
                writeQueue.push_back(bookkeepingKey);
            }
            writeQueueMutex.ExclusiveUnlock(self);
        }
        if (object->GetDirtyBit()) {
            Thread * self = Thread::Current();
            writeQueueMutex.ExclusiveLock(self);
            if (!writeSet.count(bookkeepingKey)) {
                writeSet.insert(bookkeepingKey);
                writeQueue.push_back(bookkeepingKey);
            }
            writeQueueMutex.ExclusiveUnlock(self);
        }
        // jiacheng end
    }

    if (wasRead) {
        object->ClearReadBit();
    }
    if (wasWritten) {
        object->ClearWriteBit();
    }

    object->UpdateReadShiftRegister(wasRead);
    object->UpdateWriteShiftRegister(wasWritten);

    if (stub != nullptr) {
        stub->UnlockTableEntry();
    }
}

void validateSwapFile(Thread * self) {
    bool error = false;

    LOG(INFO) << "NIEL starting swap file validation";

    swapFileMutex.ExclusiveLock(self);
    swapFileMapsMutex.SharedLock(self);

    if (objectOffsetMap.size() != objectSizeMap.size()) {
        LOG(ERROR) << "NIELERROR: objectOffsetMap and objectSizeMap sizes differ";
        error = true;
    }

    std::streampos curPos = swapfile.tellp();
    if (checkStreamError(swapfile, "validation initial tellp()")) {
        error = true;
    }

    for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
        void * object = it->first;
        std::streampos offset = it->second;

        if (!objectSizeMap.count(object)) {
            LOG(ERROR) << "NIELERROR: objectSizeMap does not contain object " << object;
            error = true;
        }

        size_t objectSize = objectSizeMap[object];
        char * objectData = new char[objectSize];

        swapfile.seekg(offset);
        if (checkStreamError(swapfile, "validation seekg()")) {
            error = true;
        }
        swapfile.read(objectData, objectSize);
        if (checkStreamError(swapfile, "validation read()")) {
            error = true;
        }

        if (objectSize < 16) {
            LOG(ERROR) << "NIELERROR: object size " << objectSize << " is too small";
            error = true;
        }
        delete[] objectData;
    }

    swapfile.seekp(curPos);
    if (checkStreamError(swapfile, "validation final seekp()")) {
        error = true;
    }

    swapFileMapsMutex.SharedUnlock(self);
    swapFileMutex.ExclusiveUnlock(self);

    if (error) {
        LOG(ERROR) << "NIELERROR swap file validation detected errors";
    }
    else {
        LOG(INFO) << "NIEL swap file validation successful";
    }
}

void debugPrintDataStructureInfo(Thread * self, const std::string & message) {
  int objectOffsetMapStubCount = 0;
  int objectOffsetMapTotalCount = 0;
  swapFileMapsMutex.SharedLock(self);
  for (auto it = objectOffsetMap.begin(); it != objectOffsetMap.end(); it++) {
    mirror::Object * obj = (mirror::Object *)it->first;
    if (obj->GetStubFlag()) {
      objectOffsetMapStubCount++;
    }
  }
  objectOffsetMapTotalCount = objectOffsetMap.size();
  swapFileMapsMutex.SharedUnlock(self);

  LOG(INFO) << "NIEL (" << message << ") objectOffsetMap contains "
            << objectOffsetMapTotalCount << " total objects, " << objectOffsetMapStubCount
            << " stubs";
}

class DumpObjectVisitor {
  public:
    void operator()(mirror::Object * obj,
                    MemberOffset offset,
                    bool is_static ATTRIBUTE_UNUSED) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        mirror::Object * ref = obj->GetFieldObject<mirror::Object>(offset);
        std::string refString;
        if (ref == nullptr) {
            refString = "null";
        }
        else if (ref->GetStubFlag()) {
            refString = "stub";
        }
        else if (ref->GetClass() == nullptr) {
            refString = "null class";
        }
        else {
            refString = mirror::Class::PrettyClass(ref->GetClass());
        }
        LOG(INFO) << "ref: " << ref << " " << refString;
    }
    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}
    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}
};

class DumpObjectReferenceVisitor {
  public:
  // jiacheng start
    // void operator()(mirror::Class* klass ATTRIBUTE_UNUSED,
    //                 mirror::Reference* ref) const
    //         REQUIRES_SHARED(Locks::mutator_lock_) {
    void operator()(ObjPtr<art::mirror::Class> klass ATTRIBUTE_UNUSED,
                    ObjPtr<art::mirror::Reference> ref) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
  // jiacheng end
        mirror::Object * referent = ref->GetReferent();
        std::string referentString;
        if (referent == nullptr) {
            referentString = "null";
        }
        else if (referent->GetStubFlag()) {
            referentString = "stub";
        }
        else if (referent->GetClass() == nullptr) {
            referentString = "null class";
        }
        else {
            referentString = mirror::Class::PrettyClass(referent->GetClass());
        }
        LOG(INFO) << "reference referent: " << referent << " " << referentString;
    }
};

void dumpObject(mirror::Object * obj) REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(INFO) << "NIEL dump of object @" << obj;
    LOG(INFO) << "size: " << obj->SizeOf();
    LOG(INFO) << "class: "
              << (obj->GetClass() == nullptr ? "null" : mirror::Class::PrettyClass(obj->GetClass()));

    DumpObjectVisitor visitor;
    DumpObjectReferenceVisitor refVisitor;
    obj->VisitReferences(visitor, refVisitor);

    LOG(INFO) << "NIEL end dump of object @" << obj;
}

// jiacheng start

// 对于MS GC，Marvin在PerformHomogeneousSpaceCompact()函数中创建Stubs并且Swap Out
// 本函数与该功能对应，在Concurrent GC的情况下使用 (desired_collector_type == kCollectorTypeCCBackground)。
// 本函数应该在GC之前使用，从而可以对内存进行Compact
void CreateStubsAndSwapOut() {
    Thread* self = Thread::Current();
    gc::Heap* heap = getHeapChecked();
    LOG(INFO) << "jiacheng debug niel_swap.cc CreateStubsAndSwapOut() 1942 "
              << " objectOffsetMap.size()= " << objectOffsetMap.size()
              << " objectSizeMap.size()= " << objectSizeMap.size()
              << " createStubSet.size()= " << createStubSet.size(); 
    heap->WaitForGcToComplete(gc::kGcCauseBackground, self);
    {
        ScopedSuspendAll ssa(__FUNCTION__);
        niel::swap::UnlockAllReclamationTableEntries();
        niel::swap::CreateStubs(self, heap);
        niel::swap::SwapObjectsOut();
    }
    LOG(INFO) << "jiacheng debug niel_swap.cc CreateStubsAndSwapOut() 1953 "
              << " objectOffsetMap.size()= " << objectOffsetMap.size()
              << " objectSizeMap.size()= " << objectSizeMap.size()
              << " createStubSet.size()= " << createStubSet.size(); 
}

// jiacheng end

} // namespace swap
} // namespace niel
} // namespace art