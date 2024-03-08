#include "niel_stub.h"

#include <iostream>


#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "mirror/object_reference.h"
#include "niel_stub-inl.h"

#include "mirror/object-refvisitor-inl.h"

namespace art {

namespace niel {

namespace swap {

// Based on MarkVisitor in runtime/gc/collector/mark_sweep.cc
//
// TODO: Make sure none of these things cause correctness issues:
//     1) Using a mutable variable modified from a const function to track the current offset
//     2) Assuming Object::VisitReferences() visits references in a deterministic order
//     3) Assuming the object is not modified during visiting
class StubPopulateVisitor {
  public:
    StubPopulateVisitor(Stub * stub) : stub_(stub), cur_ref_(0) {}

    void operator()(mirror::Object * obj,
                    MemberOffset offset,
                    bool is_static ATTRIBUTE_UNUSED) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        CHECK_LT((int)cur_ref_, stub_->GetNumRefs());
        stub_->SetReference(cur_ref_, obj->GetFieldObject<mirror::Object>(offset));
        cur_ref_++;
    }

    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

  private:
    Stub * stub_;
    mutable uint32_t cur_ref_;
};

// Method signature from MarkSweep::DelayReferenceReferentVisitor in
// runtime/gc/collector/mark_sweep.cc.
//
// TODO: Make sure this class is not actually being used when populating stubs
//       or copying refs into objects
class DummyReferenceVisitor {
  public:
    void operator()(ObjPtr<art::mirror::Class> klass ATTRIBUTE_UNUSED,
                    ObjPtr<art::mirror::Reference> ref ATTRIBUTE_UNUSED) const {}
};

// Based on MarkVisitor in runtime/gc/collector/mark_sweep.cc
//
// TODO: Make sure none of these things cause correctness issues:
//     1) Using a mutable variable modified from a const function to track the current offset
//     2) Assuming Object::VisitReferences() visits references in a deterministic order
class CopyRefsVisitor {
  public:
    CopyRefsVisitor(Stub * stub) : stub_(stub), cur_ref_(0) {}

    void operator()(mirror::Object * obj,
                    MemberOffset offset,
                    bool is_static ATTRIBUTE_UNUSED) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        if (obj->GetFieldObject<mirror::Object>(offset) != stub_->GetReference(cur_ref_)) {
            obj->SetFieldObjectWithoutWriteBarrier<false>(offset, stub_->GetReference(cur_ref_));
        }
        cur_ref_++;
    }

    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

  private:
    Stub * stub_;
    mutable uint32_t cur_ref_;
};

void Stub::PopulateFrom(mirror::Object * object) {
    ClearFlags();
    SetStubFlag();
    SetObjectAddress(object);
    forwarding_address_ = 0;
    padding_b_ = 0;

    // jiacheng start
    memcpy(reinterpret_cast<uint8_t*>(this),
           reinterpret_cast<const uint8_t*>(object),
           sizeof(uint32_t) + sizeof(uint32_t));
    // jiacheng end

    num_refs_ = CountReferences(object);

    for (int i = 0; i < num_refs_; i++) {
        GetReferenceAddress(i)->Assign(nullptr);
    }

    StubPopulateVisitor visitor(this);
    DummyReferenceVisitor dummyVisitor;
    object->VisitReferences(visitor, dummyVisitor);
}

// jiacheng start
void Stub::CopyMetaData(mirror::Object * object) {
    memcpy(reinterpret_cast<uint8_t*>(object),
         reinterpret_cast<const uint8_t*>(this),
         sizeof(uint32_t) + sizeof(uint32_t));
}
// jiacheng end

void Stub::CopyRefsInto(mirror::Object * object) {
    CopyRefsVisitor visitor(this);
    DummyReferenceVisitor dummyVisitor;
    object->VisitReferences(visitor, dummyVisitor);
}


// jiacheng start
class CountReferencesVisitor {
public:
    CountReferencesVisitor(): count_(0) {}

    void operator()(mirror::Object * obj ATTRIBUTE_UNUSED,
                    MemberOffset offset ATTRIBUTE_UNUSED,
                    bool is_static ATTRIBUTE_UNUSED) const
            REQUIRES_SHARED(Locks::mutator_lock_) {
        ++count_;
    }

    void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

    size_t GetCount() {
        return count_;
    }

private:
    mutable size_t count_;
};


void Stub::CopyHeadFrom(Stub* from_ref) {
    forwarding_address_ = from_ref->forwarding_address_;
    x_x0_flags_ = from_ref->x_x0_flags_.load();
    padding_b_ = from_ref->padding_b_;
    num_refs_ = from_ref->num_refs_;
    table_entry_ = from_ref->table_entry_;
}


// jiacheng end

int Stub::CountReferences(mirror::Object * object) {
    // jiacheng start
    // Copied from marvin_instrumentation.cc

    // object->SetIgnoreReadFlag();
    // mirror::Class * klass = object->GetClass();
    // object->ClearIgnoreReadFlag();
    // uint32_t numPointers = klass->NumReferenceInstanceFields();
    // ObjPtr<art::mirror::Class> superClass = klass->GetSuperClass();
    // while (superClass != nullptr) {
    //     numPointers += superClass->NumReferenceInstanceFields();
    //     superClass = superClass->GetSuperClass();
    // }
    // return numPointers;

    CountReferencesVisitor visitor;
    DummyReferenceVisitor dummyVisitor;
    
    object->SetIgnoreReadFlag();
    object->VisitReferences(visitor, dummyVisitor);
    object->ClearIgnoreReadFlag();
    return visitor.GetCount();
    // jiacheng end
}

void Stub::RawDump() {
    LOG(INFO) << "MARVIN raw dump for stub @" << this;
    size_t stubSize = GetSize();
    char * stubData = (char *)this;
    for (size_t i = 0; i < stubSize; i++) {
        LOG(INFO) << i << ": " << std::hex << (int)stubData[i];
    }
    LOG(INFO) << "MARVIN end raw dump for stub @" << this;
}

void Stub::SemanticDump() {
    LOG(INFO) << "MARVIN semantic dump for stub @" << this;
    LOG(INFO) << "table_entry_: " << std::hex << table_entry_;
    LOG(INFO) << "forwarding_address_: " << std::hex << forwarding_address_;
    LOG(INFO) << "stub flag: "<< GetStubFlag();
    LOG(INFO) << "num_refs_: " << num_refs_;
    for (int i = 0; i < GetNumRefs(); i++) {
        std::string refString;
        mirror::Object * ref = GetReference(i);
        if (ref == nullptr) {
            refString = "null";
        }
        else {
            if (ref->GetStubFlag()) {
                refString = "stub";
            }
            else {
                mirror::Class * klass = ref->GetClass();
                if (klass == nullptr) {
                    refString = "null class";
                }
                refString = klass->PrettyClass();
            }
        }
        LOG(INFO) << "ref " << i << ": " << ref << " " << refString;
    }
    LOG(INFO) << "MARVIN end semantic dump for stub @" << this;
}

void PopulateStub(Stub * stub, mirror::Object * object) {
    stub->PopulateFrom(object);
}

} // namespace swap
} // namespace marvin
} // namespace art
