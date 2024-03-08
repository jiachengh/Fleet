#ifndef ART_RUNTIME_NIEL_RECLAMATION_TABLE_H_
#define ART_RUNTIME_NIEL_RECLAMATION_TABLE_H_

#include "base/mutex.h"

#include <atomic>
#include <stddef.h>
#include <stdint.h>

namespace art {

namespace mirror {
    class Object;
}

namespace niel {

namespace swap {

const unsigned int OCCUPIED_BIT_OFFSET = 0;
const unsigned int KERNEL_LOCK_BIT_OFFSET = 1;
const unsigned int RESIDENT_BIT_OFFSET = 2;

class TableEntry {
  public:
    NO_INLINE void LockFromAppThread(){
        // jiacheng start
        while (GetKernelLockBit()) {
            continue;
        }
        IncrAppLockCounter();
        while (GetKernelLockBit()) {
            continue;
        }
        IncrAppLockCounter();
        // jiacheng end
    }

    NO_INLINE void UnlockFromAppThread() {
        DecrAppLockCounter();
    }

    NO_INLINE bool GetOccupiedBit() {
        return GetBit(OCCUPIED_BIT_OFFSET);
    }

    NO_INLINE void SetOccupiedBit() {
        SetBit(OCCUPIED_BIT_OFFSET);
    }

    NO_INLINE void ClearOccupiedBit() {
        ClearBit(OCCUPIED_BIT_OFFSET);
    }

    NO_INLINE bool GetKernelLockBit() {
        return GetBit(KERNEL_LOCK_BIT_OFFSET);
    }

    NO_INLINE void SetKernelLockBit() {
        SetBit(KERNEL_LOCK_BIT_OFFSET);
    }

    NO_INLINE void ClearKernelLockBit() {
        ClearBit(KERNEL_LOCK_BIT_OFFSET);
    }

    NO_INLINE bool GetResidentBit() {
        return GetBit(RESIDENT_BIT_OFFSET);
    }

    NO_INLINE void SetResidentBit() {
        SetBit(RESIDENT_BIT_OFFSET);
    }

    NO_INLINE void ClearResidentBit() {
        ClearBit(RESIDENT_BIT_OFFSET);
    }

    NO_INLINE uint8_t GetAppLockCounter() {
        return app_lock_counter_.load();
    }

    NO_INLINE void IncrAppLockCounter() {
        app_lock_counter_++;
    }
    // void IncrAppLockCounter() {
    //     app_lock_counter_++;
    // }

    NO_INLINE void DecrAppLockCounter() {
        app_lock_counter_--;
    }

    NO_INLINE void ZeroAppLockCounter() {
        app_lock_counter_ = 0;
    }

    NO_INLINE uint16_t GetNumPages() {
        return num_pages_;
    }

    NO_INLINE void SetNumPages(uint16_t num) {
        num_pages_ = num;
    }

    NO_INLINE mirror::Object * GetObjectAddress() {
        return reinterpret_cast<mirror::Object *>(object_address_);
    }


    NO_INLINE void SetObjectAddress(mirror::Object * obj) {
        object_address_ = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(obj));
    }

    NO_INLINE TableEntry() {
        stub_back_pointer_ = 0; // prevents the compiler from complaining about an unused variable
    }

  private:
    NO_INLINE bool GetBit(unsigned int offset) {
        return ((bit_flags_.load() >> offset) & 0x1);
    }

    NO_INLINE void SetBit(unsigned int offset) {
        bit_flags_.fetch_or(1 << offset);
    }

    NO_INLINE void ClearBit(unsigned int offset) {
        bit_flags_.fetch_and(~(1 << offset));
    }

    std::atomic<uint8_t> bit_flags_;
    std::atomic<uint8_t> app_lock_counter_;
    uint16_t num_pages_;
    uint32_t object_address_;
    uint32_t stub_back_pointer_; // only used by compiled code
};

class ReclamationTable {
  public:
    static ReclamationTable CreateTable(int numEntries);
    ReclamationTable() : base_address_(nullptr), num_entries_(0) { }
    // CreateEntry() is not thread-safe. Callers should ensure that only one
    // thread at a time calls it.
    TableEntry * CreateEntry();
    void FreeEntry(TableEntry * entry);
    bool IsValid();
    void UnlockAllEntries() REQUIRES(Locks::mutator_lock_);
    void DebugPrint();

    ALWAYS_INLINE TableEntry * Begin() {
        return base_address_;
    }
    ALWAYS_INLINE TableEntry * End() {
        return base_address_ + num_entries_;
    }

  private:
    ReclamationTable(void * base_address, size_t num_entries)
            : base_address_((TableEntry *)base_address), num_entries_(num_entries) { }

    TableEntry * base_address_;
    size_t num_entries_;
};

} // namespace swap
} // namespace niel
} // namespace art

#endif // ART_RUNTIME_NIEL_RECLAMATION_TABLE_H_