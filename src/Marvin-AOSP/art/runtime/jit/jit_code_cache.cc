/*
 * Copyright 2014 The Android Open Source Project
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

#include "jit_code_cache.h"

#include <sstream>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include "arch/context.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/histogram-inl.h"
#include "base/logging.h"  // For VLOG.
#include "base/membarrier.h"
#include "base/memfd.h"
#include "base/mem_map.h"
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/utils.h"
#include "cha.h"
#include "debugger_interface.h"
#include "dex/dex_file_loader.h"
#include "dex/method_reference.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "gc/accounting/bitmap-inl.h"
#include "gc/allocator/dlmalloc.h"
#include "gc/scoped_gc_critical_section.h"
#include "handle.h"
#include "instrumentation.h"
#include "intern_table.h"
#include "jit/jit.h"
#include "jit/profiling_info.h"
#include "linear_alloc.h"
#include "oat_file-inl.h"
#include "oat_quick_method_header.h"
#include "object_callbacks.h"
#include "profile/profile_compilation_info.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread-current-inl.h"
#include "thread_list.h"

using android::base::unique_fd;

namespace art {
namespace jit {

static constexpr size_t kCodeSizeLogThreshold = 50 * KB;
static constexpr size_t kStackMapSizeLogThreshold = 50 * KB;

// Data cache will be half of the capacity
// Code cache will be the other half of the capacity.
// TODO: Make this variable?
static constexpr size_t kCodeAndDataCapacityDivider = 2;

static constexpr int kProtR = PROT_READ;
static constexpr int kProtRW = PROT_READ | PROT_WRITE;
static constexpr int kProtRWX = PROT_READ | PROT_WRITE | PROT_EXEC;
static constexpr int kProtRX = PROT_READ | PROT_EXEC;

namespace {

// Translate an address belonging to one memory map into an address in a second. This is useful
// when there are two virtual memory ranges for the same physical memory range.
template <typename T>
T* TranslateAddress(T* src_ptr, const MemMap& src, const MemMap& dst) {
  CHECK(src.HasAddress(src_ptr));
  uint8_t* const raw_src_ptr = reinterpret_cast<uint8_t*>(src_ptr);
  return reinterpret_cast<T*>(raw_src_ptr - src.Begin() + dst.Begin());
}

}  // namespace

class JitCodeCache::JniStubKey {
 public:
  explicit JniStubKey(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_)
      : shorty_(method->GetShorty()),
        is_static_(method->IsStatic()),
        is_fast_native_(method->IsFastNative()),
        is_critical_native_(method->IsCriticalNative()),
        is_synchronized_(method->IsSynchronized()) {
    DCHECK(!(is_fast_native_ && is_critical_native_));
  }

  bool operator<(const JniStubKey& rhs) const {
    if (is_static_ != rhs.is_static_) {
      return rhs.is_static_;
    }
    if (is_synchronized_ != rhs.is_synchronized_) {
      return rhs.is_synchronized_;
    }
    if (is_fast_native_ != rhs.is_fast_native_) {
      return rhs.is_fast_native_;
    }
    if (is_critical_native_ != rhs.is_critical_native_) {
      return rhs.is_critical_native_;
    }
    return strcmp(shorty_, rhs.shorty_) < 0;
  }

  // Update the shorty to point to another method's shorty. Call this function when removing
  // the method that references the old shorty from JniCodeData and not removing the entire
  // JniCodeData; the old shorty may become a dangling pointer when that method is unloaded.
  void UpdateShorty(ArtMethod* method) const REQUIRES_SHARED(Locks::mutator_lock_) {
    const char* shorty = method->GetShorty();
    DCHECK_STREQ(shorty_, shorty);
    shorty_ = shorty;
  }

 private:
  // The shorty points to a DexFile data and may need to change
  // to point to the same shorty in a different DexFile.
  mutable const char* shorty_;

  const bool is_static_;
  const bool is_fast_native_;
  const bool is_critical_native_;
  const bool is_synchronized_;
};

class JitCodeCache::JniStubData {
 public:
  JniStubData() : code_(nullptr), methods_() {}

  void SetCode(const void* code) {
    DCHECK(code != nullptr);
    code_ = code;
  }

  const void* GetCode() const {
    return code_;
  }

  bool IsCompiled() const {
    return GetCode() != nullptr;
  }

  void AddMethod(ArtMethod* method) {
    if (!ContainsElement(methods_, method)) {
      methods_.push_back(method);
    }
  }

  const std::vector<ArtMethod*>& GetMethods() const {
    return methods_;
  }

  void RemoveMethodsIn(const LinearAlloc& alloc) {
    auto kept_end = std::remove_if(
        methods_.begin(),
        methods_.end(),
        [&alloc](ArtMethod* method) { return alloc.ContainsUnsafe(method); });
    methods_.erase(kept_end, methods_.end());
  }

  bool RemoveMethod(ArtMethod* method) {
    auto it = std::find(methods_.begin(), methods_.end(), method);
    if (it != methods_.end()) {
      methods_.erase(it);
      return true;
    } else {
      return false;
    }
  }

  void MoveObsoleteMethod(ArtMethod* old_method, ArtMethod* new_method) {
    std::replace(methods_.begin(), methods_.end(), old_method, new_method);
  }

 private:
  const void* code_;
  std::vector<ArtMethod*> methods_;
};

bool JitCodeCache::InitializeMappings(bool rwx_memory_allowed,
                                      bool is_zygote,
                                      std::string* error_msg) {
  ScopedTrace trace(__PRETTY_FUNCTION__);

  const size_t capacity = max_capacity_;
  const size_t data_capacity = capacity / kCodeAndDataCapacityDivider;
  const size_t exec_capacity = capacity - data_capacity;

  // File descriptor enabling dual-view mapping of code section.
  unique_fd mem_fd;

  // Zygote shouldn't create a shared mapping for JIT, so we cannot use dual view
  // for it.
  if (!is_zygote) {
    // Bionic supports memfd_create, but the call may fail on older kernels.
    mem_fd = unique_fd(art::memfd_create("/jit-cache", /* flags= */ 0));
    if (mem_fd.get() < 0) {
      std::ostringstream oss;
      oss << "Failed to initialize dual view JIT. memfd_create() error: " << strerror(errno);
      if (!rwx_memory_allowed) {
        // Without using RWX page permissions, the JIT can not fallback to single mapping as it
        // requires tranitioning the code pages to RWX for updates.
        *error_msg = oss.str();
        return false;
      }
      VLOG(jit) << oss.str();
    }
  }

  if (mem_fd.get() >= 0 && ftruncate(mem_fd, capacity) != 0) {
    std::ostringstream oss;
    oss << "Failed to initialize memory file: " << strerror(errno);
    *error_msg = oss.str();
    return false;
  }

  std::string data_cache_name = is_zygote ? "zygote-data-code-cache" : "data-code-cache";
  std::string exec_cache_name = is_zygote ? "zygote-jit-code-cache" : "jit-code-cache";

  std::string error_str;
  // Map name specific for android_os_Debug.cpp accounting.
  // Map in low 4gb to simplify accessing root tables for x86_64.
  // We could do PC-relative addressing to avoid this problem, but that
  // would require reserving code and data area before submitting, which
  // means more windows for the code memory to be RWX.
  int base_flags;
  MemMap data_pages;
  if (mem_fd.get() >= 0) {
    // Dual view of JIT code cache case. Create an initial mapping of data pages large enough
    // for data and non-writable view of JIT code pages. We use the memory file descriptor to
    // enable dual mapping - we'll create a second mapping using the descriptor below. The
    // mappings will look like:
    //
    //       VA                  PA
    //
    //       +---------------+
    //       | non exec code |\
    //       +---------------+ \
    //       :               :\ \
    //       +---------------+.\.+---------------+
    //       |  exec code    |  \|     code      |
    //       +---------------+...+---------------+
    //       |      data     |   |     data      |
    //       +---------------+...+---------------+
    //
    // In this configuration code updates are written to the non-executable view of the code
    // cache, and the executable view of the code cache has fixed RX memory protections.
    //
    // This memory needs to be mapped shared as the code portions will have two mappings.
    base_flags = MAP_SHARED;
    data_pages = MemMap::MapFile(
        data_capacity + exec_capacity,
        kProtRW,
        base_flags,
        mem_fd,
        /* start= */ 0,
        /* low_4gb= */ true,
        data_cache_name.c_str(),
        &error_str);
  } else {
    // Single view of JIT code cache case. Create an initial mapping of data pages large enough
    // for data and JIT code pages. The mappings will look like:
    //
    //       VA                  PA
    //
    //       +---------------+...+---------------+
    //       |  exec code    |   |     code      |
    //       +---------------+...+---------------+
    //       |      data     |   |     data      |
    //       +---------------+...+---------------+
    //
    // In this configuration code updates are written to the executable view of the code cache,
    // and the executable view of the code cache transitions RX to RWX for the update and then
    // back to RX after the update.
    base_flags = MAP_PRIVATE | MAP_ANON;
    data_pages = MemMap::MapAnonymous(
        data_cache_name.c_str(),
        data_capacity + exec_capacity,
        kProtRW,
        /* low_4gb= */ true,
        &error_str);
  }

  if (!data_pages.IsValid()) {
    std::ostringstream oss;
    oss << "Failed to create read write cache: " << error_str << " size=" << capacity;
    *error_msg = oss.str();
    return false;
  }

  MemMap exec_pages;
  MemMap non_exec_pages;
  if (exec_capacity > 0) {
    uint8_t* const divider = data_pages.Begin() + data_capacity;
    // Set initial permission for executable view to catch any SELinux permission problems early
    // (for processes that cannot map WX pages). Otherwise, this region does not need to be
    // executable as there is no code in the cache yet.
    exec_pages = data_pages.RemapAtEnd(divider,
                                       exec_cache_name.c_str(),
                                       kProtRX,
                                       base_flags | MAP_FIXED,
                                       mem_fd.get(),
                                       (mem_fd.get() >= 0) ? data_capacity : 0,
                                       &error_str);
    if (!exec_pages.IsValid()) {
      std::ostringstream oss;
      oss << "Failed to create read execute code cache: " << error_str << " size=" << capacity;
      *error_msg = oss.str();
      return false;
    }

    if (mem_fd.get() >= 0) {
      // For dual view, create the secondary view of code memory used for updating code. This view
      // is never executable.
      std::string name = exec_cache_name + "-rw";
      non_exec_pages = MemMap::MapFile(exec_capacity,
                                       kProtR,
                                       base_flags,
                                       mem_fd,
                                       /* start= */ data_capacity,
                                       /* low_4GB= */ false,
                                       name.c_str(),
                                       &error_str);
      if (!non_exec_pages.IsValid()) {
        static const char* kFailedNxView = "Failed to map non-executable view of JIT code cache";
        if (rwx_memory_allowed) {
          // Log and continue as single view JIT (requires RWX memory).
          VLOG(jit) << kFailedNxView;
        } else {
          *error_msg = kFailedNxView;
          return false;
        }
      }
    }
  } else {
    // Profiling only. No memory for code required.
  }

  data_pages_ = std::move(data_pages);
  exec_pages_ = std::move(exec_pages);
  non_exec_pages_ = std::move(non_exec_pages);
  return true;
}

JitCodeCache* JitCodeCache::Create(bool used_only_for_profile_data,
                                   bool rwx_memory_allowed,
                                   bool is_zygote,
                                   std::string* error_msg) {
  // Register for membarrier expedited sync core if JIT will be generating code.
  if (!used_only_for_profile_data) {
    if (art::membarrier(art::MembarrierCommand::kRegisterPrivateExpeditedSyncCore) != 0) {
      // MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE ensures that CPU instruction pipelines are
      // flushed and it's used when adding code to the JIT. The memory used by the new code may
      // have just been released and, in theory, the old code could still be in a pipeline.
      VLOG(jit) << "Kernel does not support membarrier sync-core";
    }
  }

  // Check whether the provided max capacity in options is below 1GB.
  size_t max_capacity = Runtime::Current()->GetJITOptions()->GetCodeCacheMaxCapacity();
  // We need to have 32 bit offsets from method headers in code cache which point to things
  // in the data cache. If the maps are more than 4G apart, having multiple maps wouldn't work.
  // Ensure we're below 1 GB to be safe.
  if (max_capacity > 1 * GB) {
    std::ostringstream oss;
    oss << "Maxium code cache capacity is limited to 1 GB, "
        << PrettySize(max_capacity) << " is too big";
    *error_msg = oss.str();
    return nullptr;
  }

  size_t initial_capacity = Runtime::Current()->GetJITOptions()->GetCodeCacheInitialCapacity();

  std::unique_ptr<JitCodeCache> jit_code_cache(new JitCodeCache());

  MutexLock mu(Thread::Current(), jit_code_cache->lock_);
  jit_code_cache->InitializeState(initial_capacity, max_capacity);

  // Zygote should never collect code to share the memory with the children.
  if (is_zygote) {
    jit_code_cache->garbage_collect_code_ = false;
  }

  if (!jit_code_cache->InitializeMappings(rwx_memory_allowed, is_zygote, error_msg)) {
    return nullptr;
  }

  jit_code_cache->InitializeSpaces();

  VLOG(jit) << "Created jit code cache: initial capacity="
            << PrettySize(initial_capacity)
            << ", maximum capacity="
            << PrettySize(max_capacity);

  return jit_code_cache.release();
}

JitCodeCache::JitCodeCache()
    : lock_("Jit code cache", kJitCodeCacheLock),
      lock_cond_("Jit code cache condition variable", lock_),
      collection_in_progress_(false),
      last_collection_increased_code_cache_(false),
      garbage_collect_code_(true),
      used_memory_for_data_(0),
      used_memory_for_code_(0),
      number_of_compilations_(0),
      number_of_osr_compilations_(0),
      number_of_collections_(0),
      histogram_stack_map_memory_use_("Memory used for stack maps", 16),
      histogram_code_memory_use_("Memory used for compiled code", 16),
      histogram_profiling_info_memory_use_("Memory used for profiling info", 16),
      is_weak_access_enabled_(true),
      inline_cache_cond_("Jit inline cache condition variable", lock_),
      zygote_data_pages_(),
      zygote_exec_pages_(),
      zygote_data_mspace_(nullptr),
      zygote_exec_mspace_(nullptr) {
}

void JitCodeCache::InitializeState(size_t initial_capacity, size_t max_capacity) {
  CHECK_GE(max_capacity, initial_capacity);
  CHECK(max_capacity <= 1 * GB) << "The max supported size for JIT code cache is 1GB";
  // Align both capacities to page size, as that's the unit mspaces use.
  initial_capacity = RoundDown(initial_capacity, 2 * kPageSize);
  max_capacity = RoundDown(max_capacity, 2 * kPageSize);

  used_memory_for_data_ = 0;
  used_memory_for_code_ = 0;
  number_of_compilations_ = 0;
  number_of_osr_compilations_ = 0;
  number_of_collections_ = 0;

  data_pages_ = MemMap();
  exec_pages_ = MemMap();
  non_exec_pages_ = MemMap();
  initial_capacity_ = initial_capacity;
  max_capacity_ = max_capacity;
  current_capacity_ = initial_capacity,
  data_end_ = initial_capacity / kCodeAndDataCapacityDivider;
  exec_end_ = initial_capacity - data_end_;
}

void JitCodeCache::InitializeSpaces() {
  // Initialize the data heap
  data_mspace_ = create_mspace_with_base(data_pages_.Begin(), data_end_, false /*locked*/);
  CHECK(data_mspace_ != nullptr) << "create_mspace_with_base (data) failed";

  // Initialize the code heap
  MemMap* code_heap = nullptr;
  if (non_exec_pages_.IsValid()) {
    code_heap = &non_exec_pages_;
  } else if (exec_pages_.IsValid()) {
    code_heap = &exec_pages_;
  }
  if (code_heap != nullptr) {
    // Make all pages reserved for the code heap writable. The mspace allocator, that manages the
    // heap, will take and initialize pages in create_mspace_with_base().
    CheckedCall(mprotect, "create code heap", code_heap->Begin(), code_heap->Size(), kProtRW);
    exec_mspace_ = create_mspace_with_base(code_heap->Begin(), exec_end_, false /*locked*/);
    CHECK(exec_mspace_ != nullptr) << "create_mspace_with_base (exec) failed";
    SetFootprintLimit(initial_capacity_);
    // Protect pages containing heap metadata. Updates to the code heap toggle write permission to
    // perform the update and there are no other times write access is required.
    CheckedCall(mprotect, "protect code heap", code_heap->Begin(), code_heap->Size(), kProtR);
  } else {
    exec_mspace_ = nullptr;
    SetFootprintLimit(initial_capacity_);
  }
}

JitCodeCache::~JitCodeCache() {}

bool JitCodeCache::ContainsPc(const void* ptr) const {
  return exec_pages_.HasAddress(ptr) || zygote_exec_pages_.HasAddress(ptr);
}

bool JitCodeCache::WillExecuteJitCode(ArtMethod* method) {
  ScopedObjectAccess soa(art::Thread::Current());
  ScopedAssertNoThreadSuspension sants(__FUNCTION__);
  if (ContainsPc(method->GetEntryPointFromQuickCompiledCode())) {
    return true;
  } else if (method->GetEntryPointFromQuickCompiledCode() == GetQuickInstrumentationEntryPoint()) {
    return FindCompiledCodeForInstrumentation(method) != nullptr;
  }
  return false;
}

bool JitCodeCache::ContainsMethod(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  if (UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    if (it != jni_stubs_map_.end() &&
        it->second.IsCompiled() &&
        ContainsElement(it->second.GetMethods(), method)) {
      return true;
    }
  } else {
    for (const auto& it : method_code_map_) {
      if (it.second == method) {
        return true;
      }
    }
  }
  return false;
}

const void* JitCodeCache::GetJniStubCode(ArtMethod* method) {
  DCHECK(method->IsNative());
  MutexLock mu(Thread::Current(), lock_);
  auto it = jni_stubs_map_.find(JniStubKey(method));
  if (it != jni_stubs_map_.end()) {
    JniStubData& data = it->second;
    if (data.IsCompiled() && ContainsElement(data.GetMethods(), method)) {
      return data.GetCode();
    }
  }
  return nullptr;
}

const void* JitCodeCache::FindCompiledCodeForInstrumentation(ArtMethod* method) {
  // If jit-gc is still on we use the SavedEntryPoint field for doing that and so cannot use it to
  // find the instrumentation entrypoint.
  if (LIKELY(GetGarbageCollectCode())) {
    return nullptr;
  }
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  if (info == nullptr) {
    return nullptr;
  }
  // When GC is disabled for trampoline tracing we will use SavedEntrypoint to hold the actual
  // jit-compiled version of the method. If jit-gc is disabled for other reasons this will just be
  // nullptr.
  return info->GetSavedEntryPoint();
}

const void* JitCodeCache::GetZygoteSavedEntryPoint(ArtMethod* method) {
  if (Runtime::Current()->IsUsingApexBootImageLocation() &&
      // Currently only applies to boot classpath
      method->GetDeclaringClass()->GetClassLoader() == nullptr) {
    const void* entry_point = nullptr;
    if (method->IsNative()) {
      const void* code_ptr = GetJniStubCode(method);
      if (code_ptr != nullptr) {
        entry_point = OatQuickMethodHeader::FromCodePointer(code_ptr)->GetEntryPoint();
      }
    } else {
      ProfilingInfo* profiling_info = method->GetProfilingInfo(kRuntimePointerSize);
      if (profiling_info != nullptr) {
        entry_point = profiling_info->GetSavedEntryPoint();
      }
    }
    if (Runtime::Current()->IsZygote() || IsInZygoteExecSpace(entry_point)) {
      return entry_point;
    }
  }
  return nullptr;
}

class ScopedCodeCacheWrite : ScopedTrace {
 public:
  explicit ScopedCodeCacheWrite(const JitCodeCache* const code_cache)
      : ScopedTrace("ScopedCodeCacheWrite"),
        code_cache_(code_cache) {
    ScopedTrace trace("mprotect all");
    const MemMap* const updatable_pages = code_cache_->GetUpdatableCodeMapping();
    if (updatable_pages != nullptr) {
      int prot = code_cache_->HasDualCodeMapping() ? kProtRW : kProtRWX;
      CheckedCall(mprotect, "Cache +W", updatable_pages->Begin(), updatable_pages->Size(), prot);
    }
  }

  ~ScopedCodeCacheWrite() {
    ScopedTrace trace("mprotect code");
    const MemMap* const updatable_pages = code_cache_->GetUpdatableCodeMapping();
    if (updatable_pages != nullptr) {
      int prot = code_cache_->HasDualCodeMapping() ? kProtR : kProtRX;
      CheckedCall(mprotect, "Cache -W", updatable_pages->Begin(), updatable_pages->Size(), prot);
    }
  }

 private:
  const JitCodeCache* const code_cache_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCodeCacheWrite);
};

uint8_t* JitCodeCache::CommitCode(Thread* self,
                                  ArtMethod* method,
                                  uint8_t* stack_map,
                                  uint8_t* roots_data,
                                  const uint8_t* code,
                                  size_t code_size,
                                  size_t data_size,
                                  bool osr,
                                  const std::vector<Handle<mirror::Object>>& roots,
                                  bool has_should_deoptimize_flag,
                                  const ArenaSet<ArtMethod*>& cha_single_implementation_list) {
  uint8_t* result = CommitCodeInternal(self,
                                       method,
                                       stack_map,
                                       roots_data,
                                       code,
                                       code_size,
                                       data_size,
                                       osr,
                                       roots,
                                       has_should_deoptimize_flag,
                                       cha_single_implementation_list);
  if (result == nullptr) {
    // Retry.
    GarbageCollectCache(self);
    result = CommitCodeInternal(self,
                                method,
                                stack_map,
                                roots_data,
                                code,
                                code_size,
                                data_size,
                                osr,
                                roots,
                                has_should_deoptimize_flag,
                                cha_single_implementation_list);
  }
  return result;
}

bool JitCodeCache::WaitForPotentialCollectionToComplete(Thread* self) {
  bool in_collection = false;
  while (collection_in_progress_) {
    in_collection = true;
    lock_cond_.Wait(self);
  }
  return in_collection;
}

static size_t GetJitCodeAlignment() {
  if (kRuntimeISA == InstructionSet::kArm || kRuntimeISA == InstructionSet::kThumb2) {
    // Some devices with 32-bit ARM kernels need additional JIT code alignment when using dual
    // view JIT (b/132205399). The alignment returned here coincides with the typical ARM d-cache
    // line (though the value should be probed ideally). Both the method header and code in the
    // cache are aligned to this size. Anything less than 64-bytes exhibits the problem.
    return 64;
  }
  return GetInstructionSetAlignment(kRuntimeISA);
}

static uintptr_t FromCodeToAllocation(const void* code) {
  size_t alignment = GetJitCodeAlignment();
  return reinterpret_cast<uintptr_t>(code) - RoundUp(sizeof(OatQuickMethodHeader), alignment);
}

static uint32_t ComputeRootTableSize(uint32_t number_of_roots) {
  return sizeof(uint32_t) + number_of_roots * sizeof(GcRoot<mirror::Object>);
}

static uint32_t GetNumberOfRoots(const uint8_t* stack_map) {
  // The length of the table is stored just before the stack map (and therefore at the end of
  // the table itself), in order to be able to fetch it from a `stack_map` pointer.
  return reinterpret_cast<const uint32_t*>(stack_map)[-1];
}

static void FillRootTableLength(uint8_t* roots_data, uint32_t length) {
  // Store the length of the table at the end. This will allow fetching it from a `stack_map`
  // pointer.
  reinterpret_cast<uint32_t*>(roots_data)[length] = length;
}

static const uint8_t* FromStackMapToRoots(const uint8_t* stack_map_data) {
  return stack_map_data - ComputeRootTableSize(GetNumberOfRoots(stack_map_data));
}

static void DCheckRootsAreValid(const std::vector<Handle<mirror::Object>>& roots)
    REQUIRES(!Locks::intern_table_lock_) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (!kIsDebugBuild) {
    return;
  }
  // Put all roots in `roots_data`.
  for (Handle<mirror::Object> object : roots) {
    // Ensure the string is strongly interned. b/32995596
    if (object->IsString()) {
      ObjPtr<mirror::String> str = object->AsString();
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      CHECK(class_linker->GetInternTable()->LookupStrong(Thread::Current(), str) != nullptr);
    }
  }
}

void JitCodeCache::FillRootTable(uint8_t* roots_data,
                                 const std::vector<Handle<mirror::Object>>& roots) {
  GcRoot<mirror::Object>* gc_roots = reinterpret_cast<GcRoot<mirror::Object>*>(roots_data);
  const uint32_t length = roots.size();
  // Put all roots in `roots_data`.
  for (uint32_t i = 0; i < length; ++i) {
    ObjPtr<mirror::Object> object = roots[i].Get();
    gc_roots[i] = GcRoot<mirror::Object>(object);
  }
}

static uint8_t* GetRootTable(const void* code_ptr, uint32_t* number_of_roots = nullptr) {
  OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
  uint8_t* data = method_header->GetOptimizedCodeInfoPtr();
  uint32_t roots = GetNumberOfRoots(data);
  if (number_of_roots != nullptr) {
    *number_of_roots = roots;
  }
  return data - ComputeRootTableSize(roots);
}

// Use a sentinel for marking entries in the JIT table that have been cleared.
// This helps diagnosing in case the compiled code tries to wrongly access such
// entries.
static mirror::Class* const weak_sentinel =
    reinterpret_cast<mirror::Class*>(Context::kBadGprBase + 0xff);

// Helper for the GC to process a weak class in a JIT root table.
static inline void ProcessWeakClass(GcRoot<mirror::Class>* root_ptr,
                                    IsMarkedVisitor* visitor,
                                    mirror::Class* update)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // This does not need a read barrier because this is called by GC.
  mirror::Class* cls = root_ptr->Read<kWithoutReadBarrier>();
  if (cls != nullptr && cls != weak_sentinel) {
    DCHECK((cls->IsClass<kDefaultVerifyFlags>()));
    // Look at the classloader of the class to know if it has been unloaded.
    // This does not need a read barrier because this is called by GC.
    ObjPtr<mirror::Object> class_loader =
        cls->GetClassLoader<kDefaultVerifyFlags, kWithoutReadBarrier>();
    if (class_loader == nullptr || visitor->IsMarked(class_loader.Ptr()) != nullptr) {
      // The class loader is live, update the entry if the class has moved.
      mirror::Class* new_cls = down_cast<mirror::Class*>(visitor->IsMarked(cls));
      // Note that new_object can be null for CMS and newly allocated objects.
      if (new_cls != nullptr && new_cls != cls) {
        *root_ptr = GcRoot<mirror::Class>(new_cls);
      }
    } else {
      // The class loader is not live, clear the entry.
      *root_ptr = GcRoot<mirror::Class>(update);
    }
  }
}

void JitCodeCache::SweepRootTables(IsMarkedVisitor* visitor) {
  MutexLock mu(Thread::Current(), lock_);
  for (const auto& entry : method_code_map_) {
    uint32_t number_of_roots = 0;
    uint8_t* roots_data = GetRootTable(entry.first, &number_of_roots);
    GcRoot<mirror::Object>* roots = reinterpret_cast<GcRoot<mirror::Object>*>(roots_data);
    for (uint32_t i = 0; i < number_of_roots; ++i) {
      // This does not need a read barrier because this is called by GC.
      mirror::Object* object = roots[i].Read<kWithoutReadBarrier>();
      if (object == nullptr || object == weak_sentinel) {
        // entry got deleted in a previous sweep.
      } else if (object->IsString<kDefaultVerifyFlags>()) {
        mirror::Object* new_object = visitor->IsMarked(object);
        // We know the string is marked because it's a strongly-interned string that
        // is always alive. The IsMarked implementation of the CMS collector returns
        // null for newly allocated objects, but we know those haven't moved. Therefore,
        // only update the entry if we get a different non-null string.
        // TODO: Do not use IsMarked for j.l.Class, and adjust once we move this method
        // out of the weak access/creation pause. b/32167580
        if (new_object != nullptr && new_object != object) {
          DCHECK(new_object->IsString());
          roots[i] = GcRoot<mirror::Object>(new_object);
        }
      } else {
        ProcessWeakClass(
            reinterpret_cast<GcRoot<mirror::Class>*>(&roots[i]), visitor, weak_sentinel);
      }
    }
  }
  // Walk over inline caches to clear entries containing unloaded classes.
  for (ProfilingInfo* info : profiling_infos_) {
    for (size_t i = 0; i < info->number_of_inline_caches_; ++i) {
      InlineCache* cache = &info->cache_[i];
      for (size_t j = 0; j < InlineCache::kIndividualCacheSize; ++j) {
        ProcessWeakClass(&cache->classes_[j], visitor, nullptr);
      }
    }
  }
}

void JitCodeCache::FreeCodeAndData(const void* code_ptr) {
  if (IsInZygoteExecSpace(code_ptr)) {
    // No need to free, this is shared memory.
    return;
  }
  uintptr_t allocation = FromCodeToAllocation(code_ptr);
  // Notify native debugger that we are about to remove the code.
  // It does nothing if we are not using native debugger.
  RemoveNativeDebugInfoForJit(Thread::Current(), code_ptr);
  if (OatQuickMethodHeader::FromCodePointer(code_ptr)->IsOptimized()) {
    FreeData(GetRootTable(code_ptr));
  }  // else this is a JNI stub without any data.

  uint8_t* code_allocation = reinterpret_cast<uint8_t*>(allocation);
  if (HasDualCodeMapping()) {
    code_allocation = TranslateAddress(code_allocation, exec_pages_, non_exec_pages_);
  }

  FreeCode(code_allocation);
}

void JitCodeCache::FreeAllMethodHeaders(
    const std::unordered_set<OatQuickMethodHeader*>& method_headers) {
  // We need to remove entries in method_headers from CHA dependencies
  // first since once we do FreeCode() below, the memory can be reused
  // so it's possible for the same method_header to start representing
  // different compile code.
  MutexLock mu(Thread::Current(), lock_);
  {
    MutexLock mu2(Thread::Current(), *Locks::cha_lock_);
    Runtime::Current()->GetClassLinker()->GetClassHierarchyAnalysis()
        ->RemoveDependentsWithMethodHeaders(method_headers);
  }

  ScopedCodeCacheWrite scc(this);
  for (const OatQuickMethodHeader* method_header : method_headers) {
    FreeCodeAndData(method_header->GetCode());
  }
}

void JitCodeCache::RemoveMethodsIn(Thread* self, const LinearAlloc& alloc) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  // We use a set to first collect all method_headers whose code need to be
  // removed. We need to free the underlying code after we remove CHA dependencies
  // for entries in this set. And it's more efficient to iterate through
  // the CHA dependency map just once with an unordered_set.
  std::unordered_set<OatQuickMethodHeader*> method_headers;
  {
    MutexLock mu(self, lock_);
    // We do not check if a code cache GC is in progress, as this method comes
    // with the classlinker_classes_lock_ held, and suspending ourselves could
    // lead to a deadlock.
    {
      ScopedCodeCacheWrite scc(this);
      for (auto it = jni_stubs_map_.begin(); it != jni_stubs_map_.end();) {
        it->second.RemoveMethodsIn(alloc);
        if (it->second.GetMethods().empty()) {
          method_headers.insert(OatQuickMethodHeader::FromCodePointer(it->second.GetCode()));
          it = jni_stubs_map_.erase(it);
        } else {
          it->first.UpdateShorty(it->second.GetMethods().front());
          ++it;
        }
      }
      for (auto it = method_code_map_.begin(); it != method_code_map_.end();) {
        if (alloc.ContainsUnsafe(it->second)) {
          method_headers.insert(OatQuickMethodHeader::FromCodePointer(it->first));
          it = method_code_map_.erase(it);
        } else {
          ++it;
        }
      }
    }
    for (auto it = osr_code_map_.begin(); it != osr_code_map_.end();) {
      if (alloc.ContainsUnsafe(it->first)) {
        // Note that the code has already been pushed to method_headers in the loop
        // above and is going to be removed in FreeCode() below.
        it = osr_code_map_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = profiling_infos_.begin(); it != profiling_infos_.end();) {
      ProfilingInfo* info = *it;
      if (alloc.ContainsUnsafe(info->GetMethod())) {
        info->GetMethod()->SetProfilingInfo(nullptr);
        FreeData(reinterpret_cast<uint8_t*>(info));
        it = profiling_infos_.erase(it);
      } else {
        ++it;
      }
    }
  }
  FreeAllMethodHeaders(method_headers);
}

bool JitCodeCache::IsWeakAccessEnabled(Thread* self) const {
  return kUseReadBarrier
      ? self->GetWeakRefAccessEnabled()
      : is_weak_access_enabled_.load(std::memory_order_seq_cst);
}

void JitCodeCache::WaitUntilInlineCacheAccessible(Thread* self) {
  if (IsWeakAccessEnabled(self)) {
    return;
  }
  ScopedThreadSuspension sts(self, kWaitingWeakGcRootRead);
  MutexLock mu(self, lock_);
  while (!IsWeakAccessEnabled(self)) {
    inline_cache_cond_.Wait(self);
  }
}

void JitCodeCache::BroadcastForInlineCacheAccess() {
  Thread* self = Thread::Current();
  MutexLock mu(self, lock_);
  inline_cache_cond_.Broadcast(self);
}

void JitCodeCache::AllowInlineCacheAccess() {
  DCHECK(!kUseReadBarrier);
  is_weak_access_enabled_.store(true, std::memory_order_seq_cst);
  BroadcastForInlineCacheAccess();
}

void JitCodeCache::DisallowInlineCacheAccess() {
  DCHECK(!kUseReadBarrier);
  is_weak_access_enabled_.store(false, std::memory_order_seq_cst);
}

void JitCodeCache::CopyInlineCacheInto(const InlineCache& ic,
                                       Handle<mirror::ObjectArray<mirror::Class>> array) {
  WaitUntilInlineCacheAccessible(Thread::Current());
  // Note that we don't need to lock `lock_` here, the compiler calling
  // this method has already ensured the inline cache will not be deleted.
  for (size_t in_cache = 0, in_array = 0;
       in_cache < InlineCache::kIndividualCacheSize;
       ++in_cache) {
    mirror::Class* object = ic.classes_[in_cache].Read();
    if (object != nullptr) {
      array->Set(in_array++, object);
    }
  }
}

static void ClearMethodCounter(ArtMethod* method, bool was_warm)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  if (was_warm) {
    method->SetPreviouslyWarm();
  }
  // We reset the counter to 1 so that the profile knows that the method was executed at least once.
  // This is required for layout purposes.
  // We also need to make sure we'll pass the warmup threshold again, so we set to 0 if
  // the warmup threshold is 1.
  uint16_t jit_warmup_threshold = Runtime::Current()->GetJITOptions()->GetWarmupThreshold();
  method->SetCounter(std::min(jit_warmup_threshold - 1, 1));
}

void JitCodeCache::WaitForPotentialCollectionToCompleteRunnable(Thread* self) {
  while (collection_in_progress_) {
    lock_.Unlock(self);
    {
      ScopedThreadSuspension sts(self, kSuspended);
      MutexLock mu(self, lock_);
      WaitForPotentialCollectionToComplete(self);
    }
    lock_.Lock(self);
  }
}

const MemMap* JitCodeCache::GetUpdatableCodeMapping() const {
  if (HasDualCodeMapping()) {
    return &non_exec_pages_;
  } else if (HasCodeMapping()) {
    return &exec_pages_;
  } else {
    return nullptr;
  }
}

uint8_t* JitCodeCache::CommitCodeInternal(Thread* self,
                                          ArtMethod* method,
                                          uint8_t* stack_map,
                                          uint8_t* roots_data,
                                          const uint8_t* code,
                                          size_t code_size,
                                          size_t data_size,
                                          bool osr,
                                          const std::vector<Handle<mirror::Object>>& roots,
                                          bool has_should_deoptimize_flag,
                                          const ArenaSet<ArtMethod*>&
                                              cha_single_implementation_list) {
  DCHECK(!method->IsNative() || !osr);

  if (!method->IsNative()) {
    // We need to do this before grabbing the lock_ because it needs to be able to see the string
    // InternTable. Native methods do not have roots.
    DCheckRootsAreValid(roots);
  }

  OatQuickMethodHeader* method_header = nullptr;
  uint8_t* nox_memory = nullptr;
  uint8_t* code_ptr = nullptr;

  MutexLock mu(self, lock_);
  // We need to make sure that there will be no jit-gcs going on and wait for any ongoing one to
  // finish.
  WaitForPotentialCollectionToCompleteRunnable(self);
  {
    ScopedCodeCacheWrite scc(this);

    size_t alignment = GetJitCodeAlignment();
    // Ensure the header ends up at expected instruction alignment.
    size_t header_size = RoundUp(sizeof(OatQuickMethodHeader), alignment);
    size_t total_size = header_size + code_size;

    // AllocateCode allocates memory in non-executable region for alignment header and code. The
    // header size may include alignment padding.
    nox_memory = AllocateCode(total_size);
    if (nox_memory == nullptr) {
      return nullptr;
    }

    // code_ptr points to non-executable code.
    code_ptr = nox_memory + header_size;
    std::copy(code, code + code_size, code_ptr);
    method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);

    // From here code_ptr points to executable code.
    if (HasDualCodeMapping()) {
      code_ptr = TranslateAddress(code_ptr, non_exec_pages_, exec_pages_);
    }

    new (method_header) OatQuickMethodHeader(
        (stack_map != nullptr) ? code_ptr - stack_map : 0u,
        code_size);

    DCHECK(!Runtime::Current()->IsAotCompiler());
    if (has_should_deoptimize_flag) {
      method_header->SetHasShouldDeoptimizeFlag();
    }

    // Update method_header pointer to executable code region.
    if (HasDualCodeMapping()) {
      method_header = TranslateAddress(method_header, non_exec_pages_, exec_pages_);
    }

    // Both instruction and data caches need flushing to the point of unification where both share
    // a common view of memory. Flushing the data cache ensures the dirty cachelines from the
    // newly added code are written out to the point of unification. Flushing the instruction
    // cache ensures the newly written code will be fetched from the point of unification before
    // use. Memory in the code cache is re-cycled as code is added and removed. The flushes
    // prevent stale code from residing in the instruction cache.
    //
    // Caches are flushed before write permission is removed because some ARMv8 Qualcomm kernels
    // may trigger a segfault if a page fault occurs when requesting a cache maintenance
    // operation. This is a kernel bug that we need to work around until affected devices
    // (e.g. Nexus 5X and 6P) stop being supported or their kernels are fixed.
    //
    // For reference, this behavior is caused by this commit:
    // https://android.googlesource.com/kernel/msm/+/3fbe6bc28a6b9939d0650f2f17eb5216c719950c
    //
    bool cache_flush_success = true;
    if (HasDualCodeMapping()) {
      // Flush the data cache lines associated with the non-executable copy of the code just added.
      cache_flush_success = FlushCpuCaches(nox_memory, nox_memory + total_size);
    }

    // Invalidate i-cache for the executable mapping.
    if (cache_flush_success) {
      uint8_t* x_memory = reinterpret_cast<uint8_t*>(FromCodeToAllocation(code_ptr));
      cache_flush_success = FlushCpuCaches(x_memory, x_memory + total_size);
    }

    // If flushing the cache has failed, reject the allocation because we can't guarantee
    // correctness of the instructions present in the processor caches.
    if (!cache_flush_success) {
      PLOG(ERROR) << "Cache flush failed for JIT code, code not committed.";
      FreeCode(nox_memory);
      return nullptr;
    }

    // Ensure CPU instruction pipelines are flushed for all cores. This is necessary for
    // correctness as code may still be in instruction pipelines despite the i-cache flush. It is
    // not safe to assume that changing permissions with mprotect (RX->RWX->RX) will cause a TLB
    // shootdown (incidentally invalidating the CPU pipelines by sending an IPI to all cores to
    // notify them of the TLB invalidation). Some architectures, notably ARM and ARM64, have
    // hardware support that broadcasts TLB invalidations and so their kernels have no software
    // based TLB shootdown. The sync-core flavor of membarrier was introduced in Linux 4.16 to
    // address this (see mbarrier(2)). The membarrier here will fail on prior kernels and on
    // platforms lacking the appropriate support.
    art::membarrier(art::MembarrierCommand::kPrivateExpeditedSyncCore);

    number_of_compilations_++;
  }

  // We need to update the entry point in the runnable state for the instrumentation.
  {
    // The following needs to be guarded by cha_lock_ also. Otherwise it's possible that the
    // compiled code is considered invalidated by some class linking, but below we still make the
    // compiled code valid for the method.  Need cha_lock_ for checking all single-implementation
    // flags and register dependencies.
    MutexLock cha_mu(self, *Locks::cha_lock_);
    bool single_impl_still_valid = true;
    for (ArtMethod* single_impl : cha_single_implementation_list) {
      if (!single_impl->HasSingleImplementation()) {
        // Simply discard the compiled code. Clear the counter so that it may be recompiled later.
        // Hopefully the class hierarchy will be more stable when compilation is retried.
        single_impl_still_valid = false;
        ClearMethodCounter(method, /*was_warm=*/ false);
        break;
      }
    }

    // Discard the code if any single-implementation assumptions are now invalid.
    if (!single_impl_still_valid) {
      VLOG(jit) << "JIT discarded jitted code due to invalid single-implementation assumptions.";
      return nullptr;
    }
    DCHECK(cha_single_implementation_list.empty() || !Runtime::Current()->IsJavaDebuggable())
        << "Should not be using cha on debuggable apps/runs!";

    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    for (ArtMethod* single_impl : cha_single_implementation_list) {
      class_linker->GetClassHierarchyAnalysis()->AddDependency(single_impl, method, method_header);
    }

    if (UNLIKELY(method->IsNative())) {
      auto it = jni_stubs_map_.find(JniStubKey(method));
      DCHECK(it != jni_stubs_map_.end())
          << "Entry inserted in NotifyCompilationOf() should be alive.";
      JniStubData* data = &it->second;
      DCHECK(ContainsElement(data->GetMethods(), method))
          << "Entry inserted in NotifyCompilationOf() should contain this method.";
      data->SetCode(code_ptr);
      instrumentation::Instrumentation* instrum = Runtime::Current()->GetInstrumentation();
      for (ArtMethod* m : data->GetMethods()) {
        if (!class_linker->IsQuickResolutionStub(m->GetEntryPointFromQuickCompiledCode())) {
          instrum->UpdateMethodsCode(m, method_header->GetEntryPoint());
        }
      }
    } else {
      // Fill the root table before updating the entry point.
      DCHECK_EQ(FromStackMapToRoots(stack_map), roots_data);
      DCHECK_LE(roots_data, stack_map);
      FillRootTable(roots_data, roots);
      {
        // Flush data cache, as compiled code references literals in it.
        // TODO(oth): establish whether this is necessary.
        if (!FlushCpuCaches(roots_data, roots_data + data_size)) {
          PLOG(ERROR) << "Cache flush failed for JIT data, code not committed.";
          ScopedCodeCacheWrite scc(this);
          FreeCode(nox_memory);
          return nullptr;
        }
      }
      method_code_map_.Put(code_ptr, method);
      if (osr) {
        number_of_osr_compilations_++;
        osr_code_map_.Put(method, code_ptr);
      } else if (class_linker->IsQuickResolutionStub(
          method->GetEntryPointFromQuickCompiledCode())) {
        // This situation currently only occurs in the jit-zygote mode.
        DCHECK(Runtime::Current()->IsZygote());
        DCHECK(Runtime::Current()->IsUsingApexBootImageLocation());
        DCHECK(method->GetProfilingInfo(kRuntimePointerSize) != nullptr);
        DCHECK(method->GetDeclaringClass()->GetClassLoader() == nullptr);
        // Save the entrypoint, so it can be fethed later once the class is
        // initialized.
        method->GetProfilingInfo(kRuntimePointerSize)->SetSavedEntryPoint(
            method_header->GetEntryPoint());
      } else {
        Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
            method, method_header->GetEntryPoint());
      }
    }
    VLOG(jit)
        << "JIT added (osr=" << std::boolalpha << osr << std::noboolalpha << ") "
        << ArtMethod::PrettyMethod(method) << "@" << method
        << " ccache_size=" << PrettySize(CodeCacheSizeLocked()) << ": "
        << " dcache_size=" << PrettySize(DataCacheSizeLocked()) << ": "
        << reinterpret_cast<const void*>(method_header->GetEntryPoint()) << ","
        << reinterpret_cast<const void*>(method_header->GetEntryPoint() +
                                         method_header->GetCodeSize());
    histogram_code_memory_use_.AddValue(code_size);
    if (code_size > kCodeSizeLogThreshold) {
      LOG(INFO) << "JIT allocated "
                << PrettySize(code_size)
                << " for compiled code of "
                << ArtMethod::PrettyMethod(method);
    }
  }

  return reinterpret_cast<uint8_t*>(method_header);
}

size_t JitCodeCache::CodeCacheSize() {
  MutexLock mu(Thread::Current(), lock_);
  return CodeCacheSizeLocked();
}

bool JitCodeCache::RemoveMethod(ArtMethod* method, bool release_memory) {
  // This function is used only for testing and only with non-native methods.
  CHECK(!method->IsNative());

  MutexLock mu(Thread::Current(), lock_);

  bool osr = osr_code_map_.find(method) != osr_code_map_.end();
  bool in_cache = RemoveMethodLocked(method, release_memory);

  if (!in_cache) {
    return false;
  }

  method->SetCounter(0);
  Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
      method, GetQuickToInterpreterBridge());
  VLOG(jit)
      << "JIT removed (osr=" << std::boolalpha << osr << std::noboolalpha << ") "
      << ArtMethod::PrettyMethod(method) << "@" << method
      << " ccache_size=" << PrettySize(CodeCacheSizeLocked()) << ": "
      << " dcache_size=" << PrettySize(DataCacheSizeLocked());
  return true;
}

bool JitCodeCache::RemoveMethodLocked(ArtMethod* method, bool release_memory) {
  if (LIKELY(!method->IsNative())) {
    ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
    if (info != nullptr) {
      RemoveElement(profiling_infos_, info);
    }
    method->SetProfilingInfo(nullptr);
  }

  bool in_cache = false;
  ScopedCodeCacheWrite ccw(this);
  if (UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    if (it != jni_stubs_map_.end() && it->second.RemoveMethod(method)) {
      in_cache = true;
      if (it->second.GetMethods().empty()) {
        if (release_memory) {
          FreeCodeAndData(it->second.GetCode());
        }
        jni_stubs_map_.erase(it);
      } else {
        it->first.UpdateShorty(it->second.GetMethods().front());
      }
    }
  } else {
    for (auto it = method_code_map_.begin(); it != method_code_map_.end();) {
      if (it->second == method) {
        in_cache = true;
        if (release_memory) {
          FreeCodeAndData(it->first);
        }
        it = method_code_map_.erase(it);
      } else {
        ++it;
      }
    }

    auto osr_it = osr_code_map_.find(method);
    if (osr_it != osr_code_map_.end()) {
      osr_code_map_.erase(osr_it);
    }
  }

  return in_cache;
}

// This notifies the code cache that the given method has been redefined and that it should remove
// any cached information it has on the method. All threads must be suspended before calling this
// method. The compiled code for the method (if there is any) must not be in any threads call stack.
void JitCodeCache::NotifyMethodRedefined(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  RemoveMethodLocked(method, /* release_memory= */ true);
}

// This invalidates old_method. Once this function returns one can no longer use old_method to
// execute code unless it is fixed up. This fixup will happen later in the process of installing a
// class redefinition.
// TODO We should add some info to ArtMethod to note that 'old_method' has been invalidated and
// shouldn't be used since it is no longer logically in the jit code cache.
// TODO We should add DCHECKS that validate that the JIT is paused when this method is entered.
void JitCodeCache::MoveObsoleteMethod(ArtMethod* old_method, ArtMethod* new_method) {
  MutexLock mu(Thread::Current(), lock_);
  if (old_method->IsNative()) {
    // Update methods in jni_stubs_map_.
    for (auto& entry : jni_stubs_map_) {
      JniStubData& data = entry.second;
      data.MoveObsoleteMethod(old_method, new_method);
    }
    return;
  }
  // Update ProfilingInfo to the new one and remove it from the old_method.
  if (old_method->GetProfilingInfo(kRuntimePointerSize) != nullptr) {
    DCHECK_EQ(old_method->GetProfilingInfo(kRuntimePointerSize)->GetMethod(), old_method);
    ProfilingInfo* info = old_method->GetProfilingInfo(kRuntimePointerSize);
    old_method->SetProfilingInfo(nullptr);
    // Since the JIT should be paused and all threads suspended by the time this is called these
    // checks should always pass.
    DCHECK(!info->IsInUseByCompiler());
    new_method->SetProfilingInfo(info);
    // Get rid of the old saved entrypoint if it is there.
    info->SetSavedEntryPoint(nullptr);
    info->method_ = new_method;
  }
  // Update method_code_map_ to point to the new method.
  for (auto& it : method_code_map_) {
    if (it.second == old_method) {
      it.second = new_method;
    }
  }
  // Update osr_code_map_ to point to the new method.
  auto code_map = osr_code_map_.find(old_method);
  if (code_map != osr_code_map_.end()) {
    osr_code_map_.Put(new_method, code_map->second);
    osr_code_map_.erase(old_method);
  }
}

void JitCodeCache::ClearEntryPointsInZygoteExecSpace() {
  MutexLock mu(Thread::Current(), lock_);
  // Iterate over profiling infos to know which methods may have been JITted. Note that
  // to be JITted, a method must have a profiling info.
  for (ProfilingInfo* info : profiling_infos_) {
    ArtMethod* method = info->GetMethod();
    if (IsInZygoteExecSpace(method->GetEntryPointFromQuickCompiledCode())) {
      method->SetEntryPointFromQuickCompiledCode(GetQuickToInterpreterBridge());
    }
    // If zygote does method tracing, or in some configuration where
    // the JIT zygote does GC, we also need to clear the saved entry point
    // in the profiling info.
    if (IsInZygoteExecSpace(info->GetSavedEntryPoint())) {
      info->SetSavedEntryPoint(nullptr);
    }
  }
}

size_t JitCodeCache::CodeCacheSizeLocked() {
  return used_memory_for_code_;
}

size_t JitCodeCache::DataCacheSize() {
  MutexLock mu(Thread::Current(), lock_);
  return DataCacheSizeLocked();
}

size_t JitCodeCache::DataCacheSizeLocked() {
  return used_memory_for_data_;
}

void JitCodeCache::ClearData(Thread* self,
                             uint8_t* stack_map_data,
                             uint8_t* roots_data) {
  DCHECK_EQ(FromStackMapToRoots(stack_map_data), roots_data);
  MutexLock mu(self, lock_);
  FreeData(reinterpret_cast<uint8_t*>(roots_data));
}

size_t JitCodeCache::ReserveData(Thread* self,
                                 size_t stack_map_size,
                                 size_t number_of_roots,
                                 ArtMethod* method,
                                 uint8_t** stack_map_data,
                                 uint8_t** roots_data) {
  size_t table_size = ComputeRootTableSize(number_of_roots);
  size_t size = RoundUp(stack_map_size + table_size, sizeof(void*));
  uint8_t* result = nullptr;

  {
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    WaitForPotentialCollectionToComplete(self);
    result = AllocateData(size);
  }

  if (result == nullptr) {
    // Retry.
    GarbageCollectCache(self);
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    WaitForPotentialCollectionToComplete(self);
    result = AllocateData(size);
  }

  MutexLock mu(self, lock_);
  histogram_stack_map_memory_use_.AddValue(size);
  if (size > kStackMapSizeLogThreshold) {
    LOG(INFO) << "JIT allocated "
              << PrettySize(size)
              << " for stack maps of "
              << ArtMethod::PrettyMethod(method);
  }
  if (result != nullptr) {
    *roots_data = result;
    *stack_map_data = result + table_size;
    FillRootTableLength(*roots_data, number_of_roots);
    return size;
  } else {
    *roots_data = nullptr;
    *stack_map_data = nullptr;
    return 0;
  }
}

class MarkCodeClosure final : public Closure {
 public:
  MarkCodeClosure(JitCodeCache* code_cache, CodeCacheBitmap* bitmap, Barrier* barrier)
      : code_cache_(code_cache), bitmap_(bitmap), barrier_(barrier) {}

  void Run(Thread* thread) override REQUIRES_SHARED(Locks::mutator_lock_) {
    ScopedTrace trace(__PRETTY_FUNCTION__);
    DCHECK(thread == Thread::Current() || thread->IsSuspended());
    StackVisitor::WalkStack(
        [&](const art::StackVisitor* stack_visitor) {
          const OatQuickMethodHeader* method_header =
              stack_visitor->GetCurrentOatQuickMethodHeader();
          if (method_header == nullptr) {
            return true;
          }
          const void* code = method_header->GetCode();
          if (code_cache_->ContainsPc(code) && !code_cache_->IsInZygoteExecSpace(code)) {
            // Use the atomic set version, as multiple threads are executing this code.
            bitmap_->AtomicTestAndSet(FromCodeToAllocation(code));
          }
          return true;
        },
        thread,
        /* context= */ nullptr,
        art::StackVisitor::StackWalkKind::kSkipInlinedFrames);

    if (kIsDebugBuild) {
      // The stack walking code queries the side instrumentation stack if it
      // sees an instrumentation exit pc, so the JIT code of methods in that stack
      // must have been seen. We sanity check this below.
      for (const instrumentation::InstrumentationStackFrame& frame
              : *thread->GetInstrumentationStack()) {
        // The 'method_' in InstrumentationStackFrame is the one that has return_pc_ in
        // its stack frame, it is not the method owning return_pc_. We just pass null to
        // LookupMethodHeader: the method is only checked against in debug builds.
        OatQuickMethodHeader* method_header =
            code_cache_->LookupMethodHeader(frame.return_pc_, /* method= */ nullptr);
        if (method_header != nullptr) {
          const void* code = method_header->GetCode();
          CHECK(bitmap_->Test(FromCodeToAllocation(code)));
        }
      }
    }
    barrier_->Pass(Thread::Current());
  }

 private:
  JitCodeCache* const code_cache_;
  CodeCacheBitmap* const bitmap_;
  Barrier* const barrier_;
};

void JitCodeCache::NotifyCollectionDone(Thread* self) {
  collection_in_progress_ = false;
  lock_cond_.Broadcast(self);
}

void JitCodeCache::SetFootprintLimit(size_t new_footprint) {
  size_t data_space_footprint = new_footprint / kCodeAndDataCapacityDivider;
  DCHECK(IsAlignedParam(data_space_footprint, kPageSize));
  DCHECK_EQ(data_space_footprint * kCodeAndDataCapacityDivider, new_footprint);
  mspace_set_footprint_limit(data_mspace_, data_space_footprint);
  if (HasCodeMapping()) {
    ScopedCodeCacheWrite scc(this);
    mspace_set_footprint_limit(exec_mspace_, new_footprint - data_space_footprint);
  }
}

bool JitCodeCache::IncreaseCodeCacheCapacity() {
  if (current_capacity_ == max_capacity_) {
    return false;
  }

  // Double the capacity if we're below 1MB, or increase it by 1MB if
  // we're above.
  if (current_capacity_ < 1 * MB) {
    current_capacity_ *= 2;
  } else {
    current_capacity_ += 1 * MB;
  }
  if (current_capacity_ > max_capacity_) {
    current_capacity_ = max_capacity_;
  }

  VLOG(jit) << "Increasing code cache capacity to " << PrettySize(current_capacity_);

  SetFootprintLimit(current_capacity_);

  return true;
}

void JitCodeCache::MarkCompiledCodeOnThreadStacks(Thread* self) {
  Barrier barrier(0);
  size_t threads_running_checkpoint = 0;
  MarkCodeClosure closure(this, GetLiveBitmap(), &barrier);
  threads_running_checkpoint = Runtime::Current()->GetThreadList()->RunCheckpoint(&closure);
  // Now that we have run our checkpoint, move to a suspended state and wait
  // for other threads to run the checkpoint.
  ScopedThreadSuspension sts(self, kSuspended);
  if (threads_running_checkpoint != 0) {
    barrier.Increment(self, threads_running_checkpoint);
  }
}

bool JitCodeCache::ShouldDoFullCollection() {
  if (current_capacity_ == max_capacity_) {
    // Always do a full collection when the code cache is full.
    return true;
  } else if (current_capacity_ < kReservedCapacity) {
    // Always do partial collection when the code cache size is below the reserved
    // capacity.
    return false;
  } else if (last_collection_increased_code_cache_) {
    // This time do a full collection.
    return true;
  } else {
    // This time do a partial collection.
    return false;
  }
}

void JitCodeCache::GarbageCollectCache(Thread* self) {
  ScopedTrace trace(__FUNCTION__);
  // Wait for an existing collection, or let everyone know we are starting one.
  {
    ScopedThreadSuspension sts(self, kSuspended);
    MutexLock mu(self, lock_);
    if (!garbage_collect_code_) {
      IncreaseCodeCacheCapacity();
      return;
    } else if (WaitForPotentialCollectionToComplete(self)) {
      return;
    } else {
      number_of_collections_++;
      live_bitmap_.reset(CodeCacheBitmap::Create(
          "code-cache-bitmap",
          reinterpret_cast<uintptr_t>(exec_pages_.Begin()),
          reinterpret_cast<uintptr_t>(exec_pages_.Begin() + current_capacity_ / 2)));
      collection_in_progress_ = true;
    }
  }

  TimingLogger logger("JIT code cache timing logger", true, VLOG_IS_ON(jit));
  {
    TimingLogger::ScopedTiming st("Code cache collection", &logger);

    bool do_full_collection = false;
    {
      MutexLock mu(self, lock_);
      do_full_collection = ShouldDoFullCollection();
    }

    VLOG(jit) << "Do "
              << (do_full_collection ? "full" : "partial")
              << " code cache collection, code="
              << PrettySize(CodeCacheSize())
              << ", data=" << PrettySize(DataCacheSize());

    DoCollection(self, /* collect_profiling_info= */ do_full_collection);

    VLOG(jit) << "After code cache collection, code="
              << PrettySize(CodeCacheSize())
              << ", data=" << PrettySize(DataCacheSize());

    {
      MutexLock mu(self, lock_);

      // Increase the code cache only when we do partial collections.
      // TODO: base this strategy on how full the code cache is?
      if (do_full_collection) {
        last_collection_increased_code_cache_ = false;
      } else {
        last_collection_increased_code_cache_ = true;
        IncreaseCodeCacheCapacity();
      }

      bool next_collection_will_be_full = ShouldDoFullCollection();

      // Start polling the liveness of compiled code to prepare for the next full collection.
      if (next_collection_will_be_full) {
        // Save the entry point of methods we have compiled, and update the entry
        // point of those methods to the interpreter. If the method is invoked, the
        // interpreter will update its entry point to the compiled code and call it.
        for (ProfilingInfo* info : profiling_infos_) {
          const void* entry_point = info->GetMethod()->GetEntryPointFromQuickCompiledCode();
          if (!IsInZygoteDataSpace(info) && ContainsPc(entry_point)) {
            info->SetSavedEntryPoint(entry_point);
            // Don't call Instrumentation::UpdateMethodsCode(), as it can check the declaring
            // class of the method. We may be concurrently running a GC which makes accessing
            // the class unsafe. We know it is OK to bypass the instrumentation as we've just
            // checked that the current entry point is JIT compiled code.
            info->GetMethod()->SetEntryPointFromQuickCompiledCode(GetQuickToInterpreterBridge());
          }
        }

        DCHECK(CheckLiveCompiledCodeHasProfilingInfo());

        // Change entry points of native methods back to the GenericJNI entrypoint.
        for (const auto& entry : jni_stubs_map_) {
          const JniStubData& data = entry.second;
          if (!data.IsCompiled() || IsInZygoteExecSpace(data.GetCode())) {
            continue;
          }
          // Make sure a single invocation of the GenericJNI trampoline tries to recompile.
          uint16_t new_counter = Runtime::Current()->GetJit()->HotMethodThreshold() - 1u;
          const OatQuickMethodHeader* method_header =
              OatQuickMethodHeader::FromCodePointer(data.GetCode());
          for (ArtMethod* method : data.GetMethods()) {
            if (method->GetEntryPointFromQuickCompiledCode() == method_header->GetEntryPoint()) {
              // Don't call Instrumentation::UpdateMethodsCode(), same as for normal methods above.
              method->SetCounter(new_counter);
              method->SetEntryPointFromQuickCompiledCode(GetQuickGenericJniStub());
            }
          }
        }
      }
      live_bitmap_.reset(nullptr);
      NotifyCollectionDone(self);
    }
  }
  Runtime::Current()->GetJit()->AddTimingLogger(logger);
}

void JitCodeCache::RemoveUnmarkedCode(Thread* self) {
  ScopedTrace trace(__FUNCTION__);
  std::unordered_set<OatQuickMethodHeader*> method_headers;
  {
    MutexLock mu(self, lock_);
    ScopedCodeCacheWrite scc(this);
    // Iterate over all compiled code and remove entries that are not marked.
    for (auto it = jni_stubs_map_.begin(); it != jni_stubs_map_.end();) {
      JniStubData* data = &it->second;
      if (IsInZygoteExecSpace(data->GetCode()) ||
          !data->IsCompiled() ||
          GetLiveBitmap()->Test(FromCodeToAllocation(data->GetCode()))) {
        ++it;
      } else {
        method_headers.insert(OatQuickMethodHeader::FromCodePointer(data->GetCode()));
        it = jni_stubs_map_.erase(it);
      }
    }
    for (auto it = method_code_map_.begin(); it != method_code_map_.end();) {
      const void* code_ptr = it->first;
      uintptr_t allocation = FromCodeToAllocation(code_ptr);
      if (IsInZygoteExecSpace(code_ptr) || GetLiveBitmap()->Test(allocation)) {
        ++it;
      } else {
        OatQuickMethodHeader* header = OatQuickMethodHeader::FromCodePointer(code_ptr);
        method_headers.insert(header);
        it = method_code_map_.erase(it);
      }
    }
  }
  FreeAllMethodHeaders(method_headers);
}

bool JitCodeCache::GetGarbageCollectCode() {
  MutexLock mu(Thread::Current(), lock_);
  return garbage_collect_code_;
}

void JitCodeCache::SetGarbageCollectCode(bool value) {
  Thread* self = Thread::Current();
  MutexLock mu(self, lock_);
  if (garbage_collect_code_ != value) {
    if (garbage_collect_code_) {
      // When dynamically disabling the garbage collection, we neee
      // to make sure that a potential current collection is finished, and also
      // clear the saved entry point in profiling infos to avoid dangling pointers.
      WaitForPotentialCollectionToComplete(self);
      for (ProfilingInfo* info : profiling_infos_) {
        info->SetSavedEntryPoint(nullptr);
      }
    }
    // Update the flag while holding the lock to ensure no thread will try to GC.
    garbage_collect_code_ = value;
  }
}

void JitCodeCache::DoCollection(Thread* self, bool collect_profiling_info) {
  ScopedTrace trace(__FUNCTION__);
  {
    MutexLock mu(self, lock_);
    if (collect_profiling_info) {
      // Clear the profiling info of methods that do not have compiled code as entrypoint.
      // Also remove the saved entry point from the ProfilingInfo objects.
      for (ProfilingInfo* info : profiling_infos_) {
        const void* ptr = info->GetMethod()->GetEntryPointFromQuickCompiledCode();
        if (!ContainsPc(ptr) && !info->IsInUseByCompiler() && !IsInZygoteDataSpace(info)) {
          info->GetMethod()->SetProfilingInfo(nullptr);
        }

        if (info->GetSavedEntryPoint() != nullptr) {
          info->SetSavedEntryPoint(nullptr);
          // We are going to move this method back to interpreter. Clear the counter now to
          // give it a chance to be hot again.
          ClearMethodCounter(info->GetMethod(), /*was_warm=*/ true);
        }
      }
    } else if (kIsDebugBuild) {
      // Sanity check that the profiling infos do not have a dangling entry point.
      for (ProfilingInfo* info : profiling_infos_) {
        DCHECK(!Runtime::Current()->IsZygote());
        const void* entry_point = info->GetSavedEntryPoint();
        DCHECK(entry_point == nullptr || IsInZygoteExecSpace(entry_point));
      }
    }

    // Mark compiled code that are entrypoints of ArtMethods. Compiled code that is not
    // an entry point is either:
    // - an osr compiled code, that will be removed if not in a thread call stack.
    // - discarded compiled code, that will be removed if not in a thread call stack.
    for (const auto& entry : jni_stubs_map_) {
      const JniStubData& data = entry.second;
      const void* code_ptr = data.GetCode();
      if (IsInZygoteExecSpace(code_ptr)) {
        continue;
      }
      const OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      for (ArtMethod* method : data.GetMethods()) {
        if (method_header->GetEntryPoint() == method->GetEntryPointFromQuickCompiledCode()) {
          GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(code_ptr));
          break;
        }
      }
    }
    for (const auto& it : method_code_map_) {
      ArtMethod* method = it.second;
      const void* code_ptr = it.first;
      if (IsInZygoteExecSpace(code_ptr)) {
        continue;
      }
      const OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      if (method_header->GetEntryPoint() == method->GetEntryPointFromQuickCompiledCode()) {
        GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(code_ptr));
      }
    }

    // Empty osr method map, as osr compiled code will be deleted (except the ones
    // on thread stacks).
    osr_code_map_.clear();
  }

  // Run a checkpoint on all threads to mark the JIT compiled code they are running.
  MarkCompiledCodeOnThreadStacks(self);

  // At this point, mutator threads are still running, and entrypoints of methods can
  // change. We do know they cannot change to a code cache entry that is not marked,
  // therefore we can safely remove those entries.
  RemoveUnmarkedCode(self);

  if (collect_profiling_info) {
    MutexLock mu(self, lock_);
    // Free all profiling infos of methods not compiled nor being compiled.
    auto profiling_kept_end = std::remove_if(profiling_infos_.begin(), profiling_infos_.end(),
      [this] (ProfilingInfo* info) NO_THREAD_SAFETY_ANALYSIS {
        const void* ptr = info->GetMethod()->GetEntryPointFromQuickCompiledCode();
        // We have previously cleared the ProfilingInfo pointer in the ArtMethod in the hope
        // that the compiled code would not get revived. As mutator threads run concurrently,
        // they may have revived the compiled code, and now we are in the situation where
        // a method has compiled code but no ProfilingInfo.
        // We make sure compiled methods have a ProfilingInfo object. It is needed for
        // code cache collection.
        if (ContainsPc(ptr) &&
            info->GetMethod()->GetProfilingInfo(kRuntimePointerSize) == nullptr) {
          info->GetMethod()->SetProfilingInfo(info);
        } else if (info->GetMethod()->GetProfilingInfo(kRuntimePointerSize) != info) {
          // No need for this ProfilingInfo object anymore.
          FreeData(reinterpret_cast<uint8_t*>(info));
          return true;
        }
        return false;
      });
    profiling_infos_.erase(profiling_kept_end, profiling_infos_.end());
    DCHECK(CheckLiveCompiledCodeHasProfilingInfo());
  }
}

bool JitCodeCache::CheckLiveCompiledCodeHasProfilingInfo() {
  ScopedTrace trace(__FUNCTION__);
  // Check that methods we have compiled do have a ProfilingInfo object. We would
  // have memory leaks of compiled code otherwise.
  for (const auto& it : method_code_map_) {
    ArtMethod* method = it.second;
    if (method->GetProfilingInfo(kRuntimePointerSize) == nullptr) {
      const void* code_ptr = it.first;
      const OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
      if (method_header->GetEntryPoint() == method->GetEntryPointFromQuickCompiledCode()) {
        // If the code is not dead, then we have a problem. Note that this can even
        // happen just after a collection, as mutator threads are running in parallel
        // and could deoptimize an existing compiled code.
        return false;
      }
    }
  }
  return true;
}

OatQuickMethodHeader* JitCodeCache::LookupMethodHeader(uintptr_t pc, ArtMethod* method) {
  static_assert(kRuntimeISA != InstructionSet::kThumb2, "kThumb2 cannot be a runtime ISA");
  if (kRuntimeISA == InstructionSet::kArm) {
    // On Thumb-2, the pc is offset by one.
    --pc;
  }
  if (!ContainsPc(reinterpret_cast<const void*>(pc))) {
    return nullptr;
  }

  if (!kIsDebugBuild) {
    // Called with null `method` only from MarkCodeClosure::Run() in debug build.
    CHECK(method != nullptr);
  }

  MutexLock mu(Thread::Current(), lock_);
  OatQuickMethodHeader* method_header = nullptr;
  ArtMethod* found_method = nullptr;  // Only for DCHECK(), not for JNI stubs.
  if (method != nullptr && UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    if (it == jni_stubs_map_.end() || !ContainsElement(it->second.GetMethods(), method)) {
      return nullptr;
    }
    const void* code_ptr = it->second.GetCode();
    method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
    if (!method_header->Contains(pc)) {
      return nullptr;
    }
  } else {
    auto it = method_code_map_.lower_bound(reinterpret_cast<const void*>(pc));
    if (it != method_code_map_.begin()) {
      --it;
      const void* code_ptr = it->first;
      if (OatQuickMethodHeader::FromCodePointer(code_ptr)->Contains(pc)) {
        method_header = OatQuickMethodHeader::FromCodePointer(code_ptr);
        found_method = it->second;
      }
    }
    if (method_header == nullptr && method == nullptr) {
      // Scan all compiled JNI stubs as well. This slow search is used only
      // for checks in debug build, for release builds the `method` is not null.
      for (auto&& entry : jni_stubs_map_) {
        const JniStubData& data = entry.second;
        if (data.IsCompiled() &&
            OatQuickMethodHeader::FromCodePointer(data.GetCode())->Contains(pc)) {
          method_header = OatQuickMethodHeader::FromCodePointer(data.GetCode());
        }
      }
    }
    if (method_header == nullptr) {
      return nullptr;
    }
  }

  if (kIsDebugBuild && method != nullptr && !method->IsNative()) {
    // When we are walking the stack to redefine classes and creating obsolete methods it is
    // possible that we might have updated the method_code_map by making this method obsolete in a
    // previous frame. Therefore we should just check that the non-obsolete version of this method
    // is the one we expect. We change to the non-obsolete versions in the error message since the
    // obsolete version of the method might not be fully initialized yet. This situation can only
    // occur when we are in the process of allocating and setting up obsolete methods. Otherwise
    // method and it->second should be identical. (See openjdkjvmti/ti_redefine.cc for more
    // information.)
    DCHECK_EQ(found_method->GetNonObsoleteMethod(), method->GetNonObsoleteMethod())
        << ArtMethod::PrettyMethod(method->GetNonObsoleteMethod()) << " "
        << ArtMethod::PrettyMethod(found_method->GetNonObsoleteMethod()) << " "
        << std::hex << pc;
  }
  return method_header;
}

OatQuickMethodHeader* JitCodeCache::LookupOsrMethodHeader(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  auto it = osr_code_map_.find(method);
  if (it == osr_code_map_.end()) {
    return nullptr;
  }
  return OatQuickMethodHeader::FromCodePointer(it->second);
}

ProfilingInfo* JitCodeCache::AddProfilingInfo(Thread* self,
                                              ArtMethod* method,
                                              const std::vector<uint32_t>& entries,
                                              bool retry_allocation)
    // No thread safety analysis as we are using TryLock/Unlock explicitly.
    NO_THREAD_SAFETY_ANALYSIS {
  ProfilingInfo* info = nullptr;
  if (!retry_allocation) {
    // If we are allocating for the interpreter, just try to lock, to avoid
    // lock contention with the JIT.
    if (lock_.ExclusiveTryLock(self)) {
      info = AddProfilingInfoInternal(self, method, entries);
      lock_.ExclusiveUnlock(self);
    }
  } else {
    {
      MutexLock mu(self, lock_);
      info = AddProfilingInfoInternal(self, method, entries);
    }

    if (info == nullptr) {
      GarbageCollectCache(self);
      MutexLock mu(self, lock_);
      info = AddProfilingInfoInternal(self, method, entries);
    }
  }
  return info;
}

ProfilingInfo* JitCodeCache::AddProfilingInfoInternal(Thread* self ATTRIBUTE_UNUSED,
                                                      ArtMethod* method,
                                                      const std::vector<uint32_t>& entries) {
  size_t profile_info_size = RoundUp(
      sizeof(ProfilingInfo) + sizeof(InlineCache) * entries.size(),
      sizeof(void*));

  // Check whether some other thread has concurrently created it.
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  if (info != nullptr) {
    return info;
  }

  uint8_t* data = AllocateData(profile_info_size);
  if (data == nullptr) {
    return nullptr;
  }
  info = new (data) ProfilingInfo(method, entries);

  // Make sure other threads see the data in the profiling info object before the
  // store in the ArtMethod's ProfilingInfo pointer.
  std::atomic_thread_fence(std::memory_order_release);

  method->SetProfilingInfo(info);
  profiling_infos_.push_back(info);
  histogram_profiling_info_memory_use_.AddValue(profile_info_size);
  return info;
}

// NO_THREAD_SAFETY_ANALYSIS as this is called from mspace code, at which point the lock
// is already held.
void* JitCodeCache::MoreCore(const void* mspace, intptr_t increment) NO_THREAD_SAFETY_ANALYSIS {
  if (mspace == exec_mspace_) {
    DCHECK(exec_mspace_ != nullptr);
    const MemMap* const code_pages = GetUpdatableCodeMapping();
    void* result = code_pages->Begin() + exec_end_;
    exec_end_ += increment;
    return result;
  } else {
    DCHECK_EQ(data_mspace_, mspace);
    void* result = data_pages_.Begin() + data_end_;
    data_end_ += increment;
    return result;
  }
}

void JitCodeCache::GetProfiledMethods(const std::set<std::string>& dex_base_locations,
                                      std::vector<ProfileMethodInfo>& methods) {
  Thread* self = Thread::Current();
  WaitUntilInlineCacheAccessible(self);
  MutexLock mu(self, lock_);
  ScopedTrace trace(__FUNCTION__);
  uint16_t jit_compile_threshold = Runtime::Current()->GetJITOptions()->GetCompileThreshold();
  for (const ProfilingInfo* info : profiling_infos_) {
    ArtMethod* method = info->GetMethod();
    const DexFile* dex_file = method->GetDexFile();
    const std::string base_location = DexFileLoader::GetBaseLocation(dex_file->GetLocation());
    if (!ContainsElement(dex_base_locations, base_location)) {
      // Skip dex files which are not profiled.
      continue;
    }
    std::vector<ProfileMethodInfo::ProfileInlineCache> inline_caches;

    // If the method didn't reach the compilation threshold don't save the inline caches.
    // They might be incomplete and cause unnecessary deoptimizations.
    // If the inline cache is empty the compiler will generate a regular invoke virtual/interface.
    if (method->GetCounter() < jit_compile_threshold) {
      methods.emplace_back(/*ProfileMethodInfo*/
          MethodReference(dex_file, method->GetDexMethodIndex()), inline_caches);
      continue;
    }

    for (size_t i = 0; i < info->number_of_inline_caches_; ++i) {
      std::vector<TypeReference> profile_classes;
      const InlineCache& cache = info->cache_[i];
      ArtMethod* caller = info->GetMethod();
      bool is_missing_types = false;
      for (size_t k = 0; k < InlineCache::kIndividualCacheSize; k++) {
        mirror::Class* cls = cache.classes_[k].Read();
        if (cls == nullptr) {
          break;
        }

        // Check if the receiver is in the boot class path or if it's in the
        // same class loader as the caller. If not, skip it, as there is not
        // much we can do during AOT.
        if (!cls->IsBootStrapClassLoaded() &&
            caller->GetClassLoader() != cls->GetClassLoader()) {
          is_missing_types = true;
          continue;
        }

        const DexFile* class_dex_file = nullptr;
        dex::TypeIndex type_index;

        if (cls->GetDexCache() == nullptr) {
          DCHECK(cls->IsArrayClass()) << cls->PrettyClass();
          // Make a best effort to find the type index in the method's dex file.
          // We could search all open dex files but that might turn expensive
          // and probably not worth it.
          class_dex_file = dex_file;
          type_index = cls->FindTypeIndexInOtherDexFile(*dex_file);
        } else {
          class_dex_file = &(cls->GetDexFile());
          type_index = cls->GetDexTypeIndex();
        }
        if (!type_index.IsValid()) {
          // Could be a proxy class or an array for which we couldn't find the type index.
          is_missing_types = true;
          continue;
        }
        if (ContainsElement(dex_base_locations,
                            DexFileLoader::GetBaseLocation(class_dex_file->GetLocation()))) {
          // Only consider classes from the same apk (including multidex).
          profile_classes.emplace_back(/*ProfileMethodInfo::ProfileClassReference*/
              class_dex_file, type_index);
        } else {
          is_missing_types = true;
        }
      }
      if (!profile_classes.empty()) {
        inline_caches.emplace_back(/*ProfileMethodInfo::ProfileInlineCache*/
            cache.dex_pc_, is_missing_types, profile_classes);
      }
    }
    methods.emplace_back(/*ProfileMethodInfo*/
        MethodReference(dex_file, method->GetDexMethodIndex()), inline_caches);
  }
}

bool JitCodeCache::IsOsrCompiled(ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  return osr_code_map_.find(method) != osr_code_map_.end();
}

bool JitCodeCache::NotifyCompilationOf(ArtMethod* method, Thread* self, bool osr) {
  if (!osr && ContainsPc(method->GetEntryPointFromQuickCompiledCode())) {
    return false;
  }

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (class_linker->IsQuickResolutionStub(method->GetEntryPointFromQuickCompiledCode())) {
    if (!Runtime::Current()->IsUsingApexBootImageLocation() || !Runtime::Current()->IsZygote()) {
      // Unless we're running as zygote in the jitzygote experiment, we currently don't save
      // the JIT compiled code if we cannot update the entrypoint due to having the resolution stub.
      VLOG(jit) << "Not compiling "
                << method->PrettyMethod()
                << " because it has the resolution stub";
      // Give it a new chance to be hot.
      ClearMethodCounter(method, /*was_warm=*/ false);
      return false;
    }
  }

  MutexLock mu(self, lock_);
  if (osr && (osr_code_map_.find(method) != osr_code_map_.end())) {
    return false;
  }

  if (UNLIKELY(method->IsNative())) {
    JniStubKey key(method);
    auto it = jni_stubs_map_.find(key);
    bool new_compilation = false;
    if (it == jni_stubs_map_.end()) {
      // Create a new entry to mark the stub as being compiled.
      it = jni_stubs_map_.Put(key, JniStubData{});
      new_compilation = true;
    }
    JniStubData* data = &it->second;
    data->AddMethod(method);
    if (data->IsCompiled()) {
      OatQuickMethodHeader* method_header = OatQuickMethodHeader::FromCodePointer(data->GetCode());
      const void* entrypoint = method_header->GetEntryPoint();
      // Update also entrypoints of other methods held by the JniStubData.
      // We could simply update the entrypoint of `method` but if the last JIT GC has
      // changed these entrypoints to GenericJNI in preparation for a full GC, we may
      // as well change them back as this stub shall not be collected anyway and this
      // can avoid a few expensive GenericJNI calls.
      instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
      for (ArtMethod* m : data->GetMethods()) {
        // Call the dedicated method instead of the more generic UpdateMethodsCode, because
        // `m` might be in the process of being deleted.
        if (!class_linker->IsQuickResolutionStub(m->GetEntryPointFromQuickCompiledCode())) {
          instrumentation->UpdateNativeMethodsCodeToJitCode(m, entrypoint);
        }
      }
      if (collection_in_progress_) {
        if (!IsInZygoteExecSpace(data->GetCode())) {
          GetLiveBitmap()->AtomicTestAndSet(FromCodeToAllocation(data->GetCode()));
        }
      }
    }
    return new_compilation;
  } else {
    ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
    if (info == nullptr) {
      VLOG(jit) << method->PrettyMethod() << " needs a ProfilingInfo to be compiled";
      // Because the counter is not atomic, there are some rare cases where we may not hit the
      // threshold for creating the ProfilingInfo. Reset the counter now to "correct" this.
      ClearMethodCounter(method, /*was_warm=*/ false);
      return false;
    }

    if (info->IsMethodBeingCompiled(osr)) {
      return false;
    }

    info->SetIsMethodBeingCompiled(true, osr);
    return true;
  }
}

ProfilingInfo* JitCodeCache::NotifyCompilerUse(ArtMethod* method, Thread* self) {
  MutexLock mu(self, lock_);
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  if (info != nullptr) {
    if (!info->IncrementInlineUse()) {
      // Overflow of inlining uses, just bail.
      return nullptr;
    }
  }
  return info;
}

void JitCodeCache::DoneCompilerUse(ArtMethod* method, Thread* self) {
  MutexLock mu(self, lock_);
  ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
  DCHECK(info != nullptr);
  info->DecrementInlineUse();
}

void JitCodeCache::DoneCompiling(ArtMethod* method, Thread* self, bool osr) {
  DCHECK_EQ(Thread::Current(), self);
  MutexLock mu(self, lock_);
  if (UNLIKELY(method->IsNative())) {
    auto it = jni_stubs_map_.find(JniStubKey(method));
    DCHECK(it != jni_stubs_map_.end());
    JniStubData* data = &it->second;
    DCHECK(ContainsElement(data->GetMethods(), method));
    if (UNLIKELY(!data->IsCompiled())) {
      // Failed to compile; the JNI compiler never fails, but the cache may be full.
      jni_stubs_map_.erase(it);  // Remove the entry added in NotifyCompilationOf().
    }  // else CommitCodeInternal() updated entrypoints of all methods in the JniStubData.
  } else {
    ProfilingInfo* info = method->GetProfilingInfo(kRuntimePointerSize);
    DCHECK(info->IsMethodBeingCompiled(osr));
    info->SetIsMethodBeingCompiled(false, osr);
  }
}

void JitCodeCache::InvalidateCompiledCodeFor(ArtMethod* method,
                                             const OatQuickMethodHeader* header) {
  DCHECK(!method->IsNative());
  ProfilingInfo* profiling_info = method->GetProfilingInfo(kRuntimePointerSize);
  const void* method_entrypoint = method->GetEntryPointFromQuickCompiledCode();
  if ((profiling_info != nullptr) &&
      (profiling_info->GetSavedEntryPoint() == header->GetEntryPoint())) {
    // When instrumentation is set, the actual entrypoint is the one in the profiling info.
    method_entrypoint = profiling_info->GetSavedEntryPoint();
    // Prevent future uses of the compiled code.
    profiling_info->SetSavedEntryPoint(nullptr);
  }

  // Clear the method counter if we are running jitted code since we might want to jit this again in
  // the future.
  if (method_entrypoint == header->GetEntryPoint()) {
    // The entrypoint is the one to invalidate, so we just update it to the interpreter entry point
    // and clear the counter to get the method Jitted again.
    Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
        method, GetQuickToInterpreterBridge());
    ClearMethodCounter(method, /*was_warm=*/ profiling_info != nullptr);
  } else {
    MutexLock mu(Thread::Current(), lock_);
    auto it = osr_code_map_.find(method);
    if (it != osr_code_map_.end() && OatQuickMethodHeader::FromCodePointer(it->second) == header) {
      // Remove the OSR method, to avoid using it again.
      osr_code_map_.erase(it);
    }
  }
}

uint8_t* JitCodeCache::AllocateCode(size_t allocation_size) {
  // Each allocation should be on its own set of cache lines. The allocation must be large enough
  // for header, code, and any padding.
  size_t alignment = GetJitCodeAlignment();
  uint8_t* result = reinterpret_cast<uint8_t*>(
      mspace_memalign(exec_mspace_, alignment, allocation_size));
  size_t header_size = RoundUp(sizeof(OatQuickMethodHeader), alignment);
  // Ensure the header ends up at expected instruction alignment.
  DCHECK_ALIGNED_PARAM(reinterpret_cast<uintptr_t>(result + header_size), alignment);
  used_memory_for_code_ += mspace_usable_size(result);
  return result;
}

void JitCodeCache::FreeCode(uint8_t* code) {
  if (IsInZygoteExecSpace(code)) {
    // No need to free, this is shared memory.
    return;
  }
  used_memory_for_code_ -= mspace_usable_size(code);
  mspace_free(exec_mspace_, code);
}

uint8_t* JitCodeCache::AllocateData(size_t data_size) {
  void* result = mspace_malloc(data_mspace_, data_size);
  used_memory_for_data_ += mspace_usable_size(result);
  return reinterpret_cast<uint8_t*>(result);
}

void JitCodeCache::FreeData(uint8_t* data) {
  if (IsInZygoteDataSpace(data)) {
    // No need to free, this is shared memory.
    return;
  }
  used_memory_for_data_ -= mspace_usable_size(data);
  mspace_free(data_mspace_, data);
}

void JitCodeCache::Dump(std::ostream& os) {
  MutexLock mu(Thread::Current(), lock_);
  os << "Current JIT code cache size: " << PrettySize(used_memory_for_code_) << "\n"
     << "Current JIT data cache size: " << PrettySize(used_memory_for_data_) << "\n"
     << "Current JIT mini-debug-info size: " << PrettySize(GetJitMiniDebugInfoMemUsage()) << "\n"
     << "Current JIT capacity: " << PrettySize(current_capacity_) << "\n"
     << "Current number of JIT JNI stub entries: " << jni_stubs_map_.size() << "\n"
     << "Current number of JIT code cache entries: " << method_code_map_.size() << "\n"
     << "Total number of JIT compilations: " << number_of_compilations_ << "\n"
     << "Total number of JIT compilations for on stack replacement: "
        << number_of_osr_compilations_ << "\n"
     << "Total number of JIT code cache collections: " << number_of_collections_ << std::endl;
  histogram_stack_map_memory_use_.PrintMemoryUse(os);
  histogram_code_memory_use_.PrintMemoryUse(os);
  histogram_profiling_info_memory_use_.PrintMemoryUse(os);
}

void JitCodeCache::PostForkChildAction(bool is_system_server, bool is_zygote) {
  if (is_zygote) {
    // Don't transition if this is for a child zygote.
    return;
  }
  MutexLock mu(Thread::Current(), lock_);

  zygote_data_pages_ = std::move(data_pages_);
  zygote_exec_pages_ = std::move(exec_pages_);
  zygote_data_mspace_ = data_mspace_;
  zygote_exec_mspace_ = exec_mspace_;

  size_t initial_capacity = Runtime::Current()->GetJITOptions()->GetCodeCacheInitialCapacity();
  size_t max_capacity = Runtime::Current()->GetJITOptions()->GetCodeCacheMaxCapacity();

  InitializeState(initial_capacity, max_capacity);

  std::string error_msg;
  if (!InitializeMappings(/* rwx_memory_allowed= */ !is_system_server, is_zygote, &error_msg)) {
    LOG(WARNING) << "Could not reset JIT state after zygote fork: " << error_msg;
    return;
  }

  InitializeSpaces();
}

}  // namespace jit
}  // namespace art
