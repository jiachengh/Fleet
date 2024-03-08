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

#ifndef ART_RUNTIME_MIRROR_OBJECT_H_
#define ART_RUNTIME_MIRROR_OBJECT_H_

#include "base/atomic.h"
#include "base/casts.h"
#include "base/enums.h"
#include "dex/primitive.h"
#include "obj_ptr.h"
#include "object_reference.h"
#include "offsets.h"
#include "read_barrier_config.h"
#include "read_barrier_option.h"
#include "runtime_globals.h"
#include "verify_object.h"

// marvin start
#include "niel_stub-inl.h"
#include "niel_swap.h"
// marvin end

// marvin start

// #define SWAP_PREAMBLE(func_name, type_name, return_type, ...)

#define SWAP_PREAMBLE(func_name, type_name, return_type, ...) \
if (UNLIKELY(GetStubFlag())) { \
  niel::swap::Stub * stub = (niel::swap::Stub *)this; \
  stub->LockTableEntry(); \
  if (UNLIKELY(!stub->GetTableEntry()->GetResidentBit())) { \
    niel::swap::SwapInOnDemand(stub); \
  } \
  return_type result = ((type_name *)(stub->GetObjectAddress()))->func_name(__VA_ARGS__); \
  stub->UnlockTableEntry(); \
  return result; \
}

// #define SWAP_PREAMBLE_TEMPLATE(func_name, type_name, return_type, template_args, ...)

#define SWAP_PREAMBLE_TEMPLATE(func_name, type_name, return_type, template_args, ...) \
if (UNLIKELY(GetStubFlag())) { \
  niel::swap::Stub * stub = (niel::swap::Stub *)this; \
  stub->LockTableEntry(); \
  if (UNLIKELY(!stub->GetTableEntry()->GetResidentBit())) { \
    niel::swap::SwapInOnDemand(stub); \
  } \
  return_type result = ((type_name *)(stub->GetObjectAddress()))->func_name<template_args>(__VA_ARGS__); \
  stub->UnlockTableEntry(); \
  return result; \
}

// #define SWAP_PREAMBLE_VOID(func_name, type_name, ...)

#define SWAP_PREAMBLE_VOID(func_name, type_name, ...) \
if (UNLIKELY(GetStubFlag())) { \
  niel::swap::Stub * stub = (niel::swap::Stub *)this; \
  stub->LockTableEntry(); \
  if (UNLIKELY(!stub->GetTableEntry()->GetResidentBit())) { \
    niel::swap::SwapInOnDemand(stub); \
  } \
  ((type_name *)(stub->GetObjectAddress()))->func_name(__VA_ARGS__); \
  stub->UnlockTableEntry(); \
  return; \
}


// #define SWAP_PREAMBLE_TEMPLATE_VOID(func_name, type_name, template_args, ...)

#define SWAP_PREAMBLE_TEMPLATE_VOID(func_name, type_name, template_args, ...) \
if (UNLIKELY(GetStubFlag())) { \
  niel::swap::Stub * stub = (niel::swap::Stub *)this; \
  stub->LockTableEntry(); \
  if (UNLIKELY(!stub->GetTableEntry()->GetResidentBit())) { \
    niel::swap::SwapInOnDemand(stub); \
  } \
  ((type_name *)(stub->GetObjectAddress()))->func_name<template_args>(__VA_ARGS__); \
  stub->UnlockTableEntry(); \
  return; \
}

// #define GATHER_TEMPLATE_ARGS(...)
#define GATHER_TEMPLATE_ARGS(...) __VA_ARGS__
// marvin end

namespace art {

class ArtField;
class ArtMethod;
class LockWord;
class Monitor;
struct ObjectOffsets;
class Thread;
class VoidFunctor;

namespace mirror {

class Array;
class Class;
class ClassLoader;
class DexCache;
class FinalizerReference;
template<class T> class ObjectArray;
template<class T> class PrimitiveArray;
typedef PrimitiveArray<uint8_t> BooleanArray;
typedef PrimitiveArray<int8_t> ByteArray;
typedef PrimitiveArray<uint16_t> CharArray;
typedef PrimitiveArray<double> DoubleArray;
typedef PrimitiveArray<float> FloatArray;
typedef PrimitiveArray<int32_t> IntArray;
typedef PrimitiveArray<int64_t> LongArray;
typedef PrimitiveArray<int16_t> ShortArray;
class Reference;
class String;
class Throwable;

// Fields within mirror objects aren't accessed directly so that the appropriate amount of
// handshaking is done with GC (for example, read and write barriers). This macro is used to
// compute an offset for the Set/Get methods defined in Object that can safely access fields.
#define OFFSET_OF_OBJECT_MEMBER(type, field) \
    MemberOffset(OFFSETOF_MEMBER(type, field))

// Checks that we don't do field assignments which violate the typing system.
static constexpr bool kCheckFieldAssignments = false;

// Size of Object.
// marvin start
// static constexpr uint32_t kObjectHeaderSize = kUseBrooksReadBarrier ? 16 : 8;
static constexpr uint32_t kObjectHeaderSize = kUseBrooksReadBarrier ? 24 : 16;
// marvin end

// C++ mirror of java.lang.Object
class MANAGED LOCKABLE Object {
 public:
  // The number of vtable entries in java.lang.Object.
  static constexpr size_t kVTableLength = 11;

  // The size of the java.lang.Class representing a java.lang.Object.
  static uint32_t ClassSize(PointerSize pointer_size);

  // Size of an instance of java.lang.Object.
  static constexpr uint32_t InstanceSize() {
    return sizeof(Object);
  }

  static constexpr MemberOffset ClassOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Object, klass_);
  }

  // jiacheng start
  static constexpr size_t PaddingOffset() {
    return OFFSETOF_MEMBER(mirror::Object, x_padding_);
  }

  static constexpr size_t FlagsOffset() {
    return OFFSETOF_MEMBER(mirror::Object, x_x0_flags_);
  }

  static constexpr size_t ShiftRegsOffset() {
    return OFFSETOF_MEMBER(mirror::Object, x_x1_shift_regs_);
  }

  static constexpr size_t DirtyBitOffset() {
    return OFFSETOF_MEMBER(mirror::Object, x_x2_dirty_bit_);
  }

  static constexpr size_t AccessBitsOffset() {
    return OFFSETOF_MEMBER(mirror::Object, x_x3_access_bits_);
  }
  // jiacheng end

  // marvin start
  static ALWAYS_INLINE uint32_t GetBits32(uint32_t data, uint32_t offset, uint32_t width) {
    return (data >> offset) & (0xffffffff >> (32 - width));
  }
  static ALWAYS_INLINE void SetBits32(uint32_t * data, uint32_t offset, uint32_t width) {
    *data = *data | ((0xffffffff >> (32 - width)) << offset);
  }
  static ALWAYS_INLINE void ClearBits32(uint32_t * data, uint32_t offset, uint32_t width) {
    *data = *data & ~((0xffffffff >> (32 - width)) << offset);
  }
  static ALWAYS_INLINE void AssignBits32(uint32_t * data, uint32_t val, uint32_t offset, uint32_t width) {
    ClearBits32(data, offset, width);
    *data = *data | ((val << offset) & (0xffffffff >> (32 - width - offset)));
  }

  static ALWAYS_INLINE uint8_t GetBits8(uint8_t data, uint8_t offset, uint8_t width) {
    return (data >> offset) & (0xff >> (8 - width));
  }
  static ALWAYS_INLINE void SetBits8(uint8_t * data, uint8_t offset, uint8_t width) {
    *data = *data | ((0xff >> (8 - width)) << offset);
  }
  static ALWAYS_INLINE void ClearBits8(uint8_t * data, uint8_t offset, uint8_t width) {
    *data = *data & ~((0xff >> (8 - width)) << offset);
  }
  static ALWAYS_INLINE void AssignBits8(uint8_t * data, uint8_t val, uint8_t offset, uint8_t width) {
    ClearBits8(data, offset, width);
    *data = *data | ((val << offset) & (0xff >> (8 - width - offset)));
  }

  static ALWAYS_INLINE uint8_t GetBitsAtomic8(const std::atomic<uint8_t> & data, uint8_t offset,
                                uint8_t width, std::memory_order order) {
    return (data.load(order) >> offset) & (0xff >> (8 - width));
  }
  static ALWAYS_INLINE void SetBitsAtomic8(std::atomic<uint8_t> & data, uint8_t offset, uint8_t width,
                              std::memory_order order) {
    data.fetch_or((0xff >> (8 - width) << offset), order);
  }
  static ALWAYS_INLINE void ClearBitsAtomic8(std::atomic<uint8_t> & data, uint8_t offset, uint8_t width,
                                std::memory_order order) {
    data.fetch_and(~((0xff >> (8 - width)) << offset), order);
  }

  static bool TestBitMethods();

  /*
   * Current layout of added header bytes:
   *
   * x_flags_:
   * 7|6543|2|10
   * s|----|n|--
   *
   * x_shift_regs_:
   * 7654|3210
   * rsr |wsr
   *
     * x_dirty_bit_:
   * 7654321|0
   * -------|d
   *
   * x_access_bits_:
   * 76543|2|1|0
   * -----|w|r|i
   *
   * s: stub flag
   * w: write bit
   * r: read bit
   * n: no-swap flag
   * i: ignore read flag
   * d: dirty bit
   * rsr: read shift register (for Clock working set estimation)
   * wsr: write shift register (for Clock)
   */

  bool GetIgnoreReadFlag();
  void SetIgnoreReadFlag();
  void ClearIgnoreReadFlag();

  bool GetWriteBit();
  void SetWriteBit();
  void ClearWriteBit();

  bool GetReadBit();
  void SetReadBit();
  void ClearReadBit();

  uint8_t GetWriteShiftRegister();
  void UpdateWriteShiftRegister(bool written);

  uint8_t GetReadShiftRegister();
  void UpdateReadShiftRegister(bool read);

  bool GetDirtyBit();
  void SetDirtyBit();
  void ClearDirtyBit();

  bool GetStubFlag() const;

  bool GetNoSwapFlag() const;
  void SetNoSwapFlag();

  uint32_t GetPadding();
  void SetPadding(uint32_t val);

  // jiacheng start
  void CopyHeadFrom(Object* from_ref);
  // jiacheng end
  // marvin end

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE Class* GetClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetClass(ObjPtr<Class> new_klass) REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the read barrier state with a fake address dependency.
  // '*fake_address_dependency' will be set to 0.
  ALWAYS_INLINE uint32_t GetReadBarrierState(uintptr_t* fake_address_dependency)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // This version does not offer any special mechanism to prevent load-load reordering.
  ALWAYS_INLINE uint32_t GetReadBarrierState() REQUIRES_SHARED(Locks::mutator_lock_);
  // Get the read barrier state with a load-acquire.
  ALWAYS_INLINE uint32_t GetReadBarrierStateAcquire() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetReadBarrierState(uint32_t rb_state) REQUIRES_SHARED(Locks::mutator_lock_);

  template<std::memory_order kMemoryOrder = std::memory_order_relaxed>
  ALWAYS_INLINE bool AtomicSetReadBarrierState(uint32_t expected_rb_state, uint32_t rb_state)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE uint32_t GetMarkBit() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE bool AtomicSetMarkBit(uint32_t expected_mark_bit, uint32_t mark_bit)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Assert that the read barrier state is in the default (white, i.e. non-gray) state.
  ALWAYS_INLINE void AssertReadBarrierState() const REQUIRES_SHARED(Locks::mutator_lock_);

  // The verifier treats all interfaces as java.lang.Object and relies on runtime checks in
  // invoke-interface to detect incompatible interface types.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool VerifierInstanceOf(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool InstanceOf(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t SizeOf() REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<Object> Clone(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  int32_t IdentityHashCode()
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::thread_list_lock_,
               !Locks::thread_suspend_count_lock_);

  static constexpr MemberOffset MonitorOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Object, monitor_);
  }

  // As_volatile can be false if the mutators are suspended. This is an optimization since it
  // avoids the barriers.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  LockWord GetLockWord(bool as_volatile) REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetLockWord(LockWord new_val, bool as_volatile) REQUIRES_SHARED(Locks::mutator_lock_);
  bool CasLockWord(LockWord old_val, LockWord new_val, CASMode mode, std::memory_order memory_order)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // marvin start
  // uint32_t GetLockOwnerThreadId();
  uint32_t GetLockOwnerThreadId()
      REQUIRES_SHARED(Locks::mutator_lock_);
  // marvin end

  // Try to enter the monitor, returns non null if we succeeded.
  ObjPtr<mirror::Object> MonitorTryEnter(Thread* self)
      EXCLUSIVE_LOCK_FUNCTION()
      REQUIRES(!Roles::uninterruptible_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<mirror::Object> MonitorEnter(Thread* self)
      EXCLUSIVE_LOCK_FUNCTION()
      REQUIRES(!Roles::uninterruptible_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool MonitorExit(Thread* self)
      REQUIRES(!Roles::uninterruptible_)
      REQUIRES_SHARED(Locks::mutator_lock_)
      UNLOCK_FUNCTION();
  void Notify(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void NotifyAll(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void Wait(Thread* self, int64_t timeout, int32_t nanos) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsClass() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<Class> AsClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsObjectArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<ObjectArray<T>> AsObjectArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<ClassLoader> AsClassLoader() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsDexCache() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<DexCache> AsDexCache() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsArrayInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<Array> AsArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsBooleanArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<BooleanArray> AsBooleanArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsByteArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<ByteArray> AsByteArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsCharArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<CharArray> AsCharArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsShortArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<ShortArray> AsShortArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsIntArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<IntArray> AsIntArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<IntArray> AsIntArrayUnchecked() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsLongArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<LongArray> AsLongArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<LongArray> AsLongArrayUnchecked() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsFloatArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<FloatArray> AsFloatArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsDoubleArray() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<DoubleArray> AsDoubleArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsString() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<String> AsString() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<Throwable> AsThrowable() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  bool IsReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<Reference> AsReference() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsWeakReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsSoftReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsFinalizerReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<FinalizerReference> AsFinalizerReference() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPhantomReferenceInstance() REQUIRES_SHARED(Locks::mutator_lock_);

  // Accessor for Java type fields.
  template<class T,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
           bool kIsVolatile = false>
  ALWAYS_INLINE T* GetFieldObject(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<class T,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE T* GetFieldObjectVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                       ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldObject(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldObjectVolatile(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldObjectTransaction(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool CasFieldObject(MemberOffset field_offset,
                                    ObjPtr<Object> old_value,
                                    ObjPtr<Object> new_value,
                                    CASMode mode,
                                    std::memory_order memory_order)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool CasFieldObjectWithoutWriteBarrier(MemberOffset field_offset,
                                                       ObjPtr<Object> old_value,
                                                       ObjPtr<Object> new_value,
                                                       CASMode mode,
                                                       std::memory_order memory_order)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<Object> CompareAndExchangeFieldObject(MemberOffset field_offset,
                                               ObjPtr<Object> old_value,
                                               ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ObjPtr<Object> ExchangeFieldObject(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  HeapReference<Object>* GetFieldObjectReferenceAddr(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<typename kType, bool kIsVolatile>
  ALWAYS_INLINE void SetFieldPrimitive(MemberOffset field_offset, kType new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // marvin start
    if (field_offset.Uint32Value() >= sizeof(Object)) {
      SWAP_PREAMBLE_TEMPLATE_VOID(SetFieldPrimitive, Object, GATHER_TEMPLATE_ARGS(kType, kIsVolatile), field_offset, new_value)
    }
    // marvin end
    uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
    kType* addr = reinterpret_cast<kType*>(raw_addr);
    if (kIsVolatile) {
      reinterpret_cast<Atomic<kType>*>(addr)->store(new_value, std::memory_order_seq_cst);
    } else {
      reinterpret_cast<Atomic<kType>*>(addr)->StoreJavaData(new_value);
    }
    // marvin start
    SetWriteBit();
    SetDirtyBit();
    // marvin end
  }

// jiacheng start
  template<typename kType, bool kIsVolatile>
  ALWAYS_INLINE kType GetFieldPrimitive(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // marvin start
    if (field_offset.Uint32Value() >= sizeof(Object)) {
      SWAP_PREAMBLE_TEMPLATE(GetFieldPrimitive, Object, kType, GATHER_TEMPLATE_ARGS(kType, kIsVolatile), field_offset)
    }
    // Object* obj = reinterpret_cast<Object*>(this);
    // if (UNLIKELY(GetStubFlag())) {
    //   niel::swap::Stub * stub = (niel::swap::Stub *)this;
    //   stub->LockTableEntry();
    //   if (UNLIKELY(!stub->GetTableEntry()->GetResidentBit())) {
    //     niel::swap::SwapInOnDemand(stub);
    //   }
    //   obj = reinterpret_cast<Object*>(stub->GetObjectAddress());
    //   stub->UnlockTableEntry();
    // }
    if (!GetIgnoreReadFlag()) {
        SetReadBit();
    }
    // const uint8_t* raw_addr = reinterpret_cast<const uint8_t*>(obj) + field_offset.Int32Value();
    const uint8_t* raw_addr = reinterpret_cast<const uint8_t*>(this) + field_offset.Int32Value();
    // marvin end
    const kType* addr = reinterpret_cast<const kType*>(raw_addr);
    if (kIsVolatile) {
      return reinterpret_cast<const Atomic<kType>*>(addr)->load(std::memory_order_seq_cst);
    } else {
      return reinterpret_cast<const Atomic<kType>*>(addr)->LoadJavaData();
    }
  }
// jiacheng end

// jiacheng debug start
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE uint8_t GetFieldBoolean(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // marvin start
    if (field_offset.Uint32Value() >= sizeof(Object)) {
      SWAP_PREAMBLE_TEMPLATE(GetFieldBoolean, Object, uint8_t, GATHER_TEMPLATE_ARGS(kVerifyFlags, kIsVolatile), field_offset)
    }
    // marvin end
    Verify<kVerifyFlags>();
    return GetFieldPrimitive<uint8_t, kIsVolatile>(field_offset);
  }
  // jiacheng debug end

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int8_t GetFieldByte(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint8_t GetFieldBooleanVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int8_t GetFieldByteVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldBoolean(MemberOffset field_offset, uint8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldByte(MemberOffset field_offset, int8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldBooleanVolatile(MemberOffset field_offset, uint8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldByteVolatile(MemberOffset field_offset, int8_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE uint16_t GetFieldChar(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int16_t GetFieldShort(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint16_t GetFieldCharVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int16_t GetFieldShortVolatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldChar(MemberOffset field_offset, uint16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetFieldShort(MemberOffset field_offset, int16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldCharVolatile(MemberOffset field_offset, uint16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetFieldShortVolatile(MemberOffset field_offset, int16_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

// jiacheng debug start
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int32_t GetField32(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // marvin start
    if (field_offset.Uint32Value() >= sizeof(Object)) {
      SWAP_PREAMBLE_TEMPLATE(GetField32, Object, int32_t, GATHER_TEMPLATE_ARGS(kVerifyFlags, kIsVolatile), field_offset)
    }
    // marvin end
    Verify<kVerifyFlags>();
    return GetFieldPrimitive<int32_t, kIsVolatile>(field_offset);
  }
// jiacheng debug end


  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int32_t GetField32Volatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags, true>(field_offset);
  }

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetField32(MemberOffset field_offset, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetField32Volatile(MemberOffset field_offset, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetField32Transaction(MemberOffset field_offset, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool CasField32(MemberOffset field_offset,
                                int32_t old_value,
                                int32_t new_value,
                                CASMode mode,
                                std::memory_order memory_order)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE int64_t GetField64(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // marvin start
    if (field_offset.Uint32Value() >= sizeof(Object)) {
      SWAP_PREAMBLE_TEMPLATE(GetField64, Object, int64_t, GATHER_TEMPLATE_ARGS(kVerifyFlags, kIsVolatile), field_offset)
    }
    // marvin end
    Verify<kVerifyFlags>();
    return GetFieldPrimitive<int64_t, kIsVolatile>(field_offset);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int64_t GetField64Volatile(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField64<kVerifyFlags, true>(field_offset);
  }

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetField64(MemberOffset field_offset, int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE void SetField64Volatile(MemberOffset field_offset, int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kIsVolatile = false>
  ALWAYS_INLINE void SetField64Transaction(MemberOffset field_offset, int32_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldWeakSequentiallyConsistent64(MemberOffset field_offset,
                                            int64_t old_value,
                                            int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool CasFieldStrongSequentiallyConsistent64(MemberOffset field_offset,
                                              int64_t old_value,
                                              int64_t new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           typename T>
  void SetFieldPtr(MemberOffset field_offset, T new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SetFieldPtrWithSize<kTransactionActive, kCheckTransaction, kVerifyFlags>(
        field_offset, new_value, kRuntimePointerSize);
  }
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           typename T>
  void SetFieldPtr64(MemberOffset field_offset, T new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SetFieldPtrWithSize<kTransactionActive, kCheckTransaction, kVerifyFlags>(
        field_offset, new_value, PointerSize::k64);
  }

  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           typename T>
  ALWAYS_INLINE void SetFieldPtrWithSize(MemberOffset field_offset,
                                         T new_value,
                                         PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (pointer_size == PointerSize::k32) {
      SetField32<kTransactionActive, kCheckTransaction, kVerifyFlags>(
          field_offset, reinterpret_cast32<int32_t>(new_value));
    } else {
      SetField64<kTransactionActive, kCheckTransaction, kVerifyFlags>(
          field_offset, reinterpret_cast64<int64_t>(new_value));
    }
  }

  // Base class for accessors used to describe accesses performed by VarHandle methods.
  template <typename T>
  class Accessor {
   public:
    virtual ~Accessor() {
      static_assert(std::is_arithmetic<T>::value, "unsupported type");
    }
    virtual void Access(T* field_address) = 0;
  };

  // Getter method that exposes the raw address of a primitive value-type field to an Accessor
  // instance. This are used by VarHandle accessor methods to read fields with a wider range of
  // memory orderings than usually required.
  template<typename T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void GetPrimitiveFieldViaAccessor(MemberOffset field_offset, Accessor<T>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Update methods that expose the raw address of a primitive value-type to an Accessor instance
  // that will attempt to update the field. These are used by VarHandle accessor methods to
  // atomically update fields with a wider range of memory orderings than usually required.
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void UpdateFieldBooleanViaAccessor(MemberOffset field_offset, Accessor<uint8_t>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void UpdateFieldByteViaAccessor(MemberOffset field_offset, Accessor<int8_t>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void UpdateFieldCharViaAccessor(MemberOffset field_offset, Accessor<uint16_t>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void UpdateFieldShortViaAccessor(MemberOffset field_offset, Accessor<int16_t>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void UpdateField32ViaAccessor(MemberOffset field_offset, Accessor<int32_t>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void UpdateField64ViaAccessor(MemberOffset field_offset, Accessor<int64_t>* accessor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template <bool kVisitNativeRoots = true,
            VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor,
            typename JavaLangRefVisitor = VoidFunctor>
  void VisitReferences(const Visitor& visitor, const JavaLangRefVisitor& ref_visitor)
      NO_THREAD_SAFETY_ANALYSIS;

  ArtField* FindFieldByOffset(MemberOffset offset) REQUIRES_SHARED(Locks::mutator_lock_);

  // Used by object_test.
  static void SetHashCodeSeed(uint32_t new_seed);
  // Generate an identity hash code. Public for object test.
  static uint32_t GenerateIdentityHashCode();

  // Returns a human-readable form of the name of the *class* of the given object.
  // So given an instance of java.lang.String, the output would
  // be "java.lang.String". Given an array of int, the output would be "int[]".
  // Given String.class, the output would be "java.lang.Class<java.lang.String>".
  static std::string PrettyTypeOf(ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyTypeOf()
      REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  // Accessors for non-Java type fields
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  T GetFieldPtr(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldPtrWithSize<T, kVerifyFlags, kIsVolatile>(field_offset, kRuntimePointerSize);
  }
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  T GetFieldPtr64(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldPtrWithSize<T, kVerifyFlags, kIsVolatile>(field_offset, PointerSize::k64);
  }

// jiacheng debug start
  template<class T, VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, bool kIsVolatile = false>
  ALWAYS_INLINE T GetFieldPtrWithSize(MemberOffset field_offset, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // marvin start
    if (field_offset.Uint32Value() >= sizeof(Object)) {
      SWAP_PREAMBLE_TEMPLATE(GetFieldPtrWithSize, Object, T, GATHER_TEMPLATE_ARGS(T, kVerifyFlags, kIsVolatile), field_offset, pointer_size)
    }
    // marvin end
    // if (pointer_size == PointerSize::k32) {
    //   int32_t v = obj->GetField32<kVerifyFlags, kIsVolatile>(field_offset);
    //   return reinterpret_cast32<T>(v);
    // } else {
    //   int64_t v = obj->GetField64<kVerifyFlags, kIsVolatile>(field_offset);
    //   return reinterpret_cast64<T>(v);
    // }
    if (pointer_size == PointerSize::k32) {
      int32_t v = GetField32<kVerifyFlags, kIsVolatile>(field_offset);
      return reinterpret_cast32<T>(v);
    } else {
      int64_t v = GetField64<kVerifyFlags, kIsVolatile>(field_offset);
      return reinterpret_cast64<T>(v);
    }
  }

// jiacheng debug end

  // TODO: Fixme when anotatalysis works with visitors.
  template<bool kIsStatic,
          VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
          ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
          typename Visitor>
  void VisitFieldsReferences(uint32_t ref_offsets, const Visitor& visitor) HOT_ATTR
      NO_THREAD_SAFETY_ANALYSIS;
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
           typename Visitor>
  void VisitInstanceFieldsReferences(ObjPtr<mirror::Class> klass, const Visitor& visitor) HOT_ATTR
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
           typename Visitor>
  void VisitStaticFieldsReferences(ObjPtr<mirror::Class> klass, const Visitor& visitor) HOT_ATTR
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  // Get a field with acquire semantics.
  template<typename kSize>
  ALWAYS_INLINE kSize GetFieldAcquire(MemberOffset field_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Verify the type correctness of stores to fields.
  // TODO: This can cause thread suspension and isn't moving GC safe.
  void CheckFieldAssignmentImpl(MemberOffset field_offset, ObjPtr<Object> new_value)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CheckFieldAssignment(MemberOffset field_offset, ObjPtr<Object>new_value)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kCheckFieldAssignments) {
      CheckFieldAssignmentImpl(field_offset, new_value);
    }
  }

  template<VerifyObjectFlags kVerifyFlags>
  ALWAYS_INLINE void Verify() {
    if (kVerifyFlags & kVerifyThis) {
      VerifyObject(this);
    }
  }

  // Not ObjPtr since the values may be unaligned for logic in verification.cc.
  template<VerifyObjectFlags kVerifyFlags, typename Reference>
  ALWAYS_INLINE static void VerifyRead(Reference value) {
    if (kVerifyFlags & kVerifyReads) {
      VerifyObject(value);
    }
  }

  template<VerifyObjectFlags kVerifyFlags>
  ALWAYS_INLINE static void VerifyWrite(ObjPtr<mirror::Object> value) {
    if (kVerifyFlags & kVerifyWrites) {
      VerifyObject(value);
    }
  }

  template<VerifyObjectFlags kVerifyFlags>
  ALWAYS_INLINE void VerifyCAS(ObjPtr<mirror::Object> new_value, ObjPtr<mirror::Object> old_value) {
    Verify<kVerifyFlags>();
    VerifyRead<kVerifyFlags>(old_value);
    VerifyWrite<kVerifyFlags>(new_value);
  }

  // Verify transaction is active (if required).
  template<bool kTransactionActive, bool kCheckTransaction>
  ALWAYS_INLINE void VerifyTransaction();

  // A utility function that copies an object in a read barrier and write barrier-aware way.
  // This is internally used by Clone() and Class::CopyOf(). If the object is finalizable,
  // it is the callers job to call Heap::AddFinalizerReference.
  static ObjPtr<Object> CopyObject(ObjPtr<mirror::Object> dest,
                                   ObjPtr<mirror::Object> src,
                                   size_t num_bytes)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags, Primitive::Type kType>
  bool IsSpecificPrimitiveArray() REQUIRES_SHARED(Locks::mutator_lock_);

  static Atomic<uint32_t> hash_code_seed;

  // The Class representing the type of the object.
  HeapReference<Class> klass_;
  // Monitor and hash code information.
  uint32_t monitor_;

  // marvin start
  // Names use 'x' prefix for the same reason that the Brooks variables defined
  // below do. See the block comment near the top of this file for info about the
  // layout of these variables.
  
  // jiacheng start
  // uint32_t x_padding_;  // +8
  // std::atomic<uint8_t> x_x0_flags_;  // +12
  // uint8_t x_x1_shift_regs_;  // +13
  // std::atomic<uint8_t> x_x2_dirty_bit_; // +14
  // uint8_t x_x3_access_bits_;  // +15

  uint32_t x_padding_;  // +8
  std::atomic<uint8_t> x_x0_flags_;  // +12
  uint8_t x_x1_shift_regs_;  // +13
  std::atomic<uint8_t> x_x2_dirty_bit_; // +14
  uint8_t x_x3_access_bits_;  // +15

  // jiacheng end

  // marvin end

#ifdef USE_BROOKS_READ_BARRIER
  // Note names use a 'x' prefix and the x_rb_ptr_ is of type int
  // instead of Object to go with the alphabetical/by-type field order
  // on the Java side.
  uint32_t x_rb_ptr_;      // For the Brooks pointer.
  uint32_t x_xpadding_;    // For 8-byte alignment. TODO: get rid of this.
#endif

  friend class art::Monitor;
  friend struct art::ObjectOffsets;  // for verifying offset information
  friend class CopyObjectVisitor;  // for CopyObject().
  friend class CopyClassVisitor;   // for CopyObject().
  DISALLOW_ALLOCATION();
  DISALLOW_IMPLICIT_CONSTRUCTORS(Object);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_H_
