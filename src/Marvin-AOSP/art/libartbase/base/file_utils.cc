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

#include "file_utils.h"

#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <unistd.h>

// We need dladdr.
#if !defined(__APPLE__) && !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#define DEFINED_GNU_SOURCE
#endif
#include <dlfcn.h>
#include <libgen.h>
#ifdef DEFINED_GNU_SOURCE
#undef _GNU_SOURCE
#undef DEFINED_GNU_SOURCE
#endif
#endif


#include <memory>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/bit_utils.h"
#include "base/globals.h"
#include "base/os.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"

#if defined(__APPLE__)
#include <crt_externs.h>
#include <sys/syscall.h>
#include "AvailabilityMacros.h"  // For MAC_OS_X_VERSION_MAX_ALLOWED
#endif

#if defined(__linux__)
#include <linux/unistd.h>
#endif

namespace art {

using android::base::StringPrintf;

static constexpr const char* kClassesDex = "classes.dex";
static constexpr const char* kApexDefaultPath = "/apex/";
static constexpr const char* kAndroidRootEnvVar = "ANDROID_ROOT";
static constexpr const char* kAndroidRootDefaultPath = "/system";
static constexpr const char* kAndroidDataEnvVar = "ANDROID_DATA";
static constexpr const char* kAndroidDataDefaultPath = "/data";
static constexpr const char* kAndroidRuntimeRootEnvVar = "ANDROID_RUNTIME_ROOT";
static constexpr const char* kAndroidRuntimeApexDefaultPath = "/apex/com.android.runtime";
static constexpr const char* kAndroidConscryptRootEnvVar = "ANDROID_CONSCRYPT_ROOT";
static constexpr const char* kAndroidConscryptApexDefaultPath = "/apex/com.android.conscrypt";

bool ReadFileToString(const std::string& file_name, std::string* result) {
  File file(file_name, O_RDONLY, false);
  if (!file.IsOpened()) {
    return false;
  }

  std::vector<char> buf(8 * KB);
  while (true) {
    int64_t n = TEMP_FAILURE_RETRY(read(file.Fd(), &buf[0], buf.size()));
    if (n == -1) {
      return false;
    }
    if (n == 0) {
      return true;
    }
    result->append(&buf[0], n);
  }
}

std::string GetAndroidRootSafe(std::string* error_msg) {
#ifdef _WIN32
  UNUSED(kAndroidRootEnvVar, kAndroidRootDefaultPath);
  *error_msg = "GetAndroidRootSafe unsupported for Windows.";
  return "";
#else
  // Prefer ANDROID_ROOT if it's set.
  const char* android_root_from_env = getenv(kAndroidRootEnvVar);
  if (android_root_from_env != nullptr) {
    if (!OS::DirectoryExists(android_root_from_env)) {
      *error_msg = StringPrintf("Failed to find ANDROID_ROOT directory %s", android_root_from_env);
      return "";
    }
    return android_root_from_env;
  }

  // Check where libart is from, and derive from there. Only do this for non-Mac.
#ifndef __APPLE__
  {
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&GetAndroidRootSafe), /* out */ &info) != 0) {
      // Make a duplicate of the fname so dirname can modify it.
      UniqueCPtr<char> fname(strdup(info.dli_fname));

      char* dir1 = dirname(fname.get());  // This is the lib directory.
      char* dir2 = dirname(dir1);         // This is the "system" directory.
      if (OS::DirectoryExists(dir2)) {
        std::string tmp = dir2;  // Make a copy here so that fname can be released.
        return tmp;
      }
    }
  }
#endif

  // Try the default path.
  if (!OS::DirectoryExists(kAndroidRootDefaultPath)) {
    *error_msg = StringPrintf("Failed to find directory %s", kAndroidRootDefaultPath);
    return "";
  }
  return kAndroidRootDefaultPath;
#endif
}

std::string GetAndroidRoot() {
  std::string error_msg;
  std::string ret = GetAndroidRootSafe(&error_msg);
  if (ret.empty()) {
    LOG(FATAL) << error_msg;
    UNREACHABLE();
  }
  return ret;
}


static const char* GetAndroidDirSafe(const char* env_var,
                                     const char* default_dir,
                                     bool must_exist,
                                     std::string* error_msg) {
  const char* android_dir = getenv(env_var);
  if (android_dir == nullptr) {
    if (!must_exist || OS::DirectoryExists(default_dir)) {
      android_dir = default_dir;
    } else {
      *error_msg = StringPrintf("%s not set and %s does not exist", env_var, default_dir);
      return nullptr;
    }
  }
  if (must_exist && !OS::DirectoryExists(android_dir)) {
    *error_msg = StringPrintf("Failed to find %s directory %s", env_var, android_dir);
    return nullptr;
  }
  return android_dir;
}

static const char* GetAndroidDir(const char* env_var, const char* default_dir) {
  std::string error_msg;
  const char* dir = GetAndroidDirSafe(env_var, default_dir, /* must_exist= */ true, &error_msg);
  if (dir != nullptr) {
    return dir;
  } else {
    LOG(FATAL) << error_msg;
    UNREACHABLE();
  }
}

std::string GetAndroidRuntimeRootSafe(std::string* error_msg) {
  const char* android_dir = GetAndroidDirSafe(kAndroidRuntimeRootEnvVar,
                                              kAndroidRuntimeApexDefaultPath,
                                              /* must_exist= */ true,
                                              error_msg);
  return (android_dir != nullptr) ? android_dir : "";
}

std::string GetAndroidRuntimeRoot() {
  return GetAndroidDir(kAndroidRuntimeRootEnvVar, kAndroidRuntimeApexDefaultPath);
}

std::string GetAndroidDataSafe(std::string* error_msg) {
  const char* android_dir = GetAndroidDirSafe(kAndroidDataEnvVar,
                                              kAndroidDataDefaultPath,
                                              /* must_exist= */ true,
                                              error_msg);
  return (android_dir != nullptr) ? android_dir : "";
}

std::string GetAndroidData() {
  return GetAndroidDir(kAndroidDataEnvVar, kAndroidDataDefaultPath);
}

std::string GetDefaultBootImageLocation(const std::string& android_root) {
  return StringPrintf("%s/framework/boot.art", android_root.c_str());
}

std::string GetDefaultBootImageLocation(std::string* error_msg) {
  std::string android_root = GetAndroidRootSafe(error_msg);
  if (android_root.empty()) {
    return "";
  }
  return GetDefaultBootImageLocation(android_root);
}

void GetDalvikCache(const char* subdir, const bool create_if_absent, std::string* dalvik_cache,
                    bool* have_android_data, bool* dalvik_cache_exists, bool* is_global_cache) {
#ifdef _WIN32
  UNUSED(subdir);
  UNUSED(create_if_absent);
  UNUSED(dalvik_cache);
  UNUSED(have_android_data);
  UNUSED(dalvik_cache_exists);
  UNUSED(is_global_cache);
  LOG(FATAL) << "GetDalvikCache unsupported on Windows.";
#else
  CHECK(subdir != nullptr);
  std::string unused_error_msg;
  std::string android_data = GetAndroidDataSafe(&unused_error_msg);
  if (android_data.empty()) {
    *have_android_data = false;
    *dalvik_cache_exists = false;
    *is_global_cache = false;
    return;
  } else {
    *have_android_data = true;
  }
  const std::string dalvik_cache_root = android_data + "/dalvik-cache";
  *dalvik_cache = dalvik_cache_root + '/' + subdir;
  *dalvik_cache_exists = OS::DirectoryExists(dalvik_cache->c_str());
  *is_global_cache = (android_data == kAndroidDataDefaultPath);
  if (create_if_absent && !*dalvik_cache_exists && !*is_global_cache) {
    // Don't create the system's /data/dalvik-cache/... because it needs special permissions.
    *dalvik_cache_exists = ((mkdir(dalvik_cache_root.c_str(), 0700) == 0 || errno == EEXIST) &&
                            (mkdir(dalvik_cache->c_str(), 0700) == 0 || errno == EEXIST));
  }
#endif
}

std::string GetDalvikCache(const char* subdir) {
  CHECK(subdir != nullptr);
  std::string android_data = GetAndroidData();
  const std::string dalvik_cache_root = android_data + "/dalvik-cache";
  const std::string dalvik_cache = dalvik_cache_root + '/' + subdir;
  if (!OS::DirectoryExists(dalvik_cache.c_str())) {
    // TODO: Check callers. Traditional behavior is to not abort.
    return "";
  }
  return dalvik_cache;
}

bool GetDalvikCacheFilename(const char* location, const char* cache_location,
                            std::string* filename, std::string* error_msg) {
  if (location[0] != '/') {
    *error_msg = StringPrintf("Expected path in location to be absolute: %s", location);
    return false;
  }
  std::string cache_file(&location[1]);  // skip leading slash
  if (!android::base::EndsWith(location, ".dex") &&
      !android::base::EndsWith(location, ".art") &&
      !android::base::EndsWith(location, ".oat")) {
    cache_file += "/";
    cache_file += kClassesDex;
  }
  std::replace(cache_file.begin(), cache_file.end(), '/', '@');
  *filename = StringPrintf("%s/%s", cache_location, cache_file.c_str());
  return true;
}

std::string GetVdexFilename(const std::string& oat_location) {
  return ReplaceFileExtension(oat_location, "vdex");
}

static void InsertIsaDirectory(const InstructionSet isa, std::string* filename) {
  // in = /foo/bar/baz
  // out = /foo/bar/<isa>/baz
  size_t pos = filename->rfind('/');
  CHECK_NE(pos, std::string::npos) << *filename << " " << isa;
  filename->insert(pos, "/", 1);
  filename->insert(pos + 1, GetInstructionSetString(isa));
}

std::string GetSystemImageFilename(const char* location, const InstructionSet isa) {
  // location = /system/framework/boot.art
  // filename = /system/framework/<isa>/boot.art
  std::string filename(location);
  InsertIsaDirectory(isa, &filename);
  return filename;
}

std::string ReplaceFileExtension(const std::string& filename, const std::string& new_extension) {
  const size_t last_ext = filename.find_last_of("./");
  if (last_ext == std::string::npos || filename[last_ext] != '.') {
    return filename + "." + new_extension;
  } else {
    return filename.substr(0, last_ext + 1) + new_extension;
  }
}

static bool StartsWithSlash(const char* str) {
  DCHECK(str != nullptr);
  return str[0] == '/';
}

static bool EndsWithSlash(const char* str) {
  DCHECK(str != nullptr);
  size_t len = strlen(str);
  return len > 0 && str[len - 1] == '/';
}

// Returns true if `full_path` is located in folder either provided with `env_var`
// or in `default_path` otherwise. The caller may optionally provide a `subdir`
// which will be appended to the tested prefix.
// All of `default_path`, `subdir` and the value of environment variable `env_var`
// are expected to begin with a slash and not end with one. If this ever changes,
// the path-building logic should be updated.
static bool IsLocationOnModule(const char* full_path,
                               const char* env_var,
                               const char* default_path,
                               const char* subdir = nullptr) {
  std::string unused_error_msg;
  const char* module_path = GetAndroidDirSafe(env_var,
                                              default_path,
                                              /* must_exist= */ kIsTargetBuild,
                                              &unused_error_msg);
  if (module_path == nullptr) {
    return false;
  }

  // Build the path which we will check is a prefix of `full_path`. The prefix must
  // end with a slash, so that "/foo/bar" does not match "/foo/barz".
  DCHECK(StartsWithSlash(module_path)) << module_path;
  std::string path_prefix(module_path);
  if (!EndsWithSlash(path_prefix.c_str())) {
    path_prefix.append("/");
  }
  if (subdir != nullptr) {
    // If `subdir` is provided, we assume it is provided without a starting slash
    // but ending with one, e.g. "sub/dir/". `path_prefix` ends with a slash at
    // this point, so we simply append `subdir`.
    DCHECK(!StartsWithSlash(subdir) && EndsWithSlash(subdir)) << subdir;
    path_prefix.append(subdir);
  }

  return android::base::StartsWith(full_path, path_prefix);
}

bool LocationIsOnSystemFramework(const char* full_path) {
  return IsLocationOnModule(full_path,
                            kAndroidRootEnvVar,
                            kAndroidRootDefaultPath,
                            /* subdir= */ "framework/");
}

bool LocationIsOnRuntimeModule(const char* full_path) {
  return IsLocationOnModule(full_path, kAndroidRuntimeRootEnvVar, kAndroidRuntimeApexDefaultPath);
}

bool LocationIsOnConscryptModule(const char* full_path) {
  return IsLocationOnModule(
      full_path, kAndroidConscryptRootEnvVar, kAndroidConscryptApexDefaultPath);
}

bool LocationIsOnApex(const char* full_path) {
  return android::base::StartsWith(full_path, kApexDefaultPath);
}

bool LocationIsOnSystem(const char* path) {
#ifdef _WIN32
  UNUSED(path);
  LOG(FATAL) << "LocationIsOnSystem is unsupported on Windows.";
  return false;
#else
  UniqueCPtr<const char[]> full_path(realpath(path, nullptr));
  return full_path != nullptr &&
      android::base::StartsWith(full_path.get(), GetAndroidRoot().c_str());
#endif
}

bool RuntimeModuleRootDistinctFromAndroidRoot() {
  std::string error_msg;
  const char* android_root = GetAndroidDirSafe(kAndroidRootEnvVar,
                                               kAndroidRootDefaultPath,
                                               /* must_exist= */ kIsTargetBuild,
                                               &error_msg);
  const char* runtime_root = GetAndroidDirSafe(kAndroidRuntimeRootEnvVar,
                                               kAndroidRuntimeApexDefaultPath,
                                               /* must_exist= */ kIsTargetBuild,
                                               &error_msg);
  return (android_root != nullptr)
      && (runtime_root != nullptr)
      && (std::string_view(android_root) != std::string_view(runtime_root));
}

int DupCloexec(int fd) {
#if defined(__linux__)
  return fcntl(fd, F_DUPFD_CLOEXEC, 0);
#else
  return dup(fd);
#endif
}

}  // namespace art
