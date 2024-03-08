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

#ifndef ART_RUNTIME_MIRROR_OBJECT_REFVISITOR_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_REFVISITOR_INL_H_

#include "object-inl.h"

#include "class-refvisitor-inl.h"
#include "class_loader-inl.h"
#include "dex_cache-inl.h"

namespace art {
namespace mirror {

template <bool kVisitNativeRoots,
          VerifyObjectFlags kVerifyFlags,
          ReadBarrierOption kReadBarrierOption,
          typename Visitor,
          typename JavaLangRefVisitor>
// jiacheng start
NO_INLINE void Object::VisitReferences(const Visitor& visitor,
                                    const JavaLangRefVisitor& ref_visitor) {
// inline void Object::VisitReferences(const Visitor& visitor,
//                                     const JavaLangRefVisitor& ref_visitor) {
// jiacheng end

  // marvin start
  if (GetStubFlag()) return;
  // SWAP_PREAMBLE_TEMPLATE_VOID(VisitReferences, Object, GATHER_TEMPLATE_ARGS(kVisitNativeRoots, kVerifyFlags, kReadBarrierOption, Visitor, JavaLangRefVisitor), visitor, ref_visitor)
  // marvin end

  // marvin start
  SetIgnoreReadFlag();
  // marvin end
  ObjPtr<Class> klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  visitor(this, ClassOffset(), /* is_static= */ false);
  // marvin start
  // Necessary in case the visitor changed the class reference
  klass = GetClass<kVerifyFlags, kReadBarrierOption>();
  // marvin end
  const uint32_t class_flags = klass->GetClassFlags<kVerifyNone>();
  if (LIKELY(class_flags == kClassFlagNormal)) {
    DCHECK((!klass->IsVariableSize<kVerifyFlags>()));
    VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
    DCHECK((!klass->IsClassClass<kVerifyFlags>()));
    DCHECK(!klass->IsStringClass<kVerifyFlags>());
    DCHECK(!klass->IsClassLoaderClass<kVerifyFlags>());
    DCHECK((!klass->IsArrayClass<kVerifyFlags>()));
  } else {
    if ((class_flags & kClassFlagNoReferenceFields) == 0) {
      DCHECK(!klass->IsStringClass<kVerifyFlags>());
      if (class_flags == kClassFlagClass) {
        DCHECK((klass->IsClassClass<kVerifyFlags>()));
        ObjPtr<Class> as_klass = AsClass<kVerifyNone>();
        as_klass->VisitReferences<kVisitNativeRoots, kVerifyFlags, kReadBarrierOption>(klass,
                                                                                       visitor);
      } else if (class_flags == kClassFlagObjectArray) {
        DCHECK((klass->IsObjectArrayClass<kVerifyFlags>()));
        AsObjectArray<mirror::Object, kVerifyNone>()->VisitReferences(visitor);
      } else if ((class_flags & kClassFlagReference) != 0) {
        VisitInstanceFieldsReferences<kVerifyFlags, kReadBarrierOption>(klass, visitor);
        ref_visitor(klass, AsReference<kVerifyFlags, kReadBarrierOption>());
      } else if (class_flags == kClassFlagDexCache) {
        ObjPtr<mirror::DexCache> const dex_cache = AsDexCache<kVerifyFlags, kReadBarrierOption>();
        dex_cache->VisitReferences<kVisitNativeRoots,
                                   kVerifyFlags,
                                   kReadBarrierOption>(klass, visitor);
      } else {
        ObjPtr<mirror::ClassLoader> const class_loader =
            AsClassLoader<kVerifyFlags, kReadBarrierOption>();
        class_loader->VisitReferences<kVisitNativeRoots,
                                      kVerifyFlags,
                                      kReadBarrierOption>(klass, visitor);
      }
    } else if (kIsDebugBuild) {
      CHECK((!klass->IsClassClass<kVerifyFlags>()));
      CHECK((!klass->IsObjectArrayClass<kVerifyFlags>()));
      // String still has instance fields for reflection purposes but these don't exist in
      // actual string instances.
      if (!klass->IsStringClass<kVerifyFlags>()) {
        size_t total_reference_instance_fields = 0;
        ObjPtr<Class> super_class = klass;
        do {
          total_reference_instance_fields +=
              super_class->NumReferenceInstanceFields<kVerifyFlags>();
          super_class = super_class->GetSuperClass<kVerifyFlags, kReadBarrierOption>();
        } while (super_class != nullptr);
        // The only reference field should be the object's class. This field is handled at the
        // beginning of the function.
        CHECK_EQ(total_reference_instance_fields, 1u);
      }
    }
  }
  // marvin start
  ClearIgnoreReadFlag();
  // marvin end
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_REFVISITOR_INL_H_
