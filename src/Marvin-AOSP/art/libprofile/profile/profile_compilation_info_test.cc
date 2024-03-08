/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <stdio.h>

#include "base/arena_allocator.h"
#include "base/common_art_test.h"
#include "base/unix_file/fd_file.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/method_reference.h"
#include "dex/type_reference.h"
#include "profile/profile_compilation_info.h"
#include "ziparchive/zip_writer.h"

namespace art {

using Hotness = ProfileCompilationInfo::MethodHotness;

static constexpr size_t kMaxMethodIds = 65535;

class ProfileCompilationInfoTest : public CommonArtTest {
 public:
  void SetUp() override {
    CommonArtTest::SetUp();
    allocator_.reset(new ArenaAllocator(&pool_));
  }

 protected:
  bool AddMethod(const std::string& dex_location,
                 uint32_t checksum,
                 uint16_t method_idx,
                 ProfileCompilationInfo* info) {
    return info->AddMethodIndex(Hotness::kFlagHot,
                                dex_location,
                                checksum,
                                method_idx,
                                kMaxMethodIds);
  }

  bool AddMethod(const std::string& dex_location,
                 uint32_t checksum,
                 uint16_t method_idx,
                 const ProfileCompilationInfo::OfflineProfileMethodInfo& pmi,
                 ProfileCompilationInfo* info) {
    return info->AddMethod(
        dex_location, checksum, method_idx, kMaxMethodIds, pmi, Hotness::kFlagPostStartup);
  }

  bool AddClass(const std::string& dex_location,
                uint32_t checksum,
                dex::TypeIndex type_index,
                ProfileCompilationInfo* info) {
    DexCacheResolvedClasses classes(dex_location, dex_location, checksum, kMaxMethodIds);
    classes.AddClass(type_index);
    return info->AddClasses({classes});
  }

  uint32_t GetFd(const ScratchFile& file) {
    return static_cast<uint32_t>(file.GetFd());
  }

  // Creates an inline cache which will be destructed at the end of the test.
  ProfileCompilationInfo::InlineCacheMap* CreateInlineCacheMap() {
    used_inline_caches.emplace_back(new ProfileCompilationInfo::InlineCacheMap(
        std::less<uint16_t>(), allocator_->Adapter(kArenaAllocProfile)));
    return used_inline_caches.back().get();
  }

  // Creates an offline profile used for testing inline caches.
  ProfileCompilationInfo::OfflineProfileMethodInfo GetOfflineProfileMethodInfo() {
    ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();

    // Monomorphic
    for (uint16_t dex_pc = 0; dex_pc < 11; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.AddClass(0, dex::TypeIndex(0));
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Polymorphic
    for (uint16_t dex_pc = 11; dex_pc < 22; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.AddClass(0, dex::TypeIndex(0));
      dex_pc_data.AddClass(1, dex::TypeIndex(1));
      dex_pc_data.AddClass(2, dex::TypeIndex(2));

      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Megamorphic
    for (uint16_t dex_pc = 22; dex_pc < 33; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMegamorphic();
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Missing types
    for (uint16_t dex_pc = 33; dex_pc < 44; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMissingTypes();
      ic_map->Put(dex_pc, dex_pc_data);
    }

    ProfileCompilationInfo::OfflineProfileMethodInfo pmi(ic_map);

    pmi.dex_references.emplace_back("dex_location1", /* checksum= */1, kMaxMethodIds);
    pmi.dex_references.emplace_back("dex_location2", /* checksum= */2, kMaxMethodIds);
    pmi.dex_references.emplace_back("dex_location3", /* checksum= */3, kMaxMethodIds);

    return pmi;
  }

  void MakeMegamorphic(/*out*/ProfileCompilationInfo::OfflineProfileMethodInfo* pmi) {
    ProfileCompilationInfo::InlineCacheMap* ic_map =
        const_cast<ProfileCompilationInfo::InlineCacheMap*>(pmi->inline_caches);
    for (auto it : *ic_map) {
      for (uint16_t k = 0; k <= 2 * ProfileCompilationInfo::kIndividualInlineCacheSize; k++) {
        it.second.AddClass(0, dex::TypeIndex(k));
      }
    }
  }

  void SetIsMissingTypes(/*out*/ProfileCompilationInfo::OfflineProfileMethodInfo* pmi) {
    ProfileCompilationInfo::InlineCacheMap* ic_map =
        const_cast<ProfileCompilationInfo::InlineCacheMap*>(pmi->inline_caches);
    for (auto it : *ic_map) {
      it.second.SetIsMissingTypes();
    }
  }

  void TestProfileLoadFromZip(const char* zip_entry,
                              size_t zip_flags,
                              bool should_succeed,
                              bool should_succeed_with_empty_profile = false) {
    // Create a valid profile.
    ScratchFile profile;
    ProfileCompilationInfo saved_info;
    for (uint16_t i = 0; i < 10; i++) {
      ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &saved_info));
      ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, /* method_idx= */ i, &saved_info));
    }
    ASSERT_TRUE(saved_info.Save(GetFd(profile)));
    ASSERT_EQ(0, profile.GetFile()->Flush());

    // Prepare the profile content for zipping.
    ASSERT_TRUE(profile.GetFile()->ResetOffset());
    std::vector<uint8_t> data(profile.GetFile()->GetLength());
    ASSERT_TRUE(profile.GetFile()->ReadFully(data.data(), data.size()));

    // Zip the profile content.
    ScratchFile zip;
    FILE* file = fopen(zip.GetFile()->GetPath().c_str(), "wb");
    ZipWriter writer(file);
    writer.StartEntry(zip_entry, zip_flags);
    writer.WriteBytes(data.data(), data.size());
    writer.FinishEntry();
    writer.Finish();
    fflush(file);
    fclose(file);

    // Verify loading from the zip archive.
    ProfileCompilationInfo loaded_info;
    ASSERT_TRUE(zip.GetFile()->ResetOffset());
    ASSERT_EQ(should_succeed, loaded_info.Load(zip.GetFile()->GetPath(), false));
    if (should_succeed) {
      if (should_succeed_with_empty_profile) {
        ASSERT_TRUE(loaded_info.IsEmpty());
      } else {
        ASSERT_TRUE(loaded_info.Equals(saved_info));
      }
    }
  }

  bool IsEmpty(const ProfileCompilationInfo& info) {
    return info.IsEmpty();
  }

  // Cannot sizeof the actual arrays so hard code the values here.
  // They should not change anyway.
  static constexpr int kProfileMagicSize = 4;
  static constexpr int kProfileVersionSize = 4;

  MallocArenaPool pool_;
  std::unique_ptr<ArenaAllocator> allocator_;

  // Cache of inline caches generated during tests.
  // This makes it easier to pass data between different utilities and ensure that
  // caches are destructed at the end of the test.
  std::vector<std::unique_ptr<ProfileCompilationInfo::InlineCacheMap>> used_inline_caches;
};

TEST_F(ProfileCompilationInfoTest, SaveFd) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  // Save a few methods.
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &saved_info));
    ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, /* method_idx= */ i, &saved_info));
  }
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(saved_info));

  // Save more methods.
  for (uint16_t i = 0; i < 100; i++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &saved_info));
    ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, /* method_idx= */ i, &saved_info));
    ASSERT_TRUE(AddMethod("dex_location3", /* checksum= */ 3, /* method_idx= */ i, &saved_info));
  }
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back everything we saved.
  ProfileCompilationInfo loaded_info2;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info2.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info2.Equals(saved_info));
}

TEST_F(ProfileCompilationInfoTest, AddMethodsAndClassesFail) {
  ScratchFile profile;

  ProfileCompilationInfo info;
  ASSERT_TRUE(AddMethod("dex_location", /* checksum= */ 1, /* method_idx= */ 1, &info));
  // Trying to add info for an existing file but with a different checksum.
  ASSERT_FALSE(AddMethod("dex_location", /* checksum= */ 2, /* method_idx= */ 2, &info));
}

TEST_F(ProfileCompilationInfoTest, MergeFail) {
  ScratchFile profile;

  ProfileCompilationInfo info1;
  ASSERT_TRUE(AddMethod("dex_location", /* checksum= */ 1, /* method_idx= */ 1, &info1));
  // Use the same file, change the checksum.
  ProfileCompilationInfo info2;
  ASSERT_TRUE(AddMethod("dex_location", /* checksum= */ 2, /* method_idx= */ 2, &info2));

  ASSERT_FALSE(info1.MergeWith(info2));
}


TEST_F(ProfileCompilationInfoTest, MergeFdFail) {
  ScratchFile profile;

  ProfileCompilationInfo info1;
  ASSERT_TRUE(AddMethod("dex_location", /* checksum= */ 1, /* method_idx= */ 1, &info1));
  // Use the same file, change the checksum.
  ProfileCompilationInfo info2;
  ASSERT_TRUE(AddMethod("dex_location", /* checksum= */ 2, /* method_idx= */ 2, &info2));

  ASSERT_TRUE(info1.Save(profile.GetFd()));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  ASSERT_FALSE(info2.Load(profile.GetFd()));
}

TEST_F(ProfileCompilationInfoTest, SaveMaxMethods) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  // Save the maximum number of methods
  for (uint16_t i = 0; i < std::numeric_limits<uint16_t>::max(); i++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &saved_info));
    ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, /* method_idx= */ i, &saved_info));
  }
  // Save the maximum number of classes
  for (uint16_t i = 0; i < std::numeric_limits<uint16_t>::max(); i++) {
    ASSERT_TRUE(AddClass("dex_location1", /* checksum= */ 1, dex::TypeIndex(i), &saved_info));
    ASSERT_TRUE(AddClass("dex_location2", /* checksum= */ 2, dex::TypeIndex(i), &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(saved_info));
}

TEST_F(ProfileCompilationInfoTest, SaveEmpty) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(saved_info));
}

TEST_F(ProfileCompilationInfoTest, LoadEmpty) {
  ScratchFile profile;

  ProfileCompilationInfo empty_info;

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(empty_info));
}

TEST_F(ProfileCompilationInfoTest, BadMagic) {
  ScratchFile profile;
  uint8_t buffer[] = { 1, 2, 3, 4 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(buffer, sizeof(buffer)));
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, BadVersion) {
  ScratchFile profile;

  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  uint8_t version[] = { 'v', 'e', 'r', 's', 'i', 'o', 'n' };
  ASSERT_TRUE(profile.GetFile()->WriteFully(version, sizeof(version)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, Incomplete) {
  ScratchFile profile;
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileVersion, kProfileVersionSize));
  // Write that we have at least one line.
  uint8_t line_number[] = { 0, 1 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line_number, sizeof(line_number)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, TooLongDexLocation) {
  ScratchFile profile;
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileVersion, kProfileVersionSize));
  // Write that we have at least one line.
  uint8_t line_number[] = { 0, 1 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line_number, sizeof(line_number)));

  // dex_location_size, methods_size, classes_size, checksum.
  // Dex location size is too big and should be rejected.
  uint8_t line[] = { 255, 255, 0, 1, 0, 1, 0, 0, 0, 0 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line, sizeof(line)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, UnexpectedContent) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  // Save the maximum number of methods
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &saved_info));
  }
  ASSERT_TRUE(saved_info.Save(GetFd(profile)));

  uint8_t random_data[] = { 1, 2, 3};
  ASSERT_TRUE(profile.GetFile()->WriteFully(random_data, sizeof(random_data)));

  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we fail because of unexpected data at the end of the file.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, SaveInlineCaches) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi = GetOfflineProfileMethodInfo();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the
    // class from the inline caches.
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod("dex_location4", /* checksum= */ 4, method_idx, pmi, &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));

  ASSERT_TRUE(loaded_info.Equals(saved_info));

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
      loaded_info.GetMethod("dex_location1", /* dex_checksum= */ 1, /* dex_method_index= */ 3);
  ASSERT_TRUE(loaded_pmi1 != nullptr);
  ASSERT_TRUE(*loaded_pmi1 == pmi);
  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi2 =
      loaded_info.GetMethod("dex_location4", /* dex_checksum= */ 4, /* dex_method_index= */ 3);
  ASSERT_TRUE(loaded_pmi2 != nullptr);
  ASSERT_TRUE(*loaded_pmi2 == pmi);
}

TEST_F(ProfileCompilationInfoTest, MegamorphicInlineCaches) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi = GetOfflineProfileMethodInfo();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Make the inline caches megamorphic and add them to the profile again.
  ProfileCompilationInfo saved_info_extra;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi_extra = GetOfflineProfileMethodInfo();
  MakeMegamorphic(&pmi_extra);
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info_extra));
  }

  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(saved_info_extra.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Merge the profiles so that we have the same view as the file.
  ASSERT_TRUE(saved_info.MergeWith(saved_info_extra));

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));

  ASSERT_TRUE(loaded_info.Equals(saved_info));

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
      loaded_info.GetMethod("dex_location1", /* dex_checksum= */ 1, /* dex_method_index= */ 3);

  ASSERT_TRUE(loaded_pmi1 != nullptr);
  ASSERT_TRUE(*loaded_pmi1 == pmi_extra);
}

TEST_F(ProfileCompilationInfoTest, MissingTypesInlineCaches) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi = GetOfflineProfileMethodInfo();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Make some inline caches megamorphic and add them to the profile again.
  ProfileCompilationInfo saved_info_extra;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi_extra = GetOfflineProfileMethodInfo();
  MakeMegamorphic(&pmi_extra);
  for (uint16_t method_idx = 5; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info_extra));
  }

  // Mark all inline caches with missing types and add them to the profile again.
  // This will verify that all inline caches (megamorphic or not) should be marked as missing types.
  ProfileCompilationInfo::OfflineProfileMethodInfo missing_types = GetOfflineProfileMethodInfo();
  SetIsMissingTypes(&missing_types);
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info_extra));
  }

  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(saved_info_extra.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Merge the profiles so that we have the same view as the file.
  ASSERT_TRUE(saved_info.MergeWith(saved_info_extra));

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));

  ASSERT_TRUE(loaded_info.Equals(saved_info));

  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
      loaded_info.GetMethod("dex_location1", /* dex_checksum= */ 1, /* dex_method_index= */ 3);
  ASSERT_TRUE(loaded_pmi1 != nullptr);
  ASSERT_TRUE(*loaded_pmi1 == pmi_extra);
}

TEST_F(ProfileCompilationInfoTest, InvalidChecksumInInlineCache) {
  ScratchFile profile;

  ProfileCompilationInfo info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi1 = GetOfflineProfileMethodInfo();
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi2 = GetOfflineProfileMethodInfo();
  // Modify the checksum to trigger a mismatch.
  pmi2.dex_references[0].dex_checksum++;

  ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /*method_idx=*/ 0, pmi1, &info));
  ASSERT_FALSE(AddMethod("dex_location2", /* checksum= */ 2, /*method_idx=*/ 0, pmi2, &info));
}

// Verify that profiles behave correctly even if the methods are added in a different
// order and with a different dex profile indices for the dex files.
TEST_F(ProfileCompilationInfoTest, MergeInlineCacheTriggerReindex) {
  ScratchFile profile;

  ProfileCompilationInfo info;
  ProfileCompilationInfo info_reindexed;

  ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi(ic_map);
  pmi.dex_references.emplace_back("dex_location1", /* checksum= */ 1, kMaxMethodIds);
  pmi.dex_references.emplace_back("dex_location2", /* checksum= */ 2, kMaxMethodIds);
  for (uint16_t dex_pc = 1; dex_pc < 5; dex_pc++) {
    ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
    dex_pc_data.AddClass(0, dex::TypeIndex(0));
    dex_pc_data.AddClass(1, dex::TypeIndex(1));
    ic_map->Put(dex_pc, dex_pc_data);
  }

  ProfileCompilationInfo::InlineCacheMap* ic_map_reindexed = CreateInlineCacheMap();
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi_reindexed(ic_map_reindexed);
  pmi_reindexed.dex_references.emplace_back("dex_location2", /* checksum= */ 2, kMaxMethodIds);
  pmi_reindexed.dex_references.emplace_back("dex_location1", /* checksum= */ 1, kMaxMethodIds);
  for (uint16_t dex_pc = 1; dex_pc < 5; dex_pc++) {
    ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
    dex_pc_data.AddClass(1, dex::TypeIndex(0));
    dex_pc_data.AddClass(0, dex::TypeIndex(1));
    ic_map_reindexed->Put(dex_pc, dex_pc_data);
  }

  // Profile 1 and Profile 2 get the same methods but in different order.
  // This will trigger a different dex numbers.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &info));
    ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, method_idx, pmi, &info));
  }

  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(AddMethod(
      "dex_location2", /* checksum= */ 2, method_idx, pmi_reindexed, &info_reindexed));
    ASSERT_TRUE(AddMethod(
      "dex_location1", /* checksum= */ 1, method_idx, pmi_reindexed, &info_reindexed));
  }

  ProfileCompilationInfo info_backup;
  info_backup.MergeWith(info);
  ASSERT_TRUE(info.MergeWith(info_reindexed));
  // Merging should have no effect as we're adding the exact same stuff.
  ASSERT_TRUE(info.Equals(info_backup));
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
        info.GetMethod("dex_location1", /* dex_checksum= */ 1, method_idx);
    ASSERT_TRUE(loaded_pmi1 != nullptr);
    ASSERT_TRUE(*loaded_pmi1 == pmi);
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi2 =
        info.GetMethod("dex_location2", /* dex_checksum= */ 2, method_idx);
    ASSERT_TRUE(loaded_pmi2 != nullptr);
    ASSERT_TRUE(*loaded_pmi2 == pmi);
  }
}

TEST_F(ProfileCompilationInfoTest, AddMoreDexFileThanLimit) {
  ProfileCompilationInfo info;
  // Save a few methods.
  for (uint16_t i = 0; i < std::numeric_limits<uint8_t>::max(); i++) {
    std::string dex_location = std::to_string(i);
    ASSERT_TRUE(AddMethod(dex_location, /* checksum= */ 1, /* method_idx= */ i, &info));
  }
  // We only support at most 255 dex files.
  ASSERT_FALSE(AddMethod(
      /*dex_location=*/ "256", /* checksum= */ 1, /* method_idx= */ 0, &info));
}

TEST_F(ProfileCompilationInfoTest, MegamorphicInlineCachesMerge) {
  // Create a megamorphic inline cache.
  ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi(ic_map);
  pmi.dex_references.emplace_back("dex_location1", /* checksum= */ 1, kMaxMethodIds);
  ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
  dex_pc_data.SetIsMegamorphic();
  ic_map->Put(/*dex_pc*/ 0, dex_pc_data);

  ProfileCompilationInfo info_megamorphic;
  ASSERT_TRUE(AddMethod("dex_location1",
                        /*checksum=*/ 1,
                        /*method_idx=*/ 0,
                        pmi,
                        &info_megamorphic));

  // Create a profile with no inline caches (for the same method).
  ProfileCompilationInfo info_no_inline_cache;
  ASSERT_TRUE(AddMethod("dex_location1",
                        /*checksum=*/ 1,
                        /*method_idx=*/ 0,
                        &info_no_inline_cache));

  // Merge the megamorphic cache into the empty one.
  ASSERT_TRUE(info_no_inline_cache.MergeWith(info_megamorphic));
  ScratchFile profile;
  // Saving profile should work without crashing (b/35644850).
  ASSERT_TRUE(info_no_inline_cache.Save(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, MissingTypesInlineCachesMerge) {
  // Create an inline cache with missing types
  ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi(ic_map);
  pmi.dex_references.emplace_back("dex_location1", /* checksum= */ 1, kMaxMethodIds);
  ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
  dex_pc_data.SetIsMissingTypes();
  ic_map->Put(/*dex_pc*/ 0, dex_pc_data);

  ProfileCompilationInfo info_megamorphic;
  ASSERT_TRUE(AddMethod("dex_location1",
                        /*checksum=*/ 1,
                        /*method_idx=*/ 0,
                        pmi,
                        &info_megamorphic));

  // Create a profile with no inline caches (for the same method).
  ProfileCompilationInfo info_no_inline_cache;
  ASSERT_TRUE(AddMethod("dex_location1",
                        /*checksum=*/ 1,
                        /*method_idx=*/ 0,
                        &info_no_inline_cache));

  // Merge the missing type cache into the empty one.
  // Everything should be saved without errors.
  ASSERT_TRUE(info_no_inline_cache.MergeWith(info_megamorphic));
  ScratchFile profile;
  ASSERT_TRUE(info_no_inline_cache.Save(GetFd(profile)));
}

TEST_F(ProfileCompilationInfoTest, SampledMethodsTest) {
  ProfileCompilationInfo test_info;
  static constexpr size_t kNumMethods = 1000;
  static constexpr size_t kChecksum1 = 1234;
  static constexpr size_t kChecksum2 = 4321;
  static const std::string kDex1 = "dex1";
  static const std::string kDex2 = "dex2";
  test_info.AddMethodIndex(Hotness::kFlagStartup, kDex1, kChecksum1, 1, kNumMethods);
  test_info.AddMethodIndex(Hotness::kFlagPostStartup, kDex1, kChecksum1, 5, kNumMethods);
  test_info.AddMethodIndex(Hotness::kFlagStartup, kDex2, kChecksum2, 2, kNumMethods);
  test_info.AddMethodIndex(Hotness::kFlagPostStartup, kDex2, kChecksum2, 4, kNumMethods);
  auto run_test = [](const ProfileCompilationInfo& info) {
    EXPECT_FALSE(info.GetMethodHotness(kDex1, kChecksum1, 2).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(kDex1, kChecksum1, 4).IsInProfile());
    EXPECT_TRUE(info.GetMethodHotness(kDex1, kChecksum1, 1).IsStartup());
    EXPECT_FALSE(info.GetMethodHotness(kDex1, kChecksum1, 3).IsStartup());
    EXPECT_TRUE(info.GetMethodHotness(kDex1, kChecksum1, 5).IsPostStartup());
    EXPECT_FALSE(info.GetMethodHotness(kDex1, kChecksum1, 6).IsStartup());
    EXPECT_TRUE(info.GetMethodHotness(kDex2, kChecksum2, 2).IsStartup());
    EXPECT_TRUE(info.GetMethodHotness(kDex2, kChecksum2, 4).IsPostStartup());
  };
  run_test(test_info);

  // Save the profile.
  ScratchFile profile;
  ASSERT_TRUE(test_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Load the profile and make sure we can read the data and it matches what we expect.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  run_test(loaded_info);

  // Test that the bitmap gets merged properly.
  EXPECT_FALSE(test_info.GetMethodHotness(kDex1, kChecksum1, 11).IsStartup());
  {
    ProfileCompilationInfo merge_info;
    merge_info.AddMethodIndex(Hotness::kFlagStartup, kDex1, kChecksum1, 11, kNumMethods);
    test_info.MergeWith(merge_info);
  }
  EXPECT_TRUE(test_info.GetMethodHotness(kDex1, kChecksum1, 11).IsStartup());

  // Test bulk adding.
  {
    std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
    ProfileCompilationInfo info;
    std::vector<uint16_t> hot_methods = {1, 3, 5};
    std::vector<uint16_t> startup_methods = {1, 2};
    std::vector<uint16_t> post_methods = {0, 2, 6};
    ASSERT_GE(dex->NumMethodIds(), 7u);
    info.AddMethodsForDex(static_cast<Hotness::Flag>(Hotness::kFlagHot | Hotness::kFlagStartup),
                          dex.get(),
                          hot_methods.begin(),
                          hot_methods.end());
    info.AddMethodsForDex(Hotness::kFlagStartup,
                          dex.get(),
                          startup_methods.begin(),
                          startup_methods.end());
    info.AddMethodsForDex(Hotness::kFlagPostStartup,
                          dex.get(),
                          post_methods.begin(),
                          post_methods.end());
    for (uint16_t id : hot_methods) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsHot());
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsStartup());
    }
    for (uint16_t id : startup_methods) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsStartup());
    }
    for (uint16_t id : post_methods) {
      EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsPostStartup());
    }
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), 6)).IsPostStartup());
    // Check that methods that shouldn't have been touched are OK.
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), 0)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 4)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 7)).IsInProfile());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 1)).IsPostStartup());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 4)).IsStartup());
    EXPECT_FALSE(info.GetMethodHotness(MethodReference(dex.get(), 6)).IsStartup());
  }
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipCompress) {
  TestProfileLoadFromZip("primary.prof",
                         ZipWriter::kCompress | ZipWriter::kAlign32,
                         /*should_succeed=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipUnCompress) {
  TestProfileLoadFromZip("primary.prof",
                         ZipWriter::kAlign32,
                         /*should_succeed=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipUnAligned) {
  TestProfileLoadFromZip("primary.prof",
                         0,
                         /*should_succeed=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipFailBadZipEntry) {
  TestProfileLoadFromZip("invalid.profile.entry",
                         0,
                         /*should_succeed=*/true,
                         /*should_succeed_with_empty_profile=*/true);
}

TEST_F(ProfileCompilationInfoTest, LoadFromZipFailBadProfile) {
  // Create a bad profile.
  ScratchFile profile;
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileMagic, kProfileMagicSize));
  ASSERT_TRUE(profile.GetFile()->WriteFully(
      ProfileCompilationInfo::kProfileVersion, kProfileVersionSize));
  // Write that we have at least one line.
  uint8_t line_number[] = { 0, 1 };
  ASSERT_TRUE(profile.GetFile()->WriteFully(line_number, sizeof(line_number)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Prepare the profile content for zipping.
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  std::vector<uint8_t> data(profile.GetFile()->GetLength());
  ASSERT_TRUE(profile.GetFile()->ReadFully(data.data(), data.size()));

  // Zip the profile content.
  ScratchFile zip;
  FILE* file = fopen(zip.GetFile()->GetPath().c_str(), "wb");
  ZipWriter writer(file);
  writer.StartEntry("primary.prof", ZipWriter::kAlign32);
  writer.WriteBytes(data.data(), data.size());
  writer.FinishEntry();
  writer.Finish();
  fflush(file);
  fclose(file);

  // Check that we failed to load.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(zip.GetFile()->ResetOffset());
  ASSERT_FALSE(loaded_info.Load(GetFd(zip)));
}

TEST_F(ProfileCompilationInfoTest, UpdateProfileKeyOk) {
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("MultiDex");

  ProfileCompilationInfo info;
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    // Create the profile with a different location so that we can update it to the
    // real dex location later.
    std::string base_location = DexFileLoader::GetBaseLocation(dex->GetLocation());
    std::string multidex_suffix = DexFileLoader::GetMultiDexSuffix(dex->GetLocation());
    std::string old_name = base_location + "-old" + multidex_suffix;
    info.AddMethodIndex(Hotness::kFlagHot,
                        old_name,
                        dex->GetLocationChecksum(),
                        /* method_idx= */ 0,
                        dex->NumMethodIds());
  }

  // Update the profile keys based on the original dex files
  ASSERT_TRUE(info.UpdateProfileKeys(dex_files));

  // Verify that we find the methods when searched with the original dex files.
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi =
        info.GetMethod(dex->GetLocation(), dex->GetLocationChecksum(), /* dex_method_index= */ 0);
    ASSERT_TRUE(loaded_pmi != nullptr);
  }
}

TEST_F(ProfileCompilationInfoTest, UpdateProfileKeyOkButNoUpdate) {
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("MultiDex");

  ProfileCompilationInfo info;
  info.AddMethodIndex(Hotness::kFlagHot,
                      "my.app",
                      /* checksum= */ 123,
                      /* method_idx= */ 0,
                      /* num_method_ids= */ 10);

  // Update the profile keys based on the original dex files
  ASSERT_TRUE(info.UpdateProfileKeys(dex_files));

  // Verify that we did not perform any update and that we cannot find anything with the new
  // location.
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi =
        info.GetMethod(dex->GetLocation(), dex->GetLocationChecksum(), /* dex_method_index= */ 0);
    ASSERT_TRUE(loaded_pmi == nullptr);
  }

  // Verify that we can find the original entry.
  std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi =
        info.GetMethod("my.app", /* dex_checksum= */ 123, /* dex_method_index= */ 0);
  ASSERT_TRUE(loaded_pmi != nullptr);
}

TEST_F(ProfileCompilationInfoTest, UpdateProfileKeyFail) {
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("MultiDex");


  ProfileCompilationInfo info;
  // Add all dex
  for (const std::unique_ptr<const DexFile>& dex : dex_files) {
    // Create the profile with a different location so that we can update it to the
    // real dex location later.
    std::string base_location = DexFileLoader::GetBaseLocation(dex->GetLocation());
    std::string multidex_suffix = DexFileLoader::GetMultiDexSuffix(dex->GetLocation());
    std::string old_name = base_location + "-old" + multidex_suffix;
    info.AddMethodIndex(Hotness::kFlagHot,
                        old_name,
                        dex->GetLocationChecksum(),
                        /* method_idx= */ 0,
                        dex->NumMethodIds());
  }

  // Add a method index using the location we want to rename to.
  // This will cause the rename to fail because an existing entry would already have that name.
  info.AddMethodIndex(Hotness::kFlagHot,
                      dex_files[0]->GetLocation(),
                      /* checksum= */ 123,
                      /* method_idx= */ 0,
                      dex_files[0]->NumMethodIds());

  ASSERT_FALSE(info.UpdateProfileKeys(dex_files));
}

TEST_F(ProfileCompilationInfoTest, FilteredLoading) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi = GetOfflineProfileMethodInfo();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the class from the inline caches.
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info));
    ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, method_idx, pmi, &saved_info));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod("dex_location4", /* checksum= */ 4, method_idx, pmi, &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Filter out dex locations. Keep only dex_location1 and dex_location3.
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [](const std::string& dex_location, uint32_t checksum) -> bool {
          return (dex_location == "dex_location1" && checksum == 1)
              || (dex_location == "dex_location3" && checksum == 3);
        };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));

  // Verify that we filtered out locations during load.

  // Dex location 2 and 4 should have been filtered out
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    ASSERT_TRUE(nullptr == loaded_info.GetMethod("dex_location2",
                                                 /* dex_checksum= */ 2,
                                                 method_idx));
    ASSERT_TRUE(nullptr == loaded_info.GetMethod("dex_location4",
                                                 /* dex_checksum= */ 4,
                                                 method_idx));
  }

  // Dex location 1 should have all all the inline caches referencing dex location 2 set to
  // missing types.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // The methods for dex location 1 should be in the profile data.
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
        loaded_info.GetMethod("dex_location1", /* dex_checksum= */ 1, method_idx);
    ASSERT_TRUE(loaded_pmi1 != nullptr);

    // Verify the inline cache.
    // Everything should be as constructed by GetOfflineProfileMethodInfo with the exception
    // of the inline caches referring types from dex_location2.
    // These should be set to IsMissingType.
    ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();

    // Monomorphic types should remain the same as dex_location1 was kept.
    for (uint16_t dex_pc = 0; dex_pc < 11; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.AddClass(0, dex::TypeIndex(0));
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Polymorphic inline cache should have been transformed to IsMissingType due to
    // the removal of dex_location2.
    for (uint16_t dex_pc = 11; dex_pc < 22; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMissingTypes();
      ic_map->Put(dex_pc, dex_pc_data);
    }

    // Megamorphic are not affected by removal of dex files.
    for (uint16_t dex_pc = 22; dex_pc < 33; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMegamorphic();
      ic_map->Put(dex_pc, dex_pc_data);
    }
    // Missing types are not affected be removal of dex files.
    for (uint16_t dex_pc = 33; dex_pc < 44; dex_pc++) {
      ProfileCompilationInfo::DexPcData dex_pc_data(allocator_.get());
      dex_pc_data.SetIsMissingTypes();
      ic_map->Put(dex_pc, dex_pc_data);
    }

    ProfileCompilationInfo::OfflineProfileMethodInfo expected_pmi(ic_map);

    // The dex references should not have  dex_location2 in the list.
    expected_pmi.dex_references.emplace_back("dex_location1", /* checksum= */1, kMaxMethodIds);
    expected_pmi.dex_references.emplace_back("dex_location3", /* checksum= */3, kMaxMethodIds);

    // Now check that we get back what we expect.
    ASSERT_TRUE(*loaded_pmi1 == expected_pmi);
  }
}

TEST_F(ProfileCompilationInfoTest, FilteredLoadingRemoveAll) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi = GetOfflineProfileMethodInfo();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the class from the inline caches.
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info));
    ASSERT_TRUE(AddMethod("dex_location2", /* checksum= */ 2, method_idx, pmi, &saved_info));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod("dex_location4", /* checksum= */ 4, method_idx, pmi, &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Remove all elements.
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [](const std::string&, uint32_t) -> bool { return false; };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));

  // Verify that we filtered out everything.
  ASSERT_TRUE(IsEmpty(loaded_info));
}

TEST_F(ProfileCompilationInfoTest, FilteredLoadingKeepAll) {
  ScratchFile profile;

  ProfileCompilationInfo saved_info;
  ProfileCompilationInfo::OfflineProfileMethodInfo pmi = GetOfflineProfileMethodInfo();

  // Add methods with inline caches.
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    // Add a method which is part of the same dex file as one of the
    // class from the inline caches.
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, method_idx, pmi, &saved_info));
    // Add a method which is outside the set of dex files.
    ASSERT_TRUE(AddMethod("dex_location4", /* checksum= */ 4, method_idx, pmi, &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());

  // Keep all elements.
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [](const std::string&, uint32_t) -> bool { return true; };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));


  ASSERT_TRUE(loaded_info.Equals(saved_info));

  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi1 =
        loaded_info.GetMethod("dex_location1", /* dex_checksum= */ 1, method_idx);
    ASSERT_TRUE(loaded_pmi1 != nullptr);
    ASSERT_TRUE(*loaded_pmi1 == pmi);
  }
  for (uint16_t method_idx = 0; method_idx < 10; method_idx++) {
    std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> loaded_pmi2 =
        loaded_info.GetMethod("dex_location4", /* dex_checksum= */ 4, method_idx);
    ASSERT_TRUE(loaded_pmi2 != nullptr);
    ASSERT_TRUE(*loaded_pmi2 == pmi);
  }
}

// Regression test: we were failing to do a filtering loading when the filtered dex file
// contained profiled classes.
TEST_F(ProfileCompilationInfoTest, FilteredLoadingWithClasses) {
  ScratchFile profile;

  // Save a profile with 2 dex files containing just classes.
  ProfileCompilationInfo saved_info;
  uint16_t item_count = 1000;
  for (uint16_t i = 0; i < item_count; i++) {
    ASSERT_TRUE(AddClass("dex_location1", /* checksum= */ 1, dex::TypeIndex(i), &saved_info));
    ASSERT_TRUE(AddClass("dex_location2", /* checksum= */ 2, dex::TypeIndex(i), &saved_info));
  }

  ASSERT_TRUE(saved_info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());


  // Filter out dex locations: kepp only dex_location2.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ProfileCompilationInfo::ProfileLoadFilterFn filter_fn =
      [](const std::string& dex_location, uint32_t checksum) -> bool {
          return (dex_location == "dex_location2" && checksum == 2);
        };
  ASSERT_TRUE(loaded_info.Load(GetFd(profile), true, filter_fn));

  // Compute the expectation.
  ProfileCompilationInfo expected_info;
  for (uint16_t i = 0; i < item_count; i++) {
    ASSERT_TRUE(AddClass("dex_location2", /* checksum= */ 2, dex::TypeIndex(i), &expected_info));
  }

  // Validate the expectation.
  ASSERT_TRUE(loaded_info.Equals(expected_info));
}


TEST_F(ProfileCompilationInfoTest, ClearData) {
  ProfileCompilationInfo info;
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &info));
  }
  ASSERT_FALSE(IsEmpty(info));
  info.ClearData();
  ASSERT_TRUE(IsEmpty(info));
}

TEST_F(ProfileCompilationInfoTest, ClearDataAndSave) {
  ProfileCompilationInfo info;
  for (uint16_t i = 0; i < 10; i++) {
    ASSERT_TRUE(AddMethod("dex_location1", /* checksum= */ 1, /* method_idx= */ i, &info));
  }
  info.ClearData();

  ScratchFile profile;
  ASSERT_TRUE(info.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(info));
}

TEST_F(ProfileCompilationInfoTest, PrepareForAggregationCounters) {
  ProfileCompilationInfo info;
  ASSERT_EQ(
      memcmp(info.GetVersion(),
             ProfileCompilationInfo::kProfileVersion,
             ProfileCompilationInfo::kProfileVersionSize),
      0);

  info.PrepareForAggregationCounters();

  ASSERT_EQ(
      memcmp(info.GetVersion(),
             ProfileCompilationInfo::kProfileVersionWithCounters,
             ProfileCompilationInfo::kProfileVersionSize),
      0);
  ASSERT_TRUE(info.StoresAggregationCounters());
  ASSERT_EQ(info.GetAggregationCounter(), 0);
}

TEST_F(ProfileCompilationInfoTest, MergeWithAggregationCounters) {
  ProfileCompilationInfo info1;
  info1.PrepareForAggregationCounters();

  ProfileCompilationInfo info2;
  ProfileCompilationInfo info3;

  std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
  std::string location = dex->GetLocation();
  int checksum = dex->GetLocationChecksum();

  AddMethod(location, checksum, /* method_idx= */ 1, &info1);

  AddMethod(location, checksum, /* method_idx= */ 2, &info1);
  AddMethod(location, checksum, /* method_idx= */ 2, &info2);

  info1.AddMethodIndex(Hotness::kFlagStartup, location, checksum, 3, kMaxMethodIds);
  info2.AddMethodIndex(Hotness::kFlagPostStartup, location, checksum, 3, kMaxMethodIds);
  info3.AddMethodIndex(Hotness::kFlagStartup, location, checksum, 3, kMaxMethodIds);

  AddMethod(location, checksum, /* method_idx= */ 6, &info2);
  AddMethod(location, checksum, /* method_idx= */ 6, &info3);

  AddClass(location, checksum, dex::TypeIndex(10), &info1);

  AddClass(location, checksum, dex::TypeIndex(20), &info1);
  AddClass(location, checksum, dex::TypeIndex(20), &info2);

  AddClass(location, checksum, dex::TypeIndex(30), &info1);
  AddClass(location, checksum, dex::TypeIndex(30), &info2);
  AddClass(location, checksum, dex::TypeIndex(30), &info3);

  ASSERT_EQ(info1.GetAggregationCounter(), 0);
  info1.MergeWith(info2);
  ASSERT_EQ(info1.GetAggregationCounter(), 1);
  info1.MergeWith(info3);
  ASSERT_EQ(info1.GetAggregationCounter(), 2);

  ASSERT_EQ(0, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 1)));
  ASSERT_EQ(1, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 2)));
  ASSERT_EQ(2, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 3)));
  ASSERT_EQ(1, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 6)));

  ASSERT_EQ(0, info1.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(10))));
  ASSERT_EQ(1, info1.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(20))));
  ASSERT_EQ(2, info1.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(30))));

  // Check methods that do not exists.
  ASSERT_EQ(-1, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 4)));
  ASSERT_EQ(-1, info1.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(40))));
}

TEST_F(ProfileCompilationInfoTest, SaveAndLoadAggregationCounters) {
  ProfileCompilationInfo info1;
  info1.PrepareForAggregationCounters();

  ProfileCompilationInfo info2;
  ProfileCompilationInfo info3;

  std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
  std::string location = dex->GetLocation();
  int checksum = dex->GetLocationChecksum();

  AddMethod(location, checksum, /* method_idx= */ 1, &info1);

  AddMethod(location, checksum, /* method_idx= */ 2, &info1);
  AddMethod(location, checksum, /* method_idx= */ 2, &info2);

  info1.AddMethodIndex(Hotness::kFlagStartup, location, checksum, 3, kMaxMethodIds);
  info2.AddMethodIndex(Hotness::kFlagPostStartup, location, checksum, 3, kMaxMethodIds);
  info3.AddMethodIndex(Hotness::kFlagStartup, location, checksum, 3, kMaxMethodIds);

  AddMethod(location, checksum, /* method_idx= */ 6, &info2);
  AddMethod(location, checksum, /* method_idx= */ 6, &info3);

  AddClass(location, checksum, dex::TypeIndex(10), &info1);

  AddClass(location, checksum, dex::TypeIndex(20), &info1);
  AddClass(location, checksum, dex::TypeIndex(20), &info2);

  AddClass(location, checksum, dex::TypeIndex(30), &info1);
  AddClass(location, checksum, dex::TypeIndex(30), &info2);
  AddClass(location, checksum, dex::TypeIndex(30), &info3);

  info1.MergeWith(info2);
  info1.MergeWith(info3);

  ScratchFile profile;

  ASSERT_TRUE(info1.Save(GetFd(profile)));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  // Check that we get back what we saved.
  ProfileCompilationInfo loaded_info;
  loaded_info.PrepareForAggregationCounters();
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(loaded_info.Load(GetFd(profile)));
  ASSERT_TRUE(loaded_info.Equals(info1));

  ASSERT_EQ(2, loaded_info.GetAggregationCounter());

  ASSERT_EQ(0, loaded_info.GetMethodAggregationCounter(MethodReference(dex.get(), 1)));
  ASSERT_EQ(1, loaded_info.GetMethodAggregationCounter(MethodReference(dex.get(), 2)));
  ASSERT_EQ(2, loaded_info.GetMethodAggregationCounter(MethodReference(dex.get(), 3)));
  ASSERT_EQ(1, loaded_info.GetMethodAggregationCounter(MethodReference(dex.get(), 6)));

  ASSERT_EQ(0, loaded_info.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(10))));
  ASSERT_EQ(1, loaded_info.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(20))));
  ASSERT_EQ(2, loaded_info.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(30))));
}

TEST_F(ProfileCompilationInfoTest, MergeTwoWithAggregationCounters) {
  ProfileCompilationInfo info1;
  info1.PrepareForAggregationCounters();

  ProfileCompilationInfo info2;

  std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
  std::string location = dex->GetLocation();
  int checksum = dex->GetLocationChecksum();

  AddMethod(location, checksum, /* method_idx= */ 1, &info1);

  AddMethod(location, checksum, /* method_idx= */ 2, &info1);
  AddMethod(location, checksum, /* method_idx= */ 2, &info2);

  AddClass(location, checksum, dex::TypeIndex(20), &info1);

  AddClass(location, checksum, dex::TypeIndex(10), &info1);
  AddClass(location, checksum, dex::TypeIndex(10), &info2);

  info1.MergeWith(info2);
  info1.MergeWith(info2);
  ASSERT_EQ(2, info1.GetAggregationCounter());

  // Save and load the profile to create a copy of the data
  ScratchFile profile;
  info1.Save(GetFd(profile));
  ASSERT_EQ(0, profile.GetFile()->Flush());

  ProfileCompilationInfo loaded_info;
  loaded_info.PrepareForAggregationCounters();
  profile.GetFile()->ResetOffset();
  loaded_info.Load(GetFd(profile));

  // Merge the data
  info1.MergeWith(loaded_info);

  ASSERT_EQ(4, info1.GetAggregationCounter());

  ASSERT_EQ(0, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 1)));
  ASSERT_EQ(4, info1.GetMethodAggregationCounter(MethodReference(dex.get(), 2)));

  ASSERT_EQ(4, info1.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(10))));
  ASSERT_EQ(0, info1.GetClassAggregationCounter(TypeReference(dex.get(), dex::TypeIndex(20))));
}

}  // namespace art
