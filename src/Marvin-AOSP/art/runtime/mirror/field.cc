/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "field-inl.h"

#include "class-inl.h"
#include "dex_cache-inl.h"
#include "object-inl.h"
#include "object_array-inl.h"

namespace art {
namespace mirror {

ArtField* Field::GetArtField() {
  ObjPtr<mirror::Class> declaring_class = GetDeclaringClass();
  if (UNLIKELY(declaring_class->IsProxyClass())) {
    DCHECK(IsStatic());
    DCHECK_EQ(declaring_class->NumStaticFields(), 2U);
    // 0 == Class[] interfaces; 1 == Class[][] throws;
    if (GetDexFieldIndex() == 0) {
      return &declaring_class->GetSFieldsPtr()->At(0);
    } else {
      DCHECK_EQ(GetDexFieldIndex(), 1U);
      return &declaring_class->GetSFieldsPtr()->At(1);
    }
  }
  const ObjPtr<mirror::DexCache> dex_cache = declaring_class->GetDexCache();
  ArtField* art_field = dex_cache->GetResolvedField(GetDexFieldIndex(), kRuntimePointerSize);
  if (UNLIKELY(art_field == nullptr)) {
    if (IsStatic()) {
      art_field = declaring_class->FindDeclaredStaticField(dex_cache, GetDexFieldIndex());
    } else {
      art_field = declaring_class->FindInstanceField(dex_cache, GetDexFieldIndex());
    }
    CHECK(art_field != nullptr);
    dex_cache->SetResolvedField(GetDexFieldIndex(), art_field, kRuntimePointerSize);
  }
  CHECK_EQ(declaring_class, art_field->GetDeclaringClass());
  return art_field;
}

}  // namespace mirror
}  // namespace art
