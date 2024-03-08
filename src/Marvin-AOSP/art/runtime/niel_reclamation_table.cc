#include "niel_reclamation_table.h"

#include "base/logging.h"

#include <sys/mman.h>

namespace art {

namespace niel {

namespace swap {

// jiacheng start
// void TableEntry::LockFromAppThread() {
//     while (GetKernelLockBit()) {
//         continue;
//     }
//     IncrAppLockCounter();
//     while (GetKernelLockBit()) {
//         continue;
//     }
// }

// void TableEntry::IncrAppLockCounter() {
//     app_lock_counter_++;
// }

// bool TableEntry::GetBit(unsigned int offset) {
//     return ((bit_flags_.load() >> offset) & 0x1);
// }
// jiacheng end

ReclamationTable ReclamationTable::CreateTable(int numEntries) {
    size_t size = numEntries * sizeof(TableEntry);
    void * baseAddress = mmap(nullptr,
                              size,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS,
                              -1,
                              0);
    if (baseAddress == MAP_FAILED) {
        LOG(ERROR) << "NIELERROR mmap failed during reclamation table creation: "
                   << strerror(errno);
        return ReclamationTable(nullptr, 0);
    }
    return ReclamationTable(baseAddress, numEntries);
}

TableEntry * ReclamationTable::CreateEntry() {
    TableEntry * curEntry = Begin();
    while (curEntry < End()) {
        if (!curEntry->GetOccupiedBit()) {
            // The correctness of this RTE initialization code depends on these
            // assumptions:
            // A) The OS never touches any RTE where the occupied bit is 0.
            // B) Only one app thread at a time calls CreateEntry().
            CHECK(curEntry->GetKernelLockBit() == 0);
            curEntry->ZeroAppLockCounter();
            curEntry->SetNumPages(0);
            curEntry->SetObjectAddress(nullptr);
            curEntry->SetOccupiedBit();
            return curEntry;
        }
        curEntry++;
    }
    return nullptr;
}

void ReclamationTable::FreeEntry(TableEntry * entry) {
    entry->ClearOccupiedBit();
}

bool ReclamationTable::IsValid() {
    return base_address_ != nullptr;
}

void ReclamationTable::UnlockAllEntries() {
    TableEntry * curEntry = Begin();
    while (curEntry < End()) {
        if (curEntry->GetOccupiedBit()) {
            curEntry->ZeroAppLockCounter();
        }
        curEntry++;
    }
}

void ReclamationTable::DebugPrint() {
    LOG(INFO) << "NIELDEBUG ReclamationTable base_address_" << base_address_
              << " num_entries_ " << num_entries_;
    for (TableEntry * curEntry = Begin(); curEntry < End(); curEntry++) {
        LOG(INFO) << "NIELDEBUG " << curEntry
                  << "|" << curEntry->GetOccupiedBit()
                  << "|" << curEntry->GetKernelLockBit()
                  << "|" << (int)curEntry->GetAppLockCounter()
                  << "|" << curEntry->GetNumPages()
                  << "|" << (void *)curEntry->GetObjectAddress();
    }
}

} // namespace swap

} // namespace niel

} // namespace art