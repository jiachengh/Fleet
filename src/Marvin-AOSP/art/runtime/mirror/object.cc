/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctime>

#include "object.h"

#include "array-inl.h"
#include "art_field-inl.h"
#include "art_field.h"
#include "class-inl.h"
#include "class.h"
#include "class_linker-inl.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap-inl.h"
#include "handle_scope-inl.h"
#include "iftable-inl.h"
#include "monitor.h"
#include "object-inl.h"
#include "object-refvisitor-inl.h"
#include "object_array-inl.h"
#include "runtime.h"
#include "throwable.h"
#include "well_known_classes.h"

namespace art {
namespace mirror {

Atomic<uint32_t> Object::hash_code_seed(987654321U + std::time(nullptr));

class CopyReferenceFieldsWithReadBarrierVisitor {
 public:
  explicit CopyReferenceFieldsWithReadBarrierVisitor(ObjPtr<Object> dest_obj)
      : dest_obj_(dest_obj) {}

  void operator()(ObjPtr<Object> obj, MemberOffset offset, bool /* is_static */) const
      ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    // GetFieldObject() contains a RB.
    ObjPtr<Object> ref = obj->GetFieldObject<Object>(offset);
    // No WB here as a large object space does not have a card table
    // coverage. Instead, cards will be marked separately.
    dest_obj_->SetFieldObjectWithoutWriteBarrier<false, false>(offset, ref);
  }

  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    // Copy java.lang.ref.Reference.referent which isn't visited in
    // Object::VisitReferences().
    DCHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), false);
  }

  // Unused since we don't copy class native roots.
  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED)
      const {}
  void VisitRoot(mirror::CompressedReference<mirror::Object>* root ATTRIBUTE_UNUSED) const {}

 private:
  const ObjPtr<Object> dest_obj_;
};

ObjPtr<Object> Object::CopyObject(ObjPtr<mirror::Object> dest,
                                  ObjPtr<mirror::Object> src,
                                  size_t num_bytes) {
  // Copy instance data.  Don't assume memcpy copies by words (b/32012820).
  {
    const size_t offset = sizeof(Object);
    uint8_t* src_bytes = reinterpret_cast<uint8_t*>(src.Ptr()) + offset;
    uint8_t* dst_bytes = reinterpret_cast<uint8_t*>(dest.Ptr()) + offset;
    num_bytes -= offset;
    DCHECK_ALIGNED(src_bytes, sizeof(uintptr_t));
    DCHECK_ALIGNED(dst_bytes, sizeof(uintptr_t));
    // Use word sized copies to begin.
    while (num_bytes >= sizeof(uintptr_t)) {
      reinterpret_cast<Atomic<uintptr_t>*>(dst_bytes)->store(
          reinterpret_cast<Atomic<uintptr_t>*>(src_bytes)->load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      src_bytes += sizeof(uintptr_t);
      dst_bytes += sizeof(uintptr_t);
      num_bytes -= sizeof(uintptr_t);
    }
    // Copy possible 32 bit word.
    if (sizeof(uintptr_t) != sizeof(uint32_t) && num_bytes >= sizeof(uint32_t)) {
      reinterpret_cast<Atomic<uint32_t>*>(dst_bytes)->store(
          reinterpret_cast<Atomic<uint32_t>*>(src_bytes)->load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      src_bytes += sizeof(uint32_t);
      dst_bytes += sizeof(uint32_t);
      num_bytes -= sizeof(uint32_t);
    }
    // Copy remaining bytes, avoid going past the end of num_bytes since there may be a redzone
    // there.
    while (num_bytes > 0) {
      reinterpret_cast<Atomic<uint8_t>*>(dst_bytes)->store(
          reinterpret_cast<Atomic<uint8_t>*>(src_bytes)->load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      src_bytes += sizeof(uint8_t);
      dst_bytes += sizeof(uint8_t);
      num_bytes -= sizeof(uint8_t);
    }
  }

  if (kUseReadBarrier) {
    // We need a RB here. After copying the whole object above, copy references fields one by one
    // again with a RB to make sure there are no from space refs. TODO: Optimize this later?
    CopyReferenceFieldsWithReadBarrierVisitor visitor(dest);
    src->VisitReferences(visitor, visitor);
  }
  // Perform write barriers on copied object references.
  ObjPtr<Class> c = src->GetClass();
  if (c->IsArrayClass()) {
    if (!c->GetComponentType()->IsPrimitive()) {
      ObjPtr<ObjectArray<Object>> array = dest->AsObjectArray<Object>();
      WriteBarrier::ForArrayWrite(dest, 0, array->GetLength());
    }
  } else {
    WriteBarrier::ForEveryFieldWrite(dest);
  }
  return dest;
}

// An allocation pre-fence visitor that copies the object.
class CopyObjectVisitor {
 public:
  CopyObjectVisitor(Handle<Object>* orig, size_t num_bytes)
      : orig_(orig), num_bytes_(num_bytes) {}

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Object::CopyObject(obj, orig_->Get(), num_bytes_);
  }

 private:
  Handle<Object>* const orig_;
  const size_t num_bytes_;
  DISALLOW_COPY_AND_ASSIGN(CopyObjectVisitor);
};

ObjPtr<Object> Object::Clone(Thread* self) {
  // marvin start
  SWAP_PREAMBLE(Clone, Object, ObjPtr<Object>, self)
  // marvin end
  CHECK(!IsClass()) << "Can't clone classes.";
  // Object::SizeOf gets the right size even if we're an array. Using c->AllocObject() here would
  // be wrong.
  gc::Heap* heap = Runtime::Current()->GetHeap();
  size_t num_bytes = SizeOf();
  StackHandleScope<1> hs(self);
  Handle<Object> this_object(hs.NewHandle(this));
  ObjPtr<Object> copy;
  CopyObjectVisitor visitor(&this_object, num_bytes);
  if (heap->IsMovableObject(this)) {
    copy = heap->AllocObject<true>(self, GetClass(), num_bytes, visitor);
  } else {
    copy = heap->AllocNonMovableObject<true>(self, GetClass(), num_bytes, visitor);
  }
  if (this_object->GetClass()->IsFinalizable()) {
    heap->AddFinalizerReference(self, &copy);
  }
  return copy;
}

uint32_t Object::GenerateIdentityHashCode() {
  uint32_t expected_value, new_value;
  do {
    expected_value = hash_code_seed.load(std::memory_order_relaxed);
    new_value = expected_value * 1103515245 + 12345;
  } while (!hash_code_seed.CompareAndSetWeakRelaxed(expected_value, new_value) ||
      (expected_value & LockWord::kHashMask) == 0);
  return expected_value & LockWord::kHashMask;
}

void Object::SetHashCodeSeed(uint32_t new_seed) {
  hash_code_seed.store(new_seed, std::memory_order_relaxed);
}

int32_t Object::IdentityHashCode() {
  // marvin start
  SWAP_PREAMBLE(IdentityHashCode, Object, int32_t, )
  // marvin end
  ObjPtr<Object> current_this = this;  // The this pointer may get invalidated by thread suspension.
  while (true) {
    LockWord lw = current_this->GetLockWord(false);
    switch (lw.GetState()) {
      case LockWord::kUnlocked: {
        // Try to compare and swap in a new hash, if we succeed we will return the hash on the next
        // loop iteration.
        LockWord hash_word = LockWord::FromHashCode(GenerateIdentityHashCode(), lw.GCState());
        DCHECK_EQ(hash_word.GetState(), LockWord::kHashCode);
        // Use a strong CAS to prevent spurious failures since these can make the boot image
        // non-deterministic.
        if (current_this->CasLockWord(lw, hash_word, CASMode::kStrong, std::memory_order_relaxed)) {
          return hash_word.GetHashCode();
        }
        break;
      }
      case LockWord::kThinLocked: {
        // Inflate the thin lock to a monitor and stick the hash code inside of the monitor. May
        // fail spuriously.
        Thread* self = Thread::Current();
        StackHandleScope<1> hs(self);
        Handle<mirror::Object> h_this(hs.NewHandle(current_this));
        Monitor::InflateThinLocked(self, h_this, lw, GenerateIdentityHashCode());
        // A GC may have occurred when we switched to kBlocked.
        current_this = h_this.Get();
        break;
      }
      case LockWord::kFatLocked: {
        // Already inflated, return the hash stored in the monitor.
        Monitor* monitor = lw.FatLockMonitor();
        DCHECK(monitor != nullptr);
        return monitor->GetHashCode();
      }
      case LockWord::kHashCode: {
        return lw.GetHashCode();
      }
      default: {
        LOG(FATAL) << "Invalid state during hashcode " << lw.GetState();
        UNREACHABLE();
      }
    }
  }
}

void Object::CheckFieldAssignmentImpl(MemberOffset field_offset, ObjPtr<Object> new_value) {
  // marvin start
  if (field_offset.Uint32Value() >= sizeof(Object)) {
    SWAP_PREAMBLE_VOID(CheckFieldAssignmentImpl, Object, field_offset, new_value)
  }
  // marvin end
  ObjPtr<Class> c = GetClass();
  Runtime* runtime = Runtime::Current();
  if (runtime->GetClassLinker() == nullptr || !runtime->IsStarted() ||
      !runtime->GetHeap()->IsObjectValidationEnabled() || !c->IsResolved()) {
    return;
  }
  for (ObjPtr<Class> cur = c; cur != nullptr; cur = cur->GetSuperClass()) {
    for (ArtField& field : cur->GetIFields()) {
      if (field.GetOffset().Int32Value() == field_offset.Int32Value()) {
        CHECK_NE(field.GetTypeAsPrimitiveType(), Primitive::kPrimNot);
        // TODO: resolve the field type for moving GC.
        ObjPtr<mirror::Class> field_type =
            kMovingCollector ? field.LookupResolvedType() : field.ResolveType();
        if (field_type != nullptr) {
          CHECK(field_type->IsAssignableFrom(new_value->GetClass()));
        }
        return;
      }
    }
  }
  if (c->IsArrayClass()) {
    // Bounds and assign-ability done in the array setter.
    return;
  }
  if (IsClass()) {
    for (ArtField& field : AsClass()->GetSFields()) {
      if (field.GetOffset().Int32Value() == field_offset.Int32Value()) {
        CHECK_NE(field.GetTypeAsPrimitiveType(), Primitive::kPrimNot);
        // TODO: resolve the field type for moving GC.
        ObjPtr<mirror::Class> field_type =
            kMovingCollector ? field.LookupResolvedType() : field.ResolveType();
        if (field_type != nullptr) {
          CHECK(field_type->IsAssignableFrom(new_value->GetClass()));
        }
        return;
      }
    }
  }
  LOG(FATAL) << "Failed to find field for assignment to " << reinterpret_cast<void*>(this)
             << " of type " << c->PrettyDescriptor() << " at offset " << field_offset;
  UNREACHABLE();
}

ArtField* Object::FindFieldByOffset(MemberOffset offset) {
  // marvin start
  SWAP_PREAMBLE(FindFieldByOffset, Object, ArtField*, offset)
  // marvin end
  return IsClass() ? ArtField::FindStaticFieldWithOffset(AsClass(), offset.Uint32Value())
      : ArtField::FindInstanceFieldWithOffset(GetClass(), offset.Uint32Value());
}

std::string Object::PrettyTypeOf(ObjPtr<mirror::Object> obj) {
  return (obj == nullptr) ? "null" : obj->PrettyTypeOf();
}

std::string Object::PrettyTypeOf() {
  // From-space version is the same as the to-space version since the dex file never changes.
  // Avoiding the read barrier here is important to prevent recursive AssertToSpaceInvariant
  // issues.
  ObjPtr<mirror::Class> klass = GetClass<kDefaultVerifyFlags, kWithoutReadBarrier>();
  if (klass == nullptr) {
    return "(raw)";
  }
  std::string temp;
  std::string result(PrettyDescriptor(klass->GetDescriptor(&temp)));
  if (klass->IsClassClass()) {
    result += "<" + PrettyDescriptor(AsClass()->GetDescriptor(&temp)) + ">";
  }
  return result;
}

// marvin start
bool Object::TestBitMethods() {
    const int NUM_TESTS = 15;
    uint32_t result[NUM_TESTS];
    uint32_t expected[NUM_TESTS];

    bool passed = true;

    uint32_t data0 = 0xabcdef12;
    result[0] = GetBits32(data0, 4, 12);
    expected[0] = 0xef1;

    uint32_t data1 = 0xffffffff;
    result[1] = GetBits32(data1, 17, 7);
    expected[1] = 0x7f;

    uint32_t data2 = 0x00000000;
    SetBits32(&data2, 4, 12);
    result[2] = data2;
    expected[2] = 0x0000fff0;

    uint32_t data3 = 0x00000000;
    SetBits32(&data3, 17, 7);
    result[3] = data3;
    expected[3] = 0x00fe0000;

    uint32_t data4 = 0xffffffff;
    ClearBits32(&data4, 4, 12);
    result[4] = data4;
    expected[4] = 0xffff000f;

    uint32_t data5 = 0xffffffff;
    ClearBits32(&data5, 17, 7);
    result[5] = data5;
    expected[5] = 0xff01ffff;

    uint32_t data6 = 0xabcdef12;
    AssignBits32(&data6, 0x88888, 4, 12);
    result[6] = data6;
    expected[6] = 0xabcd8882;

    uint32_t data7 = 0xffffffff;
    AssignBits32(&data7, 0xe, 16, 8);
    result[7] = data7;
    expected[7] = 0xff0effff;

    uint8_t data8 = 0xd9; // 11011001
    result[8] = GetBits8(data8, 2, 4);
    expected[8] = 6; // 0110

    uint8_t data9 = 0x00;
    SetBits8(&data9, 5, 2);
    result[9] = data9;
    expected[9] = 0x60; // 01100000

    uint8_t data10 = 0xff;
    ClearBits8(&data10, 3, 3);
    result[10] = data10;
    expected[10] = 0xc7; // 11000111

    uint8_t data11 = 0xbb; // 10111011
    AssignBits8(&data11, 0x1, 1, 4);
    result[11] = data11;
    expected[11] = 0xa3; // 10100011

    // values from test 8 above
    std::atomic<uint8_t> data12(0xd9);
    result[12] = GetBitsAtomic8(data12, 2, 4, std::memory_order_seq_cst);
    expected[12] = 6;

    // values from test 9 above
    std::atomic<uint8_t> data13(0x00);
    SetBitsAtomic8(data13, 5, 2, std::memory_order_seq_cst);
    result[13] = data13.load();
    expected[13] = 0x60;

    // values from test 10 above
    std::atomic<uint8_t> data14(0xff);
    ClearBitsAtomic8(data14, 3, 3, std::memory_order_seq_cst);
    result[14] = data14.load();
    expected[14] = 0xc7;

    for (int i = 0; i < NUM_TESTS; i++) {
        if (result[i] != expected[i]) {
            passed = false;
            LOG(ERROR) << "Object::TestBitMethods(): test " << i << " failed";
        }
    }
    return passed;
}


bool Object::GetIgnoreReadFlag() {
  if (Runtime::Current()->IsSystemServer()) return true;
  return (bool)GetBits8(x_x3_access_bits_, 0, 1);
}
void Object::SetIgnoreReadFlag() {
  if (Runtime::Current()->IsSystemServer()) return;
  SetBits8(&x_x3_access_bits_, 0, 1);
}
void Object::ClearIgnoreReadFlag() {
  if (Runtime::Current()->IsSystemServer()) return;
  ClearBits8(&x_x3_access_bits_, 0, 1);
}

bool Object::GetWriteBit() {
  if (Runtime::Current()->IsSystemServer() || !Runtime::Current()->IsStarted()) return false;
  return (bool)GetBits8(x_x3_access_bits_, 2, 1);
}
void Object::SetWriteBit() {
  if (Runtime::Current()->IsSystemServer() || !Runtime::Current()->IsStarted()) return;
  SetBits8(&x_x3_access_bits_, 2, 1);
}
void Object::ClearWriteBit() {
  if (Runtime::Current()->IsSystemServer() || !Runtime::Current()->IsStarted()) return;
  ClearBits8(&x_x3_access_bits_, 2, 1);
}

bool Object::GetReadBit() {
  if (Runtime::Current()->IsSystemServer()) return false;
  return (bool)GetBits8(x_x3_access_bits_, 1, 1);
}
void Object::SetReadBit() {
  if (Runtime::Current()->IsSystemServer()) return;
  SetBits8(&x_x3_access_bits_, 1, 1);
}
void Object::ClearReadBit() {
  if (Runtime::Current()->IsSystemServer()) return;
  ClearBits8(&x_x3_access_bits_, 1, 1);
}


uint8_t Object::GetWriteShiftRegister() {
  if (Runtime::Current()->IsSystemServer()) return 0;
  return GetBits8(x_x1_shift_regs_, 0, 4);
}
void Object::UpdateWriteShiftRegister(bool written) {
  if (Runtime::Current()->IsSystemServer()) return;
  uint8_t oldVal = GetWriteShiftRegister();
  uint8_t newVal = (oldVal >> 1) | ((uint8_t)written << (4 - 1));
  AssignBits8(&x_x1_shift_regs_, newVal, 0, 4);
}

uint8_t Object::GetReadShiftRegister() {
  if (Runtime::Current()->IsSystemServer()) return 0;
  return GetBits8(x_x1_shift_regs_, 4, 4);
}
void Object::UpdateReadShiftRegister(bool read) {
  if (Runtime::Current()->IsSystemServer()) return;
  uint8_t oldVal = GetReadShiftRegister();
  uint8_t newVal = (oldVal >> 1) | ((uint8_t)read << (4 - 1));
  AssignBits8(&x_x1_shift_regs_, newVal, 4, 4);
}

bool Object::GetDirtyBit() {
  if (Runtime::Current()->IsSystemServer() || !Runtime::Current()->IsStarted()) return false;
  return (bool)x_x2_dirty_bit_.load(std::memory_order_acquire);
}
void Object::SetDirtyBit() {
  if (Runtime::Current()->IsSystemServer() || !Runtime::Current()->IsStarted()) return;
  x_x2_dirty_bit_.store(1, std::memory_order_release);
}
void Object::ClearDirtyBit() {
  if (Runtime::Current()->IsSystemServer() || !Runtime::Current()->IsStarted()) return;
  x_x2_dirty_bit_.store(0, std::memory_order_release);
}

bool Object::GetStubFlag() const {
  if (Runtime::Current()->IsSystemServer()) return false;
  return (bool)GetBitsAtomic8(x_x0_flags_, 7, 1, std::memory_order_acquire);
}

bool Object::GetNoSwapFlag() const {
  if (Runtime::Current()->IsSystemServer()) return true;
  return (bool)GetBitsAtomic8(x_x0_flags_, 2, 1, std::memory_order_acquire);
}
void Object::SetNoSwapFlag() {
  if (Runtime::Current()->IsSystemServer()) return;
  SetBitsAtomic8(x_x0_flags_, 2, 1, std::memory_order_acq_rel);
}

uint32_t Object::GetPadding() {
  return x_padding_;
}
void Object::SetPadding(uint32_t val) {
  if (Runtime::Current()->IsSystemServer()) return;
  x_padding_ = val;
}

// jiacheng start
void Object::CopyHeadFrom(Object* from_ref) {
  x_padding_ = from_ref->x_padding_;  // +8
  x_x0_flags_ = from_ref->x_x0_flags_.load();  // +12
  x_x1_shift_regs_ = from_ref->x_x1_shift_regs_;  // +13
  x_x2_dirty_bit_ = from_ref->x_x2_dirty_bit_.load(); // +14
  x_x3_access_bits_ = from_ref->x_x3_access_bits_;  // +15
}
// jiacheng end
// marvin end

}  // namespace mirror
}  // namespace art
