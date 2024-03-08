#ifndef ART_RUNTIME_NIEL_STUB_H_
#define ART_RUNTIME_NIEL_STUB_H_

#include <atomic>

#include "base/macros.h"
#include "base/mutex.h"
#include "base/globals.h"

#include "mirror/object_reference.h"

#include "niel_reclamation_table.h"
// jiacheng start
#include "niel_swap.h"
#include "base/bit_utils.h"
// jiacheng end

namespace art {

namespace mirror {
    template <typename MirrorType> class HeapReference;
    class Object;
    class Class;
}

namespace niel {

namespace swap {

class Stub {
  public:
    static size_t GetStubSize(int numRefs) {
        return sizeof(Stub) + numRefs * sizeof(mirror::HeapReference<mirror::Object>);
    }
    
    static size_t GetStubSize(mirror::Object * object) REQUIRES_SHARED(Locks::mutator_lock_) {
        int numRefs = CountReferences(object);
        return GetStubSize(numRefs);
    }

    // jiacheng start
    static constexpr size_t ForwardingAddressOffset() {
        return OFFSETOF_MEMBER(niel::swap::Stub, forwarding_address_);
    }

    static constexpr size_t FlagsOffset() {
        return OFFSETOF_MEMBER(niel::swap::Stub, x_x0_flags_);
    }

    static constexpr size_t PaddingOffset() {
        return OFFSETOF_MEMBER(niel::swap::Stub, padding_b_);
    }

    static constexpr size_t NumRefsOffset() {
        return OFFSETOF_MEMBER(niel::swap::Stub, num_refs_);
    }

    static constexpr size_t TableEntryOffset() {
        return OFFSETOF_MEMBER(niel::swap::Stub, table_entry_);
    }
    
    template <typename Visitor,
              typename JavaLangRefVisitor,
              typename StubVisitor>
    NO_INLINE void VisitReferences(const Visitor& visitor, const JavaLangRefVisitor& ref_visitor, const StubVisitor& stub_visitor) NO_THREAD_SAFETY_ANALYSIS {
        (void)visitor;
        (void)ref_visitor;
        LockTableEntry();
        for (int i = 0; i < GetNumRefs(); i++) {
            mirror::Object* ref = GetReference(i);
            stub_visitor(ref);
        }
        UnlockTableEntry();
    }

    void CopyHeadFrom(Stub* from_ref);

    // jiacheng end

    void PopulateFrom(mirror::Object * object) REQUIRES_SHARED(Locks::mutator_lock_);

    // jiacheng start
    void CopyMetaData(mirror::Object * object) REQUIRES_SHARED(Locks::mutator_lock_);
    // jiacheng end

    void CopyRefsInto(mirror::Object * object) REQUIRES_SHARED(Locks::mutator_lock_);

    NO_INLINE void SetReference(int pos, mirror::Object * ref) REQUIRES_SHARED(Locks::mutator_lock_) {
        GetReferenceAddress(pos)->Assign(ref);
    }

    NO_INLINE mirror::Object * GetReference(int pos) REQUIRES_SHARED(Locks::mutator_lock_) {
        return GetReferenceAddress(pos)->AsMirrorPtr();
    }
    
    NO_INLINE mirror::HeapReference<mirror::Object> * GetReferenceAddress(int pos)
            REQUIRES_SHARED(Locks::mutator_lock_) {
        char * refBytePtr = (char *)this + sizeof(Stub)
                                        + pos * sizeof(mirror::HeapReference<mirror::Object>);
        return (mirror::HeapReference<mirror::Object> *)refBytePtr;
    }

    // Copied from mirror/object.h
    NO_INLINE bool GetStubFlag() {
        return (bool)GetBitsAtomic8(x_x0_flags_, 7, 1, std::memory_order_acquire);
    }

    NO_INLINE size_t GetSize() {
        return GetStubSize(num_refs_);
    }

    void RawDump();

    void SemanticDump() REQUIRES_SHARED(Locks::mutator_lock_);

    NO_INLINE int GetNumRefs() { return num_refs_; }

    NO_INLINE uint32_t GetForwardingAddress() { return forwarding_address_; }
    NO_INLINE void SetForwardingAddress(uint32_t addr) { forwarding_address_ = addr; }

    NO_INLINE mirror::Object * GetObjectAddress() {
        return GetTableEntry()->GetObjectAddress();
    }

    NO_INLINE void SetObjectAddress(mirror::Object * obj) {
        GetTableEntry()->SetObjectAddress(obj);
    }

    NO_INLINE void LockTableEntry() {
        // jiacheng start
        // GetTableEntry()->LockFromAppThread();
        TableEntry* table_entry = GetTableEntry();
        if (table_entry) {
            table_entry->LockFromAppThread();
        }
        // jiacheng end
    }

    NO_INLINE void UnlockTableEntry() {
        // jiacheng start
        // GetTableEntry()->UnlockFromAppThread();
        TableEntry* table_entry = GetTableEntry();
        if (table_entry) {
            table_entry->UnlockFromAppThread();
        }
        // jiacheng end
    }

    NO_INLINE TableEntry * GetTableEntry() {
        // jiacheng start
        // return reinterpret_cast<TableEntry *>(table_entry_);
        TableEntry * table_entry = reinterpret_cast<TableEntry *>(table_entry_);
        if (table_entry >= GetReclamationTable()->End()) {
            uint64_t table_begin = reinterpret_cast<uint64_t>(GetReclamationTable()->Begin());
            table_begin = table_begin & 0xffffffff00000000;
            table_entry_ = table_entry_ & 0x00000000ffffffff;
            table_entry_ = table_entry_ | table_begin;
            table_entry = reinterpret_cast<TableEntry *>(table_entry_);
        }
        
        CHECK(table_entry < GetReclamationTable()->End()) << " table_entry= " << table_entry 
                                                          << " GetReclamationTable()->End()= " << GetReclamationTable()->End()
                                                          << " this= " << this; 
        return table_entry;
        // jiacheng end

    }

    NO_INLINE void SetTableEntry(TableEntry * entry) {
        // jiacheng start
        // table_entry_ = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry));
        CHECK(entry < GetReclamationTable()->End()) << " table_entry= " << entry 
                                                    << " GetReclamationTable()->End()= " << GetReclamationTable()->End()
                                                    << " this= " << this; 
        table_entry_ = reinterpret_cast<uint64_t>(entry);
        // jiacheng end
    }

    // jiacheng start
    // template <bool kVisitNativeRoots = true,
    //             VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
    //             ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
    //             typename Visitor,
    //             typename JavaLangRefVisitor = VoidFunctor>
    // void VisitReferences(const Visitor& visitor, const JavaLangRefVisitor& ref_visitor) NO_THREAD_SAFETY_ANALYSIS {
    //     (void)ref_visitor;
    //     for (int i = 0; i < GetNumRefs(); i++) {
    //         mirror::Object* ref = GetReference(i);
    //         MemberOffset offset = static_cast<MemberOffset>(size_t(ref) - size_t(this));
    //         visitor(this, offset, /* is_static= */ false);
    //     }
    // }
    // jiacheng end

  private:
    // Copied from mirror/object.h
    static uint8_t GetBitsAtomic8(const std::atomic<uint8_t> & data, uint8_t offset,
                                  uint8_t width, std::memory_order order) {
        return (data.load(order) >> offset) & (0xff >> (8 - width));
    }

    static void SetBitsAtomic8(std::atomic<uint8_t> & data, uint8_t offset, uint8_t width,
                               std::memory_order order) {
        data.fetch_or((0xff >> (8 - width) << offset), order);
    }

    static void ClearBitsAtomic8(std::atomic<uint8_t> & data, uint8_t offset, uint8_t width,
                                 std::memory_order order) {
        data.fetch_and(~((0xff >> (8 - width)) << offset), order);
    }

    static int CountReferences(mirror::Object * object)
            REQUIRES_SHARED(Locks::mutator_lock_);

    NO_INLINE void SetStubFlag() {
        SetBitsAtomic8(x_x0_flags_, 7, 1, std::memory_order_acq_rel);
    }

    NO_INLINE void ClearFlags() {
        ClearBitsAtomic8(x_x0_flags_, 0, 8, std::memory_order_acq_rel);
    }
    // jiacheng start
    mirror::HeapReference<mirror::Class> klass_;
    uint32_t monitor_ ATTRIBUTE_UNUSED;
    // jiacheng end

    /*
     * x_flags_ layout:
     * 7|6|543210
     * s|
     *
     * s: stub flag
     */

    uint32_t forwarding_address_;

    std::atomic<uint8_t> x_x0_flags_;
    uint8_t padding_b_;
    uint16_t num_refs_;

    // The entry corresponding to the object represented by this stub in the
    // reclamation table.
    uint64_t table_entry_;
};

// Used as entry point for compiled code
void PopulateStub(Stub * stub, mirror::Object * object) REQUIRES_SHARED(Locks::mutator_lock_);

} // namespace swap
} // namespace marvin
} // namespace art




#endif // ART_RUNTIME_NIEL_STUB_H_