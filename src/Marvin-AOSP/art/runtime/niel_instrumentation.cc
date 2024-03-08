#include "niel_instrumentation.h"

#include "gc/allocator/rosalloc.h"
#include "gc/heap.h"
#include "gc/space/rosalloc_space.h"
#include "gc/space/space.h"
#include "base/globals.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "thread.h"

#include <cstring>
#include <map>

// jiacheng start
#include "gc/collector/garbage_collector.h"
// jiacheng end

#include "niel_bivariate_histogram.h"
#include "niel_common.h"
#include "niel_histogram.h"
#include "niel_swap.h"

namespace art {

namespace niel {

namespace inst {


/* Constants */
const int LOG_INTERVAL_SECONDS = 10;

/* Internal functions */
void maybePrintLog();
void printAllocCounts();
void printHeap();

/* Variables */
Mutex instMutex("NielInstrumentationMutex", kLoggingLock);
time_t lastLogTime;

/*     Locked with instMutex */
long numTotalRosAllocThreadLocalAllocs = 0;
long numTotalRosAllocNormalAllocs = 0;
long numTotalRosAllocLargeObjectAllocs = 0;

long sizeTotalRosAllocThreadLocalAllocs = 0;
long sizeTotalRosAllocNormalAllocs = 0;
long sizeTotalRosAllocLargeObjectAllocs = 0;

long numCurrentRosAllocAllocs = 0;
long numCurrentRosAllocLargeObjectAllocs = 0;

long sizeCurrentRosAllocAllocs = 0;
long sizeCurrentRosAllocLargeObjectAllocs = 0;

std::map<std::string, int> currentAllocCounts;
std::map<std::string, int> currentAllocSizes;

std::map<std::string, int> totalAllocCounts;
std::map<std::string, int> totalAllocSizes;

/*     End locked with instMutex */
// jiacheng start
std::set<mirror::Object*> access_count_set;
// jiacheng end

long objectsRead = 0;
long objectsWritten = 0;
long objectsReadAndWritten = 0;
long totalObjects = 0;
long shenanigansCount = 0; // used for misc debugging

long readTotalObjectSize = 0;
long readTotalPointerSize = 0;
long unreadTotalObjectSize = 0;
long unreadTotalPointerSize = 0;
long smallObjectTotalObjectSize = 0;
long smallObjectTotalPointerSize = 0;
long largeObjectTotalObjectSize = 0;
long largeObjectTotalPointerSize = 0;

long coldObjectTotalSize = 0;
long largeColdObjectTotalSize = 0;

long noSwapFlagTotalSize = 0;
long notInSpaceTotalSize = 0;
long notSwappableTypeTotalSize = 0;

long numStubs = 0;

// These numbers may duplicate the info from the cold object size numbers
// tracked by variables above, but they let me more easily tweak which objects
// are counted in the working set and add multiple definitions of "working set"
// if necessary.
long sizeWorkingSetRead = 0;
long sizeWorkingSetWrite = 0;

uint32_t maxPointerCount4KB = 0;
double maxPointerFrac4KB = 0;

LogHistogram objectSizeHist(2, 4, 25);
LogHistogram objectSizeTotalMemoryCounts(2, 4, 25);
LogHistogram pointerFrac100KBObjectHist(1.5, -40, -23);

LinearHistogram smallObjectPointerFracHist(10, 0, 1); // objects <=200 bytes
LinearHistogram largeObjectPointerFracHist(10, 0, 1); // objects >200 bytes
LinearHistogram readShiftRegHist(16, 0, 16);
LinearHistogram writeShiftRegHist(16, 0, 16);

BivariateHistogram readShiftRegVsPointerFracHist(16, 0, 16, 10, 0, 1);
BivariateHistogram writeShiftRegVsPointerFracHist(16, 0, 16, 10, 0, 1);
BivariateHistogram objectSizeVsPointerFracHist(10, 0, 1000, 10, 0, 1);

void RecordRosAllocAlloc(Thread * self, size_t size, RosAllocAllocType type) {
    instMutex.ExclusiveLock(self);

    switch(type) {
      case ROSALLOC_ALLOC_THREAD_LOCAL:
        numTotalRosAllocThreadLocalAllocs++;
        sizeTotalRosAllocThreadLocalAllocs += size;
        numCurrentRosAllocAllocs++;
        sizeCurrentRosAllocAllocs += size;
        break;
      case ROSALLOC_ALLOC_NORMAL:
        numTotalRosAllocNormalAllocs++;
        sizeTotalRosAllocNormalAllocs += size;
        numCurrentRosAllocAllocs++;
        sizeCurrentRosAllocAllocs += size;
        break;
      case ROSALLOC_ALLOC_LARGE:
        numTotalRosAllocLargeObjectAllocs++;
        sizeTotalRosAllocLargeObjectAllocs += size;
        numCurrentRosAllocLargeObjectAllocs++;
        sizeCurrentRosAllocLargeObjectAllocs += size;
        break;
    }

    instMutex.ExclusiveUnlock(self);
    maybePrintLog();
}

void RecordRosAllocFree(Thread * self, size_t size, RosAllocFreeType type) {
    instMutex.ExclusiveLock(self);

    switch(type) {
      case ROSALLOC_FREE_NORMAL_OR_THREAD_LOCAL:
        numCurrentRosAllocAllocs--;
        sizeCurrentRosAllocAllocs -= size;
        break;
      case ROSALLOC_FREE_LARGE:
        numCurrentRosAllocLargeObjectAllocs--;
        sizeCurrentRosAllocLargeObjectAllocs -= size;
        break;
    }
    instMutex.ExclusiveUnlock(self);
    maybePrintLog();
}

void RecordAlloc(Thread * self, gc::space::Space * space, size_t size) {
    instMutex.ExclusiveLock(self);

    std::string name(space->GetName());
    currentAllocCounts[name]++;
    currentAllocSizes[name] += size;
    totalAllocCounts[name]++;
    totalAllocSizes[name] += size;

    instMutex.ExclusiveUnlock(self);
    maybePrintLog();
}

void RecordFree(Thread * self, gc::space::Space * space, size_t size, int count) {
    instMutex.ExclusiveLock(self);

    std::string name(space->GetName());
    currentAllocCounts[name] -= count;
    currentAllocSizes[name] -= size;

    instMutex.ExclusiveUnlock(self);
    maybePrintLog();
}

void StartAccessCount(gc::collector::GarbageCollector * gc) {
    if (gc->GetGcType() != gc::collector::kGcTypePartial) {
        return;
    }
    // jiacheng start
    access_count_set.clear();
    // jiacheng end
    objectsRead = 0;
    objectsWritten = 0;
    objectsReadAndWritten = 0;
    totalObjects = 0;
    shenanigansCount = 0;

    readTotalObjectSize = 0;
    readTotalPointerSize = 0;
    unreadTotalObjectSize = 0;
    unreadTotalPointerSize = 0;
    smallObjectTotalObjectSize = 0;
    smallObjectTotalPointerSize = 0;
    largeObjectTotalObjectSize = 0;
    largeObjectTotalPointerSize = 0;

    objectSizeHist.Clear();
    objectSizeTotalMemoryCounts.Clear();
    pointerFrac100KBObjectHist.Clear();

    smallObjectPointerFracHist.Clear();
    largeObjectPointerFracHist.Clear();
    readShiftRegHist.Clear();
    writeShiftRegHist.Clear();

    readShiftRegVsPointerFracHist.Clear();
    writeShiftRegVsPointerFracHist.Clear();
    objectSizeVsPointerFracHist.Clear();

    coldObjectTotalSize = 0;
    largeColdObjectTotalSize = 0;

    noSwapFlagTotalSize = 0;
    notInSpaceTotalSize = 0;
    notSwappableTypeTotalSize = 0;

    numStubs = 0;

    sizeWorkingSetRead = 0;
    sizeWorkingSetWrite = 0;

    maxPointerCount4KB = 0;
    maxPointerFrac4KB = 0;
}

void CountAccess(gc::collector::GarbageCollector * gc, mirror::Object * object) {
    if (gc->GetGcType() != gc::collector::kGcTypePartial) {
        return;
    }
    // jiacheng start
    if (access_count_set.find(object) != access_count_set.end()) {
        return;
    }
    access_count_set.insert(object);
    // jiacheng end
    
    if (object->GetStubFlag()) {
        numStubs++;
        niel::swap::Stub * stub = (niel::swap::Stub *)object;
        if (stub->GetTableEntry()->GetResidentBit()) {
            CountAccess(gc, stub->GetObjectAddress());
        }
        return;
    }

    object->SetIgnoreReadFlag();
    size_t objectSize = object->SizeOf();
    bool isSwappableType = objectIsSwappableType(object);
    // jiacheng start
    // mirror::Class * klass = object->GetClass();
    ObjPtr<art::mirror::Class> klass = object->GetClass();
    // jiacheng end
    object->ClearIgnoreReadFlag();
    uint32_t numPointers = klass->NumReferenceInstanceFields();
    // jiacheng start
    // mirror::Class * superClass = klass->GetSuperClass();
    ObjPtr<art::mirror::Class> superClass = klass->GetSuperClass();
    // jiacheng end
    while (superClass != nullptr) {
        numPointers += superClass->NumReferenceInstanceFields();
        superClass = superClass->GetSuperClass();
    }
    size_t sizeOfPointers = numPointers * sizeof(mirror::HeapReference<mirror::Object>);
    double pointerSizeFrac = ((double)sizeOfPointers) / objectSize;


    // jiacheng start
    // if (objectSize > 200) {
    if (objectIsLarge(objectSize)) {
    // jiacheng end
        largeObjectPointerFracHist.Add(pointerSizeFrac);
        largeObjectTotalPointerSize += sizeOfPointers;
        largeObjectTotalObjectSize += objectSize;
    }
    else {
        smallObjectPointerFracHist.Add(pointerSizeFrac);
        smallObjectTotalPointerSize += sizeOfPointers;
        smallObjectTotalObjectSize += objectSize;
    }

    bool wasRead = object->GetReadBit();
    bool wasWritten = object->GetWriteBit();

    if (wasRead) {
        objectsRead += 1;
        readTotalPointerSize += sizeOfPointers;
        readTotalObjectSize += objectSize;
    }
    else {
        unreadTotalPointerSize += sizeOfPointers;
        unreadTotalObjectSize += objectSize;
    }

    if (wasWritten) {
        objectsWritten += 1;
    }

    if (wasRead && wasWritten) {
        objectsReadAndWritten += 1;
    }

    uint8_t rsrVal = object->GetReadShiftRegister();
    uint8_t wsrVal = object->GetWriteShiftRegister();

    objectSizeHist.Add(objectSize);
    objectSizeTotalMemoryCounts.AddMultiple(objectSize, objectSize);
    if (objectSize >= 100 * 1024) {
        pointerFrac100KBObjectHist.Add(pointerSizeFrac);
    }

    if (objectSize >= 4 * 1024) {
        if (numPointers > maxPointerCount4KB) {
            maxPointerCount4KB = numPointers;
        }
        if (pointerSizeFrac > maxPointerFrac4KB) {
            maxPointerFrac4KB = pointerSizeFrac;
        }
    }

    readShiftRegHist.Add(rsrVal);
    writeShiftRegHist.Add(wsrVal);

    readShiftRegVsPointerFracHist.Add(rsrVal, pointerSizeFrac);
    writeShiftRegVsPointerFracHist.Add(wsrVal, pointerSizeFrac);
    objectSizeVsPointerFracHist.Add(objectSize, pointerSizeFrac);

    if (objectIsCold(wsrVal, wasWritten)) {
        coldObjectTotalSize += objectSize;
        if (objectIsLarge(objectSize)) {
            largeColdObjectTotalSize += objectSize;
            if (object->GetNoSwapFlag()) {
                noSwapFlagTotalSize += objectSize;
            }
            gc::Heap * heap = getHeapChecked();
            if (!swap::objectInSwappableSpace(heap, object)) {
                notInSpaceTotalSize += objectSize;
            }
            if (!isSwappableType) {
                notSwappableTypeTotalSize += objectSize;
            }
        }
    }

    if (wsrVal >= 2 || wasWritten) {
        sizeWorkingSetWrite += objectSize;
    }
    if (rsrVal >= 2 || wasRead) {
        sizeWorkingSetRead += objectSize;
    }

    totalObjects += 1;
}

void FinishAccessCount(gc::collector::GarbageCollector * gc) {
    if (gc->GetGcType() != gc::collector::kGcTypePartial) {
        return;
    }

    LOG(INFO) << "NIEL (GC " << gc->GetName() << "): objects read: " << objectsRead
              << " objects written: " << objectsWritten << " objects read and written: "
              << objectsReadAndWritten << " total objects: " << totalObjects
              << " shenanigans: " << shenanigansCount;
    LOG(INFO) << "NIEL total pointer frac of read objects: "
              << (double)readTotalPointerSize / readTotalObjectSize
              << " pointer size: " << readTotalPointerSize
              << " object size: " << readTotalObjectSize;
    LOG(INFO) << "NIEL unread total pointer frac: "
              << (double)unreadTotalPointerSize / unreadTotalObjectSize
              << " pointer size: " << unreadTotalPointerSize
              << " object size: " << unreadTotalObjectSize;
    LOG(INFO) << "NIEL small total pointer frac: "
              << (double)smallObjectTotalPointerSize / smallObjectTotalObjectSize
              << " pointer size: " << smallObjectTotalPointerSize
              << " object size: " << smallObjectTotalObjectSize;
    LOG(INFO) << "NIEL large total pointer frac: "
              << (double)largeObjectTotalPointerSize / largeObjectTotalObjectSize
              << " pointer size: " << largeObjectTotalPointerSize
              << " object size: " << largeObjectTotalObjectSize;
    LOG(INFO) << "NIEL cold object size: " << coldObjectTotalSize
              << " large cold object size: " << largeColdObjectTotalSize
              << " large object size: " << largeObjectTotalObjectSize
              << " total object size: "
              << (smallObjectTotalObjectSize + largeObjectTotalObjectSize);
    LOG(INFO) << "NIEL size of swappable objects with NoSwapFlagSet: " << noSwapFlagTotalSize;
    LOG(INFO) << "NIEL size of swappable objects not in RosAlloc or large object space or region space: "
              << notInSpaceTotalSize;
    LOG(INFO) << "NIEL size of swappable objects of invalid type: " << notSwappableTypeTotalSize;
    LOG(INFO) << "NIEL num stubs " << numStubs;

    LOG(INFO) << "NIEL read working set size: " << sizeWorkingSetRead;
    LOG(INFO) << "NIEL write working set size: " << sizeWorkingSetWrite;

/*
    LOG(INFO) << "NIEL max pointer count in object >= 4KB: " << maxPointerCount4KB;
    LOG(INFO) << "NIEL max pointer frac in object >= 4KB: " << maxPointerFrac4KB;
    
    LOG(INFO) << "NIEL object size hist (scaled): \n" << objectSizeHist.Print(true, true);
    LOG(INFO) << "NIEL object size total memory counts (scaled): \n"
              << objectSizeTotalMemoryCounts.Print(true, true);
    LOG(INFO) << "NIEL pointer frac hist of objects >= 100KB (scaled):\n"
              << pointerFrac100KBObjectHist.Print(true, true);
*/

/*
    LOG(INFO) << "NIEL pointer frac hist of small objects (scaled):\n"
              << smallObjectPointerFracHist.Print(true, true);
    LOG(INFO) << "NIEL pointer frac hist of large objects (scaled):\n"
              << largeObjectPointerFracHist.Print(true, true);
    LOG(INFO) << "NIEL read shift register hist:\n" << readShiftRegHist.Print(false, true);
    LOG(INFO) << "NIEL write shift register hist:\n" << writeShiftRegHist.Print(false, true);
    LOG(INFO) << "NIEL read shift reg vs pointer frac hist:\n"
              << readShiftRegVsPointerFracHist.Print(false);
    LOG(INFO) << "NIEL write shift reg vs pointer frac hist:\n"
              << writeShiftRegVsPointerFracHist.Print(false);
    LOG(INFO) << "NIEL object size vs pointer frac hist:\n"
              << objectSizeVsPointerFracHist.Print(false);
*/
}

void maybePrintLog() {
    time_t currentTime = time(NULL);
    if (difftime(currentTime, lastLogTime) > LOG_INTERVAL_SECONDS) {
        printAllocCounts();
        printHeap();
        lastLogTime = currentTime;
    }
}

void printAllocCounts() {
    for (auto it = currentAllocCounts.begin(); it != currentAllocCounts.end(); it++) {
        std::string name = it->first;
        LOG(INFO) << "NIEL space |" << name << "| curr count: " << currentAllocCounts[name]
                  << " curr size: " << currentAllocSizes[name] << " total count: "
                  << totalAllocCounts[name] << " total size: " << totalAllocSizes[name]
                  ;
    }
    LOG(INFO) << "NIEL RosAlloc curr thread-local/normal count: " << numCurrentRosAllocAllocs
              << " size: " << sizeCurrentRosAllocAllocs
              << " curr large count: " << numCurrentRosAllocLargeObjectAllocs
              << " size: " << sizeCurrentRosAllocLargeObjectAllocs
              ;
    LOG(INFO) << "NIEL RosAlloc total thread-local count: " << numTotalRosAllocThreadLocalAllocs
              << " size: " << sizeTotalRosAllocThreadLocalAllocs
              << " total normal count: " << numTotalRosAllocNormalAllocs
              << " size: " << sizeTotalRosAllocNormalAllocs
              << " total large count: " << numTotalRosAllocLargeObjectAllocs
              << " size: " << sizeTotalRosAllocLargeObjectAllocs
              ;
}

void printHeap() {
    gc::Heap * heap = getHeapChecked();
    if (heap == NULL) {
        return;
    }

    LOG(INFO) << "NIEL num spaces " << (heap->nielinst_spaces_.size());
    for (auto it = heap->nielinst_spaces_.begin(); it != heap->nielinst_spaces_.end(); it++) {
        LOG(INFO) << "NIEL space " << (*it)->GetName()
                  << " continuous? " << (*it)->IsContinuousSpace()
                  << " discontinuous? " << (*it)->IsDiscontinuousSpace()
                  << " alloc? " << (*it)->IsAllocSpace()
                  ;
    }

    LOG(INFO) << "NIEL num garbage collectors " << heap->nielinst_GetGarbageCollectors()->size();
    for (auto it = heap->nielinst_GetGarbageCollectors()->begin();
            it != heap->nielinst_GetGarbageCollectors()->end(); it++) {
        LOG(INFO) << "NIEL garbage collector " << (*it)->GetName()
                  << " semi_space? " << ((*it) == heap->nielinst_GetSemiSpace())
                //   << " mark_compact? " << ((*it) == heap->nielinst_GetMarkCompact())
                  << " concurrent_copying? " << ((*it) == heap->nielinst_GetConcurrentCopying())
                  ;
    }

    gc::space::RosAllocSpace * rosAllocSpace = heap->GetRosAllocSpace();
    if (rosAllocSpace != NULL) {
        gc::allocator::RosAlloc * rosAlloc = rosAllocSpace->GetRosAlloc();
        if (rosAlloc != NULL) {
            LOG(INFO) << "NIEL RosAlloc footprint: " << rosAlloc->Footprint();
        }
    }
}

} // namespace inst
} // namespace niel
} // namespace art