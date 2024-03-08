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

#ifndef ART_DEX2OAT_LINKER_IMAGE_TEST_H_
#define ART_DEX2OAT_LINKER_IMAGE_TEST_H_

#include "image.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "art_method-inl.h"
#include "base/file_utils.h"
#include "base/hash_set.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "class_linker-inl.h"
#include "common_compiler_driver_test.h"
#include "compiler_callbacks.h"
#include "debug/method_debug_info.h"
#include "dex/quick_compiler_callbacks.h"
#include "dex/signature-inl.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "gc/space/image_space.h"
#include "image_writer.h"
#include "linker/elf_writer.h"
#include "linker/elf_writer_quick.h"
#include "linker/multi_oat_relative_patcher.h"
#include "lock_word.h"
#include "mirror/object-inl.h"
#include "oat_writer.h"
#include "scoped_thread_state_change-inl.h"
#include "signal_catcher.h"
#include "stream/buffered_output_stream.h"
#include "stream/file_output_stream.h"

namespace art {
namespace linker {

static const uintptr_t kRequestedImageBase = ART_BASE_ADDRESS;

struct CompilationHelper {
  std::vector<std::string> dex_file_locations;
  std::vector<ScratchFile> image_locations;
  std::vector<std::unique_ptr<const DexFile>> extra_dex_files;
  std::vector<ScratchFile> image_files;
  std::vector<ScratchFile> oat_files;
  std::vector<ScratchFile> vdex_files;
  std::string image_dir;

  std::vector<size_t> GetImageObjectSectionSizes();

  ~CompilationHelper();
};

class ImageTest : public CommonCompilerDriverTest {
 protected:
  void SetUp() override {
    ReserveImageSpace();
    CommonCompilerTest::SetUp();
  }

  void TestWriteRead(ImageHeader::StorageMode storage_mode, uint32_t max_image_block_size);

  void Compile(ImageHeader::StorageMode storage_mode,
               uint32_t max_image_block_size,
               /*out*/ CompilationHelper& out_helper,
               const std::string& extra_dex = "",
               const std::initializer_list<std::string>& image_classes = {},
               const std::initializer_list<std::string>& image_classes_failing_aot_clinit = {});

  void SetUpRuntimeOptions(RuntimeOptions* options) override {
    CommonCompilerTest::SetUpRuntimeOptions(options);
    QuickCompilerCallbacks* new_callbacks =
        new QuickCompilerCallbacks(CompilerCallbacks::CallbackMode::kCompileBootImage);
    new_callbacks->SetVerificationResults(verification_results_.get());
    callbacks_.reset(new_callbacks);
    options->push_back(std::make_pair("compilercallbacks", callbacks_.get()));
  }

  std::unique_ptr<HashSet<std::string>> GetImageClasses() override {
    return std::make_unique<HashSet<std::string>>(image_classes_);
  }

  ArtMethod* FindCopiedMethod(ArtMethod* origin, ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    PointerSize pointer_size = class_linker_->GetImagePointerSize();
    for (ArtMethod& m : klass->GetCopiedMethods(pointer_size)) {
      if (strcmp(origin->GetName(), m.GetName()) == 0 &&
          origin->GetSignature() == m.GetSignature()) {
        return &m;
      }
    }
    return nullptr;
  }

 private:
  void DoCompile(ImageHeader::StorageMode storage_mode, /*out*/ CompilationHelper& out_helper);

  HashSet<std::string> image_classes_;
};

inline CompilationHelper::~CompilationHelper() {
  for (ScratchFile& image_file : image_files) {
    image_file.Unlink();
  }
  for (ScratchFile& oat_file : oat_files) {
    oat_file.Unlink();
  }
  for (ScratchFile& vdex_file : vdex_files) {
    vdex_file.Unlink();
  }
  const int rmdir_result = rmdir(image_dir.c_str());
  CHECK_EQ(0, rmdir_result);
}

inline std::vector<size_t> CompilationHelper::GetImageObjectSectionSizes() {
  std::vector<size_t> ret;
  for (ScratchFile& image_file : image_files) {
    std::unique_ptr<File> file(OS::OpenFileForReading(image_file.GetFilename().c_str()));
    CHECK(file.get() != nullptr);
    ImageHeader image_header;
    CHECK_EQ(file->ReadFully(&image_header, sizeof(image_header)), true);
    CHECK(image_header.IsValid());
    ret.push_back(image_header.GetObjectsSection().Size());
  }
  return ret;
}

inline void ImageTest::DoCompile(ImageHeader::StorageMode storage_mode,
                                 /*out*/ CompilationHelper& out_helper) {
  CompilerDriver* driver = compiler_driver_.get();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  std::vector<const DexFile*> class_path = class_linker->GetBootClassPath();

  for (const std::unique_ptr<const DexFile>& dex_file : out_helper.extra_dex_files) {
    {
      ScopedObjectAccess soa(Thread::Current());
      // Inject in boot class path so that the compiler driver can see it.
      class_linker->AppendToBootClassPath(soa.Self(), *dex_file.get());
    }
    class_path.push_back(dex_file.get());
  }

  // Enable write for dex2dex.
  for (const DexFile* dex_file : class_path) {
    out_helper.dex_file_locations.push_back(dex_file->GetLocation());
    if (dex_file->IsReadOnly()) {
      dex_file->EnableWrite();
    }
  }
  {
    // Create a generic tmp file, to be the base of the .art and .oat temporary files.
    ScratchFile location;
    std::vector<std::string> image_locations =
        gc::space::ImageSpace::ExpandMultiImageLocations(out_helper.dex_file_locations,
                                                         location.GetFilename() + ".art");
    for (size_t i = 0u; i != class_path.size(); ++i) {
      out_helper.image_locations.push_back(ScratchFile(image_locations[i]));
    }
  }
  std::vector<std::string> image_filenames;
  for (ScratchFile& file : out_helper.image_locations) {
    std::string image_filename(GetSystemImageFilename(file.GetFilename().c_str(), kRuntimeISA));
    image_filenames.push_back(image_filename);
    size_t pos = image_filename.rfind('/');
    CHECK_NE(pos, std::string::npos) << image_filename;
    if (out_helper.image_dir.empty()) {
      out_helper.image_dir = image_filename.substr(0, pos);
      int mkdir_result = mkdir(out_helper.image_dir.c_str(), 0700);
      CHECK_EQ(0, mkdir_result) << out_helper.image_dir;
    }
    out_helper.image_files.push_back(ScratchFile(OS::CreateEmptyFile(image_filename.c_str())));
  }

  std::vector<std::string> oat_filenames;
  std::vector<std::string> vdex_filenames;
  for (const std::string& image_filename : image_filenames) {
    std::string oat_filename = ReplaceFileExtension(image_filename, "oat");
    out_helper.oat_files.push_back(ScratchFile(OS::CreateEmptyFile(oat_filename.c_str())));
    oat_filenames.push_back(oat_filename);
    std::string vdex_filename = ReplaceFileExtension(image_filename, "vdex");
    out_helper.vdex_files.push_back(ScratchFile(OS::CreateEmptyFile(vdex_filename.c_str())));
    vdex_filenames.push_back(vdex_filename);
  }

  std::unordered_map<const DexFile*, size_t> dex_file_to_oat_index_map;
  size_t image_idx = 0;
  for (const DexFile* dex_file : class_path) {
    dex_file_to_oat_index_map.emplace(dex_file, image_idx);
    ++image_idx;
  }
  // TODO: compile_pic should be a test argument.
  std::unique_ptr<ImageWriter> writer(new ImageWriter(*compiler_options_,
                                                      kRequestedImageBase,
                                                      storage_mode,
                                                      oat_filenames,
                                                      dex_file_to_oat_index_map,
                                                      /*class_loader=*/ nullptr,
                                                      /*dirty_image_objects=*/ nullptr));
  {
    {
      jobject class_loader = nullptr;
      TimingLogger timings("ImageTest::WriteRead", false, false);
      CompileAll(class_loader, class_path, &timings);

      TimingLogger::ScopedTiming t("WriteElf", &timings);
      SafeMap<std::string, std::string> key_value_store;
      key_value_store.Put(OatHeader::kBootClassPathKey,
                          android::base::Join(out_helper.dex_file_locations, ':'));

      std::vector<std::unique_ptr<ElfWriter>> elf_writers;
      std::vector<std::unique_ptr<OatWriter>> oat_writers;
      for (ScratchFile& oat_file : out_helper.oat_files) {
        elf_writers.emplace_back(CreateElfWriterQuick(*compiler_options_, oat_file.GetFile()));
        elf_writers.back()->Start();
        oat_writers.emplace_back(new OatWriter(*compiler_options_,
                                               &timings,
                                               /*profile_compilation_info*/nullptr,
                                               CompactDexLevel::kCompactDexLevelNone));
      }

      std::vector<OutputStream*> rodata;
      std::vector<MemMap> opened_dex_files_maps;
      std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
      // Now that we have finalized key_value_store_, start writing the oat file.
      for (size_t i = 0, size = oat_writers.size(); i != size; ++i) {
        const DexFile* dex_file = class_path[i];
        rodata.push_back(elf_writers[i]->StartRoData());
        ArrayRef<const uint8_t> raw_dex_file(
            reinterpret_cast<const uint8_t*>(&dex_file->GetHeader()),
            dex_file->GetHeader().file_size_);
        oat_writers[i]->AddRawDexFileSource(raw_dex_file,
                                            dex_file->GetLocation().c_str(),
                                            dex_file->GetLocationChecksum());

        std::vector<MemMap> cur_opened_dex_files_maps;
        std::vector<std::unique_ptr<const DexFile>> cur_opened_dex_files;
        bool dex_files_ok = oat_writers[i]->WriteAndOpenDexFiles(
            out_helper.vdex_files[i].GetFile(),
            rodata.back(),
            (i == 0u) ? &key_value_store : nullptr,
            /* verify */ false,           // Dex files may be dex-to-dex-ed, don't verify.
            /* update_input_vdex */ false,
            /* copy_dex_files */ CopyOption::kOnlyIfCompressed,
            &cur_opened_dex_files_maps,
            &cur_opened_dex_files);
        ASSERT_TRUE(dex_files_ok);

        if (!cur_opened_dex_files_maps.empty()) {
          for (MemMap& cur_map : cur_opened_dex_files_maps) {
            opened_dex_files_maps.push_back(std::move(cur_map));
          }
          for (std::unique_ptr<const DexFile>& cur_dex_file : cur_opened_dex_files) {
            // dex_file_oat_index_map_.emplace(dex_file.get(), i);
            opened_dex_files.push_back(std::move(cur_dex_file));
          }
        } else {
          ASSERT_TRUE(cur_opened_dex_files.empty());
        }
      }
      bool image_space_ok = writer->PrepareImageAddressSpace(&timings);
      ASSERT_TRUE(image_space_ok);

      DCHECK_EQ(out_helper.vdex_files.size(), out_helper.oat_files.size());
      for (size_t i = 0, size = out_helper.oat_files.size(); i != size; ++i) {
        MultiOatRelativePatcher patcher(compiler_options_->GetInstructionSet(),
                                        compiler_options_->GetInstructionSetFeatures(),
                                        driver->GetCompiledMethodStorage());
        OatWriter* const oat_writer = oat_writers[i].get();
        ElfWriter* const elf_writer = elf_writers[i].get();
        std::vector<const DexFile*> cur_dex_files(1u, class_path[i]);
        oat_writer->Initialize(driver, writer.get(), cur_dex_files);

        std::unique_ptr<BufferedOutputStream> vdex_out =
            std::make_unique<BufferedOutputStream>(
                std::make_unique<FileOutputStream>(out_helper.vdex_files[i].GetFile()));
        oat_writer->WriteVerifierDeps(vdex_out.get(), nullptr);
        oat_writer->WriteQuickeningInfo(vdex_out.get());
        oat_writer->WriteChecksumsAndVdexHeader(vdex_out.get());

        oat_writer->PrepareLayout(&patcher);
        elf_writer->PrepareDynamicSection(oat_writer->GetOatHeader().GetExecutableOffset(),
                                          oat_writer->GetCodeSize(),
                                          oat_writer->GetDataBimgRelRoSize(),
                                          oat_writer->GetBssSize(),
                                          oat_writer->GetBssMethodsOffset(),
                                          oat_writer->GetBssRootsOffset(),
                                          oat_writer->GetVdexSize());

        writer->UpdateOatFileLayout(i,
                                    elf_writer->GetLoadedSize(),
                                    oat_writer->GetOatDataOffset(),
                                    oat_writer->GetOatSize());

        bool rodata_ok = oat_writer->WriteRodata(rodata[i]);
        ASSERT_TRUE(rodata_ok);
        elf_writer->EndRoData(rodata[i]);

        OutputStream* text = elf_writer->StartText();
        bool text_ok = oat_writer->WriteCode(text);
        ASSERT_TRUE(text_ok);
        elf_writer->EndText(text);

        if (oat_writer->GetDataBimgRelRoSize() != 0u) {
          OutputStream* data_bimg_rel_ro = elf_writer->StartDataBimgRelRo();
          bool data_bimg_rel_ro_ok = oat_writer->WriteDataBimgRelRo(data_bimg_rel_ro);
          ASSERT_TRUE(data_bimg_rel_ro_ok);
          elf_writer->EndDataBimgRelRo(data_bimg_rel_ro);
        }

        bool header_ok = oat_writer->WriteHeader(elf_writer->GetStream());
        ASSERT_TRUE(header_ok);

        writer->UpdateOatFileHeader(i, oat_writer->GetOatHeader());

        elf_writer->WriteDynamicSection();
        elf_writer->WriteDebugInfo(oat_writer->GetDebugInfo());

        bool success = elf_writer->End();
        ASSERT_TRUE(success);
      }
    }

    bool success_image = writer->Write(kInvalidFd,
                                       image_filenames,
                                       oat_filenames);
    ASSERT_TRUE(success_image);

    for (size_t i = 0, size = oat_filenames.size(); i != size; ++i) {
      const char* oat_filename = oat_filenames[i].c_str();
      std::unique_ptr<File> oat_file(OS::OpenFileReadWrite(oat_filename));
      ASSERT_TRUE(oat_file != nullptr);
      bool success_fixup = ElfWriter::Fixup(oat_file.get(), writer->GetOatDataBegin(i));
      ASSERT_TRUE(success_fixup);
      ASSERT_EQ(oat_file->FlushCloseOrErase(), 0) << "Could not flush and close oat file "
                                                  << oat_filename;
    }
  }
}

inline void ImageTest::Compile(
    ImageHeader::StorageMode storage_mode,
    uint32_t max_image_block_size,
    CompilationHelper& helper,
    const std::string& extra_dex,
    const std::initializer_list<std::string>& image_classes,
    const std::initializer_list<std::string>& image_classes_failing_aot_clinit) {
  for (const std::string& image_class : image_classes_failing_aot_clinit) {
    ASSERT_TRUE(ContainsElement(image_classes, image_class));
  }
  for (const std::string& image_class : image_classes) {
    image_classes_.insert(image_class);
  }
  number_of_threads_ = kIsTargetBuild ? 2U : 16U;
  CreateCompilerDriver();
  // Set inline filter values.
  compiler_options_->SetInlineMaxCodeUnits(CompilerOptions::kDefaultInlineMaxCodeUnits);
  compiler_options_->SetMaxImageBlockSize(max_image_block_size);
  image_classes_.clear();
  if (!extra_dex.empty()) {
    helper.extra_dex_files = OpenTestDexFiles(extra_dex.c_str());
  }
  DoCompile(storage_mode, helper);
  if (image_classes.begin() != image_classes.end()) {
    // Make sure the class got initialized.
    ScopedObjectAccess soa(Thread::Current());
    ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
    for (const std::string& image_class : image_classes) {
      ObjPtr<mirror::Class> klass =
          class_linker->FindSystemClass(Thread::Current(), image_class.c_str());
      EXPECT_TRUE(klass != nullptr);
      EXPECT_TRUE(klass->IsResolved());
      if (ContainsElement(image_classes_failing_aot_clinit, image_class)) {
        EXPECT_FALSE(klass->IsInitialized());
      } else {
        EXPECT_TRUE(klass->IsInitialized());
      }
    }
  }
}

inline void ImageTest::TestWriteRead(ImageHeader::StorageMode storage_mode,
                                     uint32_t max_image_block_size) {
  CompilationHelper helper;
  Compile(storage_mode, max_image_block_size, /*out*/ helper);
  std::vector<uint64_t> image_file_sizes;
  for (ScratchFile& image_file : helper.image_files) {
    std::unique_ptr<File> file(OS::OpenFileForReading(image_file.GetFilename().c_str()));
    ASSERT_TRUE(file.get() != nullptr);
    ImageHeader image_header;
    ASSERT_EQ(file->ReadFully(&image_header, sizeof(image_header)), true);
    ASSERT_TRUE(image_header.IsValid());
    const auto& bitmap_section = image_header.GetImageBitmapSection();
    ASSERT_GE(bitmap_section.Offset(), sizeof(image_header));
    ASSERT_NE(0U, bitmap_section.Size());

    gc::Heap* heap = Runtime::Current()->GetHeap();
    ASSERT_TRUE(heap->HaveContinuousSpaces());
    gc::space::ContinuousSpace* space = heap->GetNonMovingSpace();
    ASSERT_FALSE(space->IsImageSpace());
    ASSERT_TRUE(space != nullptr);
    ASSERT_TRUE(space->IsMallocSpace());
    image_file_sizes.push_back(file->GetLength());
  }

  // Need to delete the compiler since it has worker threads which are attached to runtime.
  compiler_driver_.reset();

  // Tear down old runtime before making a new one, clearing out misc state.

  // Remove the reservation of the memory for use to load the image.
  // Need to do this before we reset the runtime.
  UnreserveImageSpace();

  helper.extra_dex_files.clear();
  runtime_.reset();
  java_lang_dex_file_ = nullptr;

  MemMap::Init();

  RuntimeOptions options;
  options.emplace_back(GetClassPathOption("-Xbootclasspath:", GetLibCoreDexFileNames()), nullptr);
  options.emplace_back(
      GetClassPathOption("-Xbootclasspath-locations:", GetLibCoreDexLocations()), nullptr);
  std::string image("-Ximage:");
  image.append(helper.image_locations[0].GetFilename());
  options.push_back(std::make_pair(image.c_str(), static_cast<void*>(nullptr)));
  // By default the compiler this creates will not include patch information.
  options.push_back(std::make_pair("-Xnorelocate", nullptr));

  if (!Runtime::Create(options, false)) {
    LOG(FATAL) << "Failed to create runtime";
    return;
  }
  runtime_.reset(Runtime::Current());
  // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
  // give it away now and then switch to a more managable ScopedObjectAccess.
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);
  ScopedObjectAccess soa(Thread::Current());
  ASSERT_TRUE(runtime_.get() != nullptr);
  class_linker_ = runtime_->GetClassLinker();

  gc::Heap* heap = Runtime::Current()->GetHeap();
  ASSERT_TRUE(heap->HasBootImageSpace());
  ASSERT_TRUE(heap->GetNonMovingSpace()->IsMallocSpace());

  // We loaded the runtime with an explicit image, so it must exist.
  ASSERT_EQ(heap->GetBootImageSpaces().size(), image_file_sizes.size());
  const HashSet<std::string>& image_classes = compiler_options_->GetImageClasses();
  for (size_t i = 0; i < helper.dex_file_locations.size(); ++i) {
    std::unique_ptr<const DexFile> dex(
        LoadExpectSingleDexFile(helper.dex_file_locations[i].c_str()));
    ASSERT_TRUE(dex != nullptr);
    uint64_t image_file_size = image_file_sizes[i];
    gc::space::ImageSpace* image_space = heap->GetBootImageSpaces()[i];
    ASSERT_TRUE(image_space != nullptr);
    if (storage_mode == ImageHeader::kStorageModeUncompressed) {
      // Uncompressed, image should be smaller than file.
      ASSERT_LE(image_space->GetImageHeader().GetImageSize(), image_file_size);
    } else if (image_file_size > 16 * KB) {
      // Compressed, file should be smaller than image. Not really valid for small images.
      ASSERT_LE(image_file_size, image_space->GetImageHeader().GetImageSize());
      // TODO: Actually validate the blocks, this is hard since the blocks are not copied over for
      // compressed images. Add kPageSize since image_size is rounded up to this.
      ASSERT_GT(image_space->GetImageHeader().GetBlockCount() * max_image_block_size,
                image_space->GetImageHeader().GetImageSize() - kPageSize);
    }

    image_space->VerifyImageAllocations();
    uint8_t* image_begin = image_space->Begin();
    uint8_t* image_end = image_space->End();
    if (i == 0) {
      // This check is only valid for image 0.
      CHECK_EQ(kRequestedImageBase, reinterpret_cast<uintptr_t>(image_begin));
    }
    for (size_t j = 0; j < dex->NumClassDefs(); ++j) {
      const dex::ClassDef& class_def = dex->GetClassDef(j);
      const char* descriptor = dex->GetClassDescriptor(class_def);
      ObjPtr<mirror::Class> klass = class_linker_->FindSystemClass(soa.Self(), descriptor);
      EXPECT_TRUE(klass != nullptr) << descriptor;
      uint8_t* raw_klass = reinterpret_cast<uint8_t*>(klass.Ptr());
      if (image_classes.find(std::string_view(descriptor)) == image_classes.end()) {
        EXPECT_TRUE(raw_klass >= image_end || raw_klass < image_begin) << descriptor;
      } else {
        // Image classes should be located inside the image.
        EXPECT_LT(image_begin, raw_klass) << descriptor;
        EXPECT_LT(raw_klass, image_end) << descriptor;
      }
      EXPECT_TRUE(Monitor::IsValidLockWord(klass->GetLockWord(false)));
    }
  }
}

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_IMAGE_TEST_H_
