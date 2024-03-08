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

#include "jit.h"

#include <dlfcn.h>

#include "art_method-inl.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/logging.h"  // For VLOG.
#include "base/memory_tool.h"
#include "base/runtime_debug.h"
#include "base/scoped_flock.h"
#include "base/utils.h"
#include "class_root.h"
#include "debugger.h"
#include "dex/type_lookup_table.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "interpreter/interpreter.h"
#include "jit-inl.h"
#include "jit_code_cache.h"
#include "jni/java_vm_ext.h"
#include "mirror/method_handle_impl.h"
#include "mirror/var_handle.h"
#include "oat_file.h"
#include "oat_file_manager.h"
#include "oat_quick_method_header.h"
#include "profile/profile_compilation_info.h"
#include "profile_saver.h"
#include "runtime.h"
#include "runtime_options.h"
#include "stack.h"
#include "stack_map.h"
#include "thread-inl.h"
#include "thread_list.h"

namespace art {
namespace jit {

static constexpr bool kEnableOnStackReplacement = true;

// Different compilation threshold constants. These can be overridden on the command line.
static constexpr size_t kJitDefaultCompileThreshold           = 10000;  // Non-debug default.
static constexpr size_t kJitStressDefaultCompileThreshold     = 100;    // Fast-debug build.
static constexpr size_t kJitSlowStressDefaultCompileThreshold = 2;      // Slow-debug build.

// JIT compiler
void* Jit::jit_library_handle_ = nullptr;
void* Jit::jit_compiler_handle_ = nullptr;
void* (*Jit::jit_load_)(void) = nullptr;
void (*Jit::jit_unload_)(void*) = nullptr;
bool (*Jit::jit_compile_method_)(void*, ArtMethod*, Thread*, bool, bool) = nullptr;
void (*Jit::jit_types_loaded_)(void*, mirror::Class**, size_t count) = nullptr;
bool (*Jit::jit_generate_debug_info_)(void*) = nullptr;
void (*Jit::jit_update_options_)(void*) = nullptr;

struct StressModeHelper {
  DECLARE_RUNTIME_DEBUG_FLAG(kSlowMode);
};
DEFINE_RUNTIME_DEBUG_FLAG(StressModeHelper, kSlowMode);

uint32_t JitOptions::RoundUpThreshold(uint32_t threshold) {
  if (threshold > kJitSamplesBatchSize) {
    threshold = RoundUp(threshold, kJitSamplesBatchSize);
  }
  CHECK_LE(threshold, std::numeric_limits<uint16_t>::max());
  return threshold;
}

JitOptions* JitOptions::CreateFromRuntimeArguments(const RuntimeArgumentMap& options) {
  auto* jit_options = new JitOptions;
  jit_options->use_jit_compilation_ = options.GetOrDefault(RuntimeArgumentMap::UseJitCompilation);

  jit_options->code_cache_initial_capacity_ =
      options.GetOrDefault(RuntimeArgumentMap::JITCodeCacheInitialCapacity);
  jit_options->code_cache_max_capacity_ =
      options.GetOrDefault(RuntimeArgumentMap::JITCodeCacheMaxCapacity);
  jit_options->dump_info_on_shutdown_ =
      options.Exists(RuntimeArgumentMap::DumpJITInfoOnShutdown);
  jit_options->profile_saver_options_ =
      options.GetOrDefault(RuntimeArgumentMap::ProfileSaverOpts);
  jit_options->thread_pool_pthread_priority_ =
      options.GetOrDefault(RuntimeArgumentMap::JITPoolThreadPthreadPriority);

  if (options.Exists(RuntimeArgumentMap::JITCompileThreshold)) {
    jit_options->compile_threshold_ = *options.Get(RuntimeArgumentMap::JITCompileThreshold);
  } else {
    jit_options->compile_threshold_ =
        kIsDebugBuild
            ? (StressModeHelper::kSlowMode
                   ? kJitSlowStressDefaultCompileThreshold
                   : kJitStressDefaultCompileThreshold)
            : kJitDefaultCompileThreshold;
  }
  jit_options->compile_threshold_ = RoundUpThreshold(jit_options->compile_threshold_);

  if (options.Exists(RuntimeArgumentMap::JITWarmupThreshold)) {
    jit_options->warmup_threshold_ = *options.Get(RuntimeArgumentMap::JITWarmupThreshold);
  } else {
    jit_options->warmup_threshold_ = jit_options->compile_threshold_ / 2;
  }
  jit_options->warmup_threshold_ = RoundUpThreshold(jit_options->warmup_threshold_);

  if (options.Exists(RuntimeArgumentMap::JITOsrThreshold)) {
    jit_options->osr_threshold_ = *options.Get(RuntimeArgumentMap::JITOsrThreshold);
  } else {
    jit_options->osr_threshold_ = jit_options->compile_threshold_ * 2;
    if (jit_options->osr_threshold_ > std::numeric_limits<uint16_t>::max()) {
      jit_options->osr_threshold_ =
          RoundDown(std::numeric_limits<uint16_t>::max(), kJitSamplesBatchSize);
    }
  }
  jit_options->osr_threshold_ = RoundUpThreshold(jit_options->osr_threshold_);

  if (options.Exists(RuntimeArgumentMap::JITPriorityThreadWeight)) {
    jit_options->priority_thread_weight_ =
        *options.Get(RuntimeArgumentMap::JITPriorityThreadWeight);
    if (jit_options->priority_thread_weight_ > jit_options->warmup_threshold_) {
      LOG(FATAL) << "Priority thread weight is above the warmup threshold.";
    } else if (jit_options->priority_thread_weight_ == 0) {
      LOG(FATAL) << "Priority thread weight cannot be 0.";
    }
  } else {
    jit_options->priority_thread_weight_ = std::max(
        jit_options->warmup_threshold_ / Jit::kDefaultPriorityThreadWeightRatio,
        static_cast<size_t>(1));
  }

  if (options.Exists(RuntimeArgumentMap::JITInvokeTransitionWeight)) {
    jit_options->invoke_transition_weight_ =
        *options.Get(RuntimeArgumentMap::JITInvokeTransitionWeight);
    if (jit_options->invoke_transition_weight_ > jit_options->warmup_threshold_) {
      LOG(FATAL) << "Invoke transition weight is above the warmup threshold.";
    } else if (jit_options->invoke_transition_weight_  == 0) {
      LOG(FATAL) << "Invoke transition weight cannot be 0.";
    }
  } else {
    jit_options->invoke_transition_weight_ = std::max(
        jit_options->warmup_threshold_ / Jit::kDefaultInvokeTransitionWeightRatio,
        static_cast<size_t>(1));
  }

  return jit_options;
}

void Jit::DumpInfo(std::ostream& os) {
  code_cache_->Dump(os);
  cumulative_timings_.Dump(os);
  MutexLock mu(Thread::Current(), lock_);
  memory_use_.PrintMemoryUse(os);
}

void Jit::DumpForSigQuit(std::ostream& os) {
  DumpInfo(os);
  ProfileSaver::DumpInstanceInfo(os);
}

void Jit::AddTimingLogger(const TimingLogger& logger) {
  cumulative_timings_.AddLogger(logger);
}

Jit::Jit(JitCodeCache* code_cache, JitOptions* options)
    : code_cache_(code_cache),
      options_(options),
      cumulative_timings_("JIT timings"),
      memory_use_("Memory used for compilation", 16),
      lock_("JIT memory use lock") {}

Jit* Jit::Create(JitCodeCache* code_cache, JitOptions* options) {
  if (jit_load_ == nullptr) {
    LOG(WARNING) << "Not creating JIT: library not loaded";
    return nullptr;
  }
  jit_compiler_handle_ = (jit_load_)();
  if (jit_compiler_handle_ == nullptr) {
    LOG(WARNING) << "Not creating JIT: failed to allocate a compiler";
    return nullptr;
  }
  std::unique_ptr<Jit> jit(new Jit(code_cache, options));

  // If the code collector is enabled, check if that still holds:
  // With 'perf', we want a 1-1 mapping between an address and a method.
  // We aren't able to keep method pointers live during the instrumentation method entry trampoline
  // so we will just disable jit-gc if we are doing that.
  if (code_cache->GetGarbageCollectCode()) {
    code_cache->SetGarbageCollectCode(!jit_generate_debug_info_(jit_compiler_handle_) &&
        !Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled());
  }

  VLOG(jit) << "JIT created with initial_capacity="
      << PrettySize(options->GetCodeCacheInitialCapacity())
      << ", max_capacity=" << PrettySize(options->GetCodeCacheMaxCapacity())
      << ", compile_threshold=" << options->GetCompileThreshold()
      << ", profile_saver_options=" << options->GetProfileSaverOptions();

  // Notify native debugger about the classes already loaded before the creation of the jit.
  jit->DumpTypeInfoForLoadedTypes(Runtime::Current()->GetClassLinker());
  return jit.release();
}

template <typename T>
bool Jit::LoadSymbol(T* address, const char* name, std::string* error_msg) {
  *address = reinterpret_cast<T>(dlsym(jit_library_handle_, name));
  if (*address == nullptr) {
    *error_msg = std::string("JIT couldn't find ") + name + std::string(" entry point");
    return false;
  }
  return true;
}

bool Jit::LoadCompilerLibrary(std::string* error_msg) {
  jit_library_handle_ = dlopen(
      kIsDebugBuild ? "libartd-compiler.so" : "libart-compiler.so", RTLD_NOW);
  if (jit_library_handle_ == nullptr) {
    std::ostringstream oss;
    oss << "JIT could not load libart-compiler.so: " << dlerror();
    *error_msg = oss.str();
    return false;
  }
  bool all_resolved = true;
  all_resolved = all_resolved && LoadSymbol(&jit_load_, "jit_load", error_msg);
  all_resolved = all_resolved && LoadSymbol(&jit_unload_, "jit_unload", error_msg);
  all_resolved = all_resolved && LoadSymbol(&jit_compile_method_, "jit_compile_method", error_msg);
  all_resolved = all_resolved && LoadSymbol(&jit_types_loaded_, "jit_types_loaded", error_msg);
  all_resolved = all_resolved && LoadSymbol(&jit_update_options_, "jit_update_options", error_msg);
  all_resolved = all_resolved &&
      LoadSymbol(&jit_generate_debug_info_, "jit_generate_debug_info", error_msg);
  if (!all_resolved) {
    dlclose(jit_library_handle_);
    return false;
  }
  return true;
}

bool Jit::CompileMethod(ArtMethod* method, Thread* self, bool baseline, bool osr) {
  DCHECK(Runtime::Current()->UseJitCompilation());
  DCHECK(!method->IsRuntimeMethod());

  RuntimeCallbacks* cb = Runtime::Current()->GetRuntimeCallbacks();
  // Don't compile the method if it has breakpoints.
  if (cb->IsMethodBeingInspected(method) && !cb->IsMethodSafeToJit(method)) {
    VLOG(jit) << "JIT not compiling " << method->PrettyMethod()
              << " due to not being safe to jit according to runtime-callbacks. For example, there"
              << " could be breakpoints in this method.";
    return false;
  }

  // Don't compile the method if we are supposed to be deoptimized.
  instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
  if (instrumentation->AreAllMethodsDeoptimized() || instrumentation->IsDeoptimized(method)) {
    VLOG(jit) << "JIT not compiling " << method->PrettyMethod() << " due to deoptimization";
    return false;
  }

  // If we get a request to compile a proxy method, we pass the actual Java method
  // of that proxy method, as the compiler does not expect a proxy method.
  ArtMethod* method_to_compile = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
  if (!code_cache_->NotifyCompilationOf(method_to_compile, self, osr)) {
    return false;
  }

  VLOG(jit) << "Compiling method "
            << ArtMethod::PrettyMethod(method_to_compile)
            << " osr=" << std::boolalpha << osr;
  bool success = jit_compile_method_(jit_compiler_handle_, method_to_compile, self, baseline, osr);
  code_cache_->DoneCompiling(method_to_compile, self, osr);
  if (!success) {
    VLOG(jit) << "Failed to compile method "
              << ArtMethod::PrettyMethod(method_to_compile)
              << " osr=" << std::boolalpha << osr;
  }
  if (kIsDebugBuild) {
    if (self->IsExceptionPending()) {
      mirror::Throwable* exception = self->GetException();
      LOG(FATAL) << "No pending exception expected after compiling "
                 << ArtMethod::PrettyMethod(method)
                 << ": "
                 << exception->Dump();
    }
  }
  return success;
}

void Jit::WaitForWorkersToBeCreated() {
  if (thread_pool_ != nullptr) {
    thread_pool_->WaitForWorkersToBeCreated();
  }
}

void Jit::DeleteThreadPool() {
  Thread* self = Thread::Current();
  DCHECK(Runtime::Current()->IsShuttingDown(self));
  if (thread_pool_ != nullptr) {
    std::unique_ptr<ThreadPool> pool;
    {
      ScopedSuspendAll ssa(__FUNCTION__);
      // Clear thread_pool_ field while the threads are suspended.
      // A mutator in the 'AddSamples' method will check against it.
      pool = std::move(thread_pool_);
    }

    // When running sanitized, let all tasks finish to not leak. Otherwise just clear the queue.
    if (!kRunningOnMemoryTool) {
      pool->StopWorkers(self);
      pool->RemoveAllTasks(self);
    }
    // We could just suspend all threads, but we know those threads
    // will finish in a short period, so it's not worth adding a suspend logic
    // here. Besides, this is only done for shutdown.
    pool->Wait(self, false, false);
  }
}

void Jit::StartProfileSaver(const std::string& filename,
                            const std::vector<std::string>& code_paths) {
  if (options_->GetSaveProfilingInfo()) {
    ProfileSaver::Start(options_->GetProfileSaverOptions(), filename, code_cache_, code_paths);
  }
}

void Jit::StopProfileSaver() {
  if (options_->GetSaveProfilingInfo() && ProfileSaver::IsStarted()) {
    ProfileSaver::Stop(options_->DumpJitInfoOnShutdown());
  }
}

bool Jit::JitAtFirstUse() {
  return HotMethodThreshold() == 0;
}

bool Jit::CanInvokeCompiledCode(ArtMethod* method) {
  return code_cache_->ContainsPc(method->GetEntryPointFromQuickCompiledCode());
}

Jit::~Jit() {
  DCHECK(!options_->GetSaveProfilingInfo() || !ProfileSaver::IsStarted());
  if (options_->DumpJitInfoOnShutdown()) {
    DumpInfo(LOG_STREAM(INFO));
    Runtime::Current()->DumpDeoptimizations(LOG_STREAM(INFO));
  }
  DeleteThreadPool();
  if (jit_compiler_handle_ != nullptr) {
    jit_unload_(jit_compiler_handle_);
    jit_compiler_handle_ = nullptr;
  }
  if (jit_library_handle_ != nullptr) {
    dlclose(jit_library_handle_);
    jit_library_handle_ = nullptr;
  }
}

void Jit::NewTypeLoadedIfUsingJit(mirror::Class* type) {
  if (!Runtime::Current()->UseJitCompilation()) {
    // No need to notify if we only use the JIT to save profiles.
    return;
  }
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit_generate_debug_info_(jit->jit_compiler_handle_)) {
    DCHECK(jit->jit_types_loaded_ != nullptr);
    jit->jit_types_loaded_(jit->jit_compiler_handle_, &type, 1);
  }
}

void Jit::DumpTypeInfoForLoadedTypes(ClassLinker* linker) {
  struct CollectClasses : public ClassVisitor {
    bool operator()(ObjPtr<mirror::Class> klass) override REQUIRES_SHARED(Locks::mutator_lock_) {
      classes_.push_back(klass.Ptr());
      return true;
    }
    std::vector<mirror::Class*> classes_;
  };

  if (jit_generate_debug_info_(jit_compiler_handle_)) {
    ScopedObjectAccess so(Thread::Current());

    CollectClasses visitor;
    linker->VisitClasses(&visitor);
    jit_types_loaded_(jit_compiler_handle_, visitor.classes_.data(), visitor.classes_.size());
  }
}

extern "C" void art_quick_osr_stub(void** stack,
                                   size_t stack_size_in_bytes,
                                   const uint8_t* native_pc,
                                   JValue* result,
                                   const char* shorty,
                                   Thread* self);

bool Jit::MaybeDoOnStackReplacement(Thread* thread,
                                    ArtMethod* method,
                                    uint32_t dex_pc,
                                    int32_t dex_pc_offset,
                                    JValue* result) {
  if (!kEnableOnStackReplacement) {
    return false;
  }

  Jit* jit = Runtime::Current()->GetJit();
  if (jit == nullptr) {
    return false;
  }

  if (UNLIKELY(__builtin_frame_address(0) < thread->GetStackEnd())) {
    // Don't attempt to do an OSR if we are close to the stack limit. Since
    // the interpreter frames are still on stack, OSR has the potential
    // to stack overflow even for a simple loop.
    // b/27094810.
    return false;
  }

  // Get the actual Java method if this method is from a proxy class. The compiler
  // and the JIT code cache do not expect methods from proxy classes.
  method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);

  // Cheap check if the method has been compiled already. That's an indicator that we should
  // osr into it.
  if (!jit->GetCodeCache()->ContainsPc(method->GetEntryPointFromQuickCompiledCode())) {
    return false;
  }

  // Fetch some data before looking up for an OSR method. We don't want thread
  // suspension once we hold an OSR method, as the JIT code cache could delete the OSR
  // method while we are being suspended.
  CodeItemDataAccessor accessor(method->DexInstructionData());
  const size_t number_of_vregs = accessor.RegistersSize();
  const char* shorty = method->GetShorty();
  std::string method_name(VLOG_IS_ON(jit) ? method->PrettyMethod() : "");
  void** memory = nullptr;
  size_t frame_size = 0;
  ShadowFrame* shadow_frame = nullptr;
  const uint8_t* native_pc = nullptr;

  {
    ScopedAssertNoThreadSuspension sts("Holding OSR method");
    const OatQuickMethodHeader* osr_method = jit->GetCodeCache()->LookupOsrMethodHeader(method);
    if (osr_method == nullptr) {
      // No osr method yet, just return to the interpreter.
      return false;
    }

    CodeInfo code_info(osr_method);

    // Find stack map starting at the target dex_pc.
    StackMap stack_map = code_info.GetOsrStackMapForDexPc(dex_pc + dex_pc_offset);
    if (!stack_map.IsValid()) {
      // There is no OSR stack map for this dex pc offset. Just return to the interpreter in the
      // hope that the next branch has one.
      return false;
    }

    // Before allowing the jump, make sure no code is actively inspecting the method to avoid
    // jumping from interpreter to OSR while e.g. single stepping. Note that we could selectively
    // disable OSR when single stepping, but that's currently hard to know at this point.
    if (Runtime::Current()->GetRuntimeCallbacks()->IsMethodBeingInspected(method)) {
      return false;
    }

    // We found a stack map, now fill the frame with dex register values from the interpreter's
    // shadow frame.
    DexRegisterMap vreg_map = code_info.GetDexRegisterMapOf(stack_map);

    frame_size = osr_method->GetFrameSizeInBytes();

    // Allocate memory to put shadow frame values. The osr stub will copy that memory to
    // stack.
    // Note that we could pass the shadow frame to the stub, and let it copy the values there,
    // but that is engineering complexity not worth the effort for something like OSR.
    memory = reinterpret_cast<void**>(malloc(frame_size));
    CHECK(memory != nullptr);
    memset(memory, 0, frame_size);

    // Art ABI: ArtMethod is at the bottom of the stack.
    memory[0] = method;

    shadow_frame = thread->PopShadowFrame();
    if (vreg_map.empty()) {
      // If we don't have a dex register map, then there are no live dex registers at
      // this dex pc.
    } else {
      DCHECK_EQ(vreg_map.size(), number_of_vregs);
      for (uint16_t vreg = 0; vreg < number_of_vregs; ++vreg) {
        DexRegisterLocation::Kind location = vreg_map[vreg].GetKind();
        if (location == DexRegisterLocation::Kind::kNone) {
          // Dex register is dead or uninitialized.
          continue;
        }

        if (location == DexRegisterLocation::Kind::kConstant) {
          // We skip constants because the compiled code knows how to handle them.
          continue;
        }

        DCHECK_EQ(location, DexRegisterLocation::Kind::kInStack);

        int32_t vreg_value = shadow_frame->GetVReg(vreg);
        int32_t slot_offset = vreg_map[vreg].GetStackOffsetInBytes();
        DCHECK_LT(slot_offset, static_cast<int32_t>(frame_size));
        DCHECK_GT(slot_offset, 0);
        (reinterpret_cast<int32_t*>(memory))[slot_offset / sizeof(int32_t)] = vreg_value;
      }
    }

    native_pc = stack_map.GetNativePcOffset(kRuntimeISA) +
        osr_method->GetEntryPoint();
    VLOG(jit) << "Jumping to "
              << method_name
              << "@"
              << std::hex << reinterpret_cast<uintptr_t>(native_pc);
  }

  {
    ManagedStack fragment;
    thread->PushManagedStackFragment(&fragment);
    (*art_quick_osr_stub)(memory,
                          frame_size,
                          native_pc,
                          result,
                          shorty,
                          thread);

    if (UNLIKELY(thread->GetException() == Thread::GetDeoptimizationException())) {
      thread->DeoptimizeWithDeoptimizationException(result);
    }
    thread->PopManagedStackFragment(fragment);
  }
  free(memory);
  thread->PushShadowFrame(shadow_frame);
  VLOG(jit) << "Done running OSR code for " << method_name;
  return true;
}

void Jit::AddMemoryUsage(ArtMethod* method, size_t bytes) {
  if (bytes > 4 * MB) {
    LOG(INFO) << "Compiler allocated "
              << PrettySize(bytes)
              << " to compile "
              << ArtMethod::PrettyMethod(method);
  }
  MutexLock mu(Thread::Current(), lock_);
  memory_use_.AddValue(bytes);
}

class JitCompileTask final : public Task {
 public:
  enum class TaskKind {
    kAllocateProfile,
    kCompile,
    kCompileBaseline,
    kCompileOsr,
  };

  JitCompileTask(ArtMethod* method, TaskKind kind) : method_(method), kind_(kind), klass_(nullptr) {
    ScopedObjectAccess soa(Thread::Current());
    // For a non-bootclasspath class, add a global ref to the class to prevent class unloading
    // until compilation is done.
    if (method->GetDeclaringClass()->GetClassLoader() != nullptr) {
      klass_ = soa.Vm()->AddGlobalRef(soa.Self(), method_->GetDeclaringClass());
      CHECK(klass_ != nullptr);
    }
  }

  ~JitCompileTask() {
    if (klass_ != nullptr) {
      ScopedObjectAccess soa(Thread::Current());
      soa.Vm()->DeleteGlobalRef(soa.Self(), klass_);
    }
  }

  void Run(Thread* self) override {
    ScopedObjectAccess soa(self);
    switch (kind_) {
      case TaskKind::kCompile:
      case TaskKind::kCompileBaseline:
      case TaskKind::kCompileOsr: {
        Runtime::Current()->GetJit()->CompileMethod(
            method_,
            self,
            /* baseline= */ (kind_ == TaskKind::kCompileBaseline),
            /* osr= */ (kind_ == TaskKind::kCompileOsr));
        break;
      }
      case TaskKind::kAllocateProfile: {
        if (ProfilingInfo::Create(self, method_, /* retry_allocation= */ true)) {
          VLOG(jit) << "Start profiling " << ArtMethod::PrettyMethod(method_);
        }
        break;
      }
    }
    ProfileSaver::NotifyJitActivity();
  }

  void Finalize() override {
    delete this;
  }

 private:
  ArtMethod* const method_;
  const TaskKind kind_;
  jobject klass_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JitCompileTask);
};

class ZygoteTask final : public Task {
 public:
  ZygoteTask() {}

  void Run(Thread* self) override {
    Runtime* runtime = Runtime::Current();
    std::string profile_file;
    for (const std::string& option : runtime->GetImageCompilerOptions()) {
      if (android::base::StartsWith(option, "--profile-file=")) {
        profile_file = option.substr(strlen("--profile-file="));
        break;
      }
    }

    const std::vector<const DexFile*>& boot_class_path =
        runtime->GetClassLinker()->GetBootClassPath();
    ScopedNullHandle<mirror::ClassLoader> null_handle;
    // We add to the queue for zygote so that we can fork processes in-between
    // compilations.
    runtime->GetJit()->CompileMethodsFromProfile(
        self, boot_class_path, profile_file, null_handle, /* add_to_queue= */ true);
  }

  void Finalize() override {
    delete this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ZygoteTask);
};

static std::string GetProfileFile(const std::string& dex_location) {
  // Hardcoded assumption where the profile file is.
  // TODO(ngeoffray): this is brittle and we would need to change change if we
  // wanted to do more eager JITting of methods in a profile. This is
  // currently only for system server.
  return dex_location + ".prof";
}

class JitProfileTask final : public Task {
 public:
  JitProfileTask(const std::vector<std::unique_ptr<const DexFile>>& dex_files,
                 ObjPtr<mirror::ClassLoader> class_loader) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    for (const auto& dex_file : dex_files) {
      dex_files_.push_back(dex_file.get());
      // Register the dex file so that we can guarantee it doesn't get deleted
      // while reading it during the task.
      class_linker->RegisterDexFile(*dex_file.get(), class_loader);
    }
    ScopedObjectAccess soa(Thread::Current());
    class_loader_ = soa.Vm()->AddGlobalRef(soa.Self(), class_loader.Ptr());
  }

  void Run(Thread* self) override {
    ScopedObjectAccess soa(self);
    StackHandleScope<1> hs(self);
    Handle<mirror::ClassLoader> loader = hs.NewHandle<mirror::ClassLoader>(
        soa.Decode<mirror::ClassLoader>(class_loader_));
    Runtime::Current()->GetJit()->CompileMethodsFromProfile(
        self,
        dex_files_,
        GetProfileFile(dex_files_[0]->GetLocation()),
        loader,
        /* add_to_queue= */ false);
  }

  void Finalize() override {
    delete this;
  }

 private:
  std::vector<const DexFile*> dex_files_;
  jobject class_loader_;

  DISALLOW_COPY_AND_ASSIGN(JitProfileTask);
};

void Jit::CreateThreadPool() {
  // There is a DCHECK in the 'AddSamples' method to ensure the tread pool
  // is not null when we instrument.

  // We need peers as we may report the JIT thread, e.g., in the debugger.
  constexpr bool kJitPoolNeedsPeers = true;
  thread_pool_.reset(new ThreadPool("Jit thread pool", 1, kJitPoolNeedsPeers));

  thread_pool_->SetPthreadPriority(options_->GetThreadPoolPthreadPriority());
  Start();

  // If we're not using the default boot image location, request a JIT task to
  // compile all methods in the boot image profile.
  Runtime* runtime = Runtime::Current();
  if (runtime->IsZygote() && runtime->IsUsingApexBootImageLocation() && UseJitCompilation()) {
    thread_pool_->AddTask(Thread::Current(), new ZygoteTask());
  }
}

void Jit::RegisterDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files,
                           ObjPtr<mirror::ClassLoader> class_loader) {
  if (dex_files.empty()) {
    return;
  }
  Runtime* runtime = Runtime::Current();
  if (runtime->IsSystemServer() && runtime->IsUsingApexBootImageLocation() && UseJitCompilation()) {
    thread_pool_->AddTask(Thread::Current(), new JitProfileTask(dex_files, class_loader));
  }
}

void Jit::CompileMethodsFromProfile(
    Thread* self,
    const std::vector<const DexFile*>& dex_files,
    const std::string& profile_file,
    Handle<mirror::ClassLoader> class_loader,
    bool add_to_queue) {

  if (profile_file.empty()) {
    LOG(WARNING) << "Expected a profile file in JIT zygote mode";
    return;
  }

  std::string error_msg;
  ScopedFlock profile = LockedFile::Open(
      profile_file.c_str(), O_RDONLY, /* block= */ false, &error_msg);

  // Return early if we're unable to obtain a lock on the profile.
  if (profile.get() == nullptr) {
    LOG(ERROR) << "Cannot lock profile: " << error_msg;
    return;
  }

  ProfileCompilationInfo profile_info;
  if (!profile_info.Load(profile->Fd())) {
    LOG(ERROR) << "Could not load profile file";
    return;
  }
  ScopedObjectAccess soa(self);
  StackHandleScope<1> hs(self);
  MutableHandle<mirror::DexCache> dex_cache = hs.NewHandle<mirror::DexCache>(nullptr);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  for (const DexFile* dex_file : dex_files) {
    if (LocationIsOnRuntimeModule(dex_file->GetLocation().c_str())) {
      // The runtime module jars are already preopted.
      continue;
    }
    // To speed up class lookups, generate a type lookup table for
    // the dex file.
    if (dex_file->GetOatDexFile() == nullptr) {
      TypeLookupTable type_lookup_table = TypeLookupTable::Create(*dex_file);
      type_lookup_tables_.push_back(
            std::make_unique<art::OatDexFile>(std::move(type_lookup_table)));
      dex_file->SetOatDexFile(type_lookup_tables_.back().get());
    }

    std::set<dex::TypeIndex> class_types;
    std::set<uint16_t> all_methods;
    if (!profile_info.GetClassesAndMethods(*dex_file,
                                           &class_types,
                                           &all_methods,
                                           &all_methods,
                                           &all_methods)) {
      // This means the profile file did not reference the dex file, which is the case
      // if there's no classes and methods of that dex file in the profile.
      continue;
    }
    dex_cache.Assign(class_linker->FindDexCache(self, *dex_file));
    CHECK(dex_cache != nullptr) << "Could not find dex cache for " << dex_file->GetLocation();

    for (uint16_t method_idx : all_methods) {
      ArtMethod* method = class_linker->ResolveMethodWithoutInvokeType(
          method_idx, dex_cache, class_loader);
      if (method == nullptr) {
        self->ClearException();
        continue;
      }
      if (!method->IsCompilable() || !method->IsInvokable()) {
        continue;
      }
      const void* entry_point = method->GetEntryPointFromQuickCompiledCode();
      if (class_linker->IsQuickToInterpreterBridge(entry_point) ||
          class_linker->IsQuickGenericJniStub(entry_point) ||
          class_linker->IsQuickResolutionStub(entry_point)) {
        if (!method->IsNative()) {
          // The compiler requires a ProfilingInfo object for non-native methods.
          ProfilingInfo::Create(self, method, /* retry_allocation= */ true);
        }
        // Special case ZygoteServer class so that it gets compiled before the
        // zygote enters it. This avoids needing to do OSR during app startup.
        // TODO: have a profile instead.
        if (!add_to_queue || method->GetDeclaringClass()->DescriptorEquals(
                "Lcom/android/internal/os/ZygoteServer;")) {
          CompileMethod(method, self, /* baseline= */ false, /* osr= */ false);
        } else {
          thread_pool_->AddTask(self,
              new JitCompileTask(method, JitCompileTask::TaskKind::kCompile));
        }
      }
    }
  }
}

static bool IgnoreSamplesForMethod(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (method->IsClassInitializer() || !method->IsCompilable()) {
    // We do not want to compile such methods.
    return true;
  }
  if (method->IsNative()) {
    ObjPtr<mirror::Class> klass = method->GetDeclaringClass();
    if (klass == GetClassRoot<mirror::MethodHandle>() ||
        klass == GetClassRoot<mirror::VarHandle>()) {
      // MethodHandle and VarHandle invocation methods are required to throw an
      // UnsupportedOperationException if invoked reflectively. We achieve this by having native
      // implementations that arise the exception. We need to disable JIT compilation of these JNI
      // methods as it can lead to transitioning between JIT compiled JNI stubs and generic JNI
      // stubs. Since these stubs have different stack representations we can then crash in stack
      // walking (b/78151261).
      return true;
    }
  }
  return false;
}

bool Jit::MaybeCompileMethod(Thread* self,
                             ArtMethod* method,
                             uint32_t old_count,
                             uint32_t new_count,
                             bool with_backedges) {
  if (thread_pool_ == nullptr) {
    // Should only see this when shutting down, starting up, or in safe mode.
    DCHECK(Runtime::Current()->IsShuttingDown(self) ||
           !Runtime::Current()->IsFinishedStarting() ||
           Runtime::Current()->IsSafeMode());
    return false;
  }
  if (IgnoreSamplesForMethod(method)) {
    return false;
  }
  if (HotMethodThreshold() == 0) {
    // Tests might request JIT on first use (compiled synchronously in the interpreter).
    return false;
  }
  DCHECK(thread_pool_ != nullptr);
  DCHECK_GT(WarmMethodThreshold(), 0);
  DCHECK_GT(HotMethodThreshold(), WarmMethodThreshold());
  DCHECK_GT(OSRMethodThreshold(), HotMethodThreshold());
  DCHECK_GE(PriorityThreadWeight(), 1);
  DCHECK_LE(PriorityThreadWeight(), HotMethodThreshold());

  if (old_count < WarmMethodThreshold() && new_count >= WarmMethodThreshold()) {
    // Note: Native method have no "warm" state or profiling info.
    if (!method->IsNative() && method->GetProfilingInfo(kRuntimePointerSize) == nullptr) {
      bool success = ProfilingInfo::Create(self, method, /* retry_allocation= */ false);
      if (success) {
        VLOG(jit) << "Start profiling " << method->PrettyMethod();
      }

      if (thread_pool_ == nullptr) {
        // Calling ProfilingInfo::Create might put us in a suspended state, which could
        // lead to the thread pool being deleted when we are shutting down.
        DCHECK(Runtime::Current()->IsShuttingDown(self));
        return false;
      }

      if (!success) {
        // We failed allocating. Instead of doing the collection on the Java thread, we push
        // an allocation to a compiler thread, that will do the collection.
        thread_pool_->AddTask(
            self, new JitCompileTask(method, JitCompileTask::TaskKind::kAllocateProfile));
      }
    }
  }
  if (UseJitCompilation()) {
    if (old_count == 0 &&
        method->IsNative() &&
        Runtime::Current()->IsUsingApexBootImageLocation()) {
      // jitzygote: Compile JNI stub on first use to avoid the expensive generic stub.
      CompileMethod(method, self, /* baseline= */ false, /* osr= */ false);
      return true;
    }
    if (old_count < HotMethodThreshold() && new_count >= HotMethodThreshold()) {
      if (!code_cache_->ContainsPc(method->GetEntryPointFromQuickCompiledCode())) {
        DCHECK(thread_pool_ != nullptr);
        thread_pool_->AddTask(self, new JitCompileTask(method, JitCompileTask::TaskKind::kCompile));
      }
    }
    if (old_count < OSRMethodThreshold() && new_count >= OSRMethodThreshold()) {
      if (!with_backedges) {
        return false;
      }
      DCHECK(!method->IsNative());  // No back edges reported for native methods.
      if (!code_cache_->IsOsrCompiled(method)) {
        DCHECK(thread_pool_ != nullptr);
        thread_pool_->AddTask(
            self, new JitCompileTask(method, JitCompileTask::TaskKind::kCompileOsr));
      }
    }
  }
  return true;
}

class ScopedSetRuntimeThread {
 public:
  explicit ScopedSetRuntimeThread(Thread* self)
      : self_(self), was_runtime_thread_(self_->IsRuntimeThread()) {
    self_->SetIsRuntimeThread(true);
  }

  ~ScopedSetRuntimeThread() {
    self_->SetIsRuntimeThread(was_runtime_thread_);
  }

 private:
  Thread* self_;
  bool was_runtime_thread_;
};

void Jit::MethodEntered(Thread* thread, ArtMethod* method) {
  Runtime* runtime = Runtime::Current();
  if (UNLIKELY(runtime->UseJitCompilation() && runtime->GetJit()->JitAtFirstUse())) {
    ArtMethod* np_method = method->GetInterfaceMethodIfProxy(kRuntimePointerSize);
    if (np_method->IsCompilable()) {
      if (!np_method->IsNative()) {
        // The compiler requires a ProfilingInfo object for non-native methods.
        ProfilingInfo::Create(thread, np_method, /* retry_allocation= */ true);
      }
      JitCompileTask compile_task(method, JitCompileTask::TaskKind::kCompile);
      // Fake being in a runtime thread so that class-load behavior will be the same as normal jit.
      ScopedSetRuntimeThread ssrt(thread);
      compile_task.Run(thread);
    }
    return;
  }

  ProfilingInfo* profiling_info = method->GetProfilingInfo(kRuntimePointerSize);
  // Update the entrypoint if the ProfilingInfo has one. The interpreter will call it
  // instead of interpreting the method. We don't update it for instrumentation as the entrypoint
  // must remain the instrumentation entrypoint.
  if ((profiling_info != nullptr) &&
      (profiling_info->GetSavedEntryPoint() != nullptr) &&
      (method->GetEntryPointFromQuickCompiledCode() != GetQuickInstrumentationEntryPoint())) {
    Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
        method, profiling_info->GetSavedEntryPoint());
  } else {
    AddSamples(thread, method, 1, /* with_backedges= */false);
  }
}

void Jit::InvokeVirtualOrInterface(ObjPtr<mirror::Object> this_object,
                                   ArtMethod* caller,
                                   uint32_t dex_pc,
                                   ArtMethod* callee ATTRIBUTE_UNUSED) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  DCHECK(this_object != nullptr);
  ProfilingInfo* info = caller->GetProfilingInfo(kRuntimePointerSize);
  if (info != nullptr) {
    info->AddInvokeInfo(dex_pc, this_object->GetClass());
  }
}

void Jit::WaitForCompilationToFinish(Thread* self) {
  if (thread_pool_ != nullptr) {
    thread_pool_->Wait(self, false, false);
  }
}

void Jit::Stop() {
  Thread* self = Thread::Current();
  // TODO(ngeoffray): change API to not require calling WaitForCompilationToFinish twice.
  WaitForCompilationToFinish(self);
  GetThreadPool()->StopWorkers(self);
  WaitForCompilationToFinish(self);
}

void Jit::Start() {
  GetThreadPool()->StartWorkers(Thread::Current());
}

ScopedJitSuspend::ScopedJitSuspend() {
  jit::Jit* jit = Runtime::Current()->GetJit();
  was_on_ = (jit != nullptr) && (jit->GetThreadPool() != nullptr);
  if (was_on_) {
    jit->Stop();
  }
}

ScopedJitSuspend::~ScopedJitSuspend() {
  if (was_on_) {
    DCHECK(Runtime::Current()->GetJit() != nullptr);
    DCHECK(Runtime::Current()->GetJit()->GetThreadPool() != nullptr);
    Runtime::Current()->GetJit()->Start();
  }
}

void Jit::PostForkChildAction(bool is_system_server, bool is_zygote) {
  if (is_zygote) {
    // Remove potential tasks that have been inherited from the zygote. Child zygotes
    // currently don't need the whole boot image compiled (ie webview_zygote).
    thread_pool_->RemoveAllTasks(Thread::Current());
    // Don't transition if this is for a child zygote.
    return;
  }
  if (Runtime::Current()->IsSafeMode()) {
    // Delete the thread pool, we are not going to JIT.
    thread_pool_.reset(nullptr);
    return;
  }
  // At this point, the compiler options have been adjusted to the particular configuration
  // of the forked child. Parse them again.
  jit_update_options_(jit_compiler_handle_);

  // Adjust the status of code cache collection: the status from zygote was to not collect.
  code_cache_->SetGarbageCollectCode(!jit_generate_debug_info_(jit_compiler_handle_) &&
      !Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled());

  if (thread_pool_ != nullptr) {
    if (!is_system_server) {
      // Remove potential tasks that have been inherited from the zygote.
      // We keep the queue for system server, as not having those methods compiled
      // impacts app startup.
      thread_pool_->RemoveAllTasks(Thread::Current());
    } else if (Runtime::Current()->IsUsingApexBootImageLocation() && UseJitCompilation()) {
      // Disable garbage collection: we don't want it to delete methods we're compiling
      // through boot and system server profiles.
      // TODO(ngeoffray): Fix this so we still collect deoptimized and unused code.
      code_cache_->SetGarbageCollectCode(false);
    }

    // Resume JIT compilation.
    thread_pool_->CreateThreads();
  }
}

void Jit::PreZygoteFork() {
  if (thread_pool_ == nullptr) {
    return;
  }
  thread_pool_->DeleteThreads();
}

void Jit::PostZygoteFork() {
  if (thread_pool_ == nullptr) {
    return;
  }
  thread_pool_->CreateThreads();
}

}  // namespace jit
}  // namespace art
