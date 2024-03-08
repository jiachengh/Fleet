/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_CLASS_EXT_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_EXT_INL_H_

#include "class_ext.h"

#include "art_method-inl.h"
#include "object-inl.h"

namespace art {
namespace mirror {

inline ObjPtr<Object> ClassExt::GetVerifyError() {
  return GetFieldObject<ClassExt>(OFFSET_OF_OBJECT_MEMBER(ClassExt, verify_error_));
}

inline ObjPtr<ObjectArray<DexCache>> ClassExt::GetObsoleteDexCaches() {
  return GetFieldObject<ObjectArray<DexCache>>(
      OFFSET_OF_OBJECT_MEMBER(ClassExt, obsolete_dex_caches_));
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline ObjPtr<PointerArray> ClassExt::GetObsoleteMethods() {
  return GetFieldObject<PointerArray, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(ClassExt, obsolete_methods_));
}

inline ObjPtr<Object> ClassExt::GetOriginalDexFile() {
  return GetFieldObject<Object>(OFFSET_OF_OBJECT_MEMBER(ClassExt, original_dex_file_));
}

template<ReadBarrierOption kReadBarrierOption, class Visitor>
void ClassExt::VisitNativeRoots(Visitor& visitor, PointerSize pointer_size) {
  ObjPtr<PointerArray> arr(GetObsoleteMethods<kDefaultVerifyFlags, kReadBarrierOption>());
  if (arr.IsNull()) {
    return;
  }
  int32_t len = arr->GetLength();
  for (int32_t i = 0; i < len; i++) {
    ArtMethod* method = arr->GetElementPtrSize<ArtMethod*, kDefaultVerifyFlags>(i, pointer_size);
    if (method != nullptr) {
      method->VisitRoots<kReadBarrierOption>(visitor, pointer_size);
    }
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_EXT_INL_H_
