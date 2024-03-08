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

#ifndef ART_RUNTIME_GC_SPACE_IMAGE_SPACE_H_
#define ART_RUNTIME_GC_SPACE_IMAGE_SPACE_H_

#include "gc/accounting/space_bitmap.h"
#include "image.h"
#include "image_space_loading_order.h"
#include "space.h"

namespace art {

template <typename T> class ArrayRef;
class DexFile;
enum class InstructionSet;
class OatFile;

namespace gc {
namespace space {

// An image space is a space backed with a memory mapped image.
class ImageSpace : public MemMapSpace {
 public:
  SpaceType GetType() const override {
    return kSpaceTypeImageSpace;
  }

  // Load boot image spaces from a primary image file for a specified instruction set.
  //
  // On successful return, the loaded spaces are added to boot_image_spaces (which must be
  // empty on entry) and `extra_reservation` is set to the requested reservation located
  // after the end of the last loaded oat file.
  static bool LoadBootImage(
      const std::vector<std::string>& boot_class_path,
      const std::vector<std::string>& boot_class_path_locations,
      const std::string& image_location,
      const InstructionSet image_isa,
      ImageSpaceLoadingOrder order,
      bool relocate,
      bool executable,
      bool is_zygote,
      size_t extra_reservation_size,
      /*out*/std::vector<std::unique_ptr<space::ImageSpace>>* boot_image_spaces,
      /*out*/MemMap* extra_reservation) REQUIRES_SHARED(Locks::mutator_lock_);

  // Try to open an existing app image space.
  static std::unique_ptr<ImageSpace> CreateFromAppImage(const char* image,
                                                        const OatFile* oat_file,
                                                        std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Reads the image header from the specified image location for the
  // instruction set image_isa. Returns null on failure, with
  // reason in error_msg.
  static std::unique_ptr<ImageHeader> ReadImageHeader(const char* image_location,
                                                      InstructionSet image_isa,
                                                      ImageSpaceLoadingOrder order,
                                                      std::string* error_msg);

  // Give access to the OatFile.
  const OatFile* GetOatFile() const;

  // Releases the OatFile from the ImageSpace so it can be transfer to
  // the caller, presumably the OatFileManager.
  std::unique_ptr<const OatFile> ReleaseOatFile();

  void VerifyImageAllocations()
      REQUIRES_SHARED(Locks::mutator_lock_);

  const ImageHeader& GetImageHeader() const {
    return *reinterpret_cast<ImageHeader*>(Begin());
  }

  // Actual filename where image was loaded from.
  // For example: /data/dalvik-cache/arm/system@framework@boot.art
  const std::string GetImageFilename() const {
    return GetName();
  }

  // Symbolic location for image.
  // For example: /system/framework/boot.art
  const std::string GetImageLocation() const {
    return image_location_;
  }

  accounting::ContinuousSpaceBitmap* GetLiveBitmap() const override {
    return live_bitmap_.get();
  }

  accounting::ContinuousSpaceBitmap* GetMarkBitmap() const override {
    // ImageSpaces have the same bitmap for both live and marked. This helps reduce the number of
    // special cases to test against.
    return live_bitmap_.get();
  }

  void Dump(std::ostream& os) const override;

  // Sweeping image spaces is a NOP.
  void Sweep(bool /* swap_bitmaps */, size_t* /* freed_objects */, size_t* /* freed_bytes */) {
  }

  bool CanMoveObjects() const override {
    return false;
  }

  // Returns the filename of the image corresponding to
  // requested image_location, or the filename where a new image
  // should be written if one doesn't exist. Looks for a generated
  // image in the specified location and then in the dalvik-cache.
  //
  // Returns true if an image was found, false otherwise.
  static bool FindImageFilename(const char* image_location,
                                InstructionSet image_isa,
                                std::string* system_location,
                                bool* has_system,
                                std::string* data_location,
                                bool* dalvik_cache_exists,
                                bool* has_data,
                                bool *is_global_cache);

  // Returns the checksums for the boot image and extra boot class path dex files,
  // based on the boot class path, image location and ISA (may differ from the ISA of an
  // initialized Runtime). The boot image and dex files do not need to be loaded in memory.
  static std::string GetBootClassPathChecksums(ArrayRef<const std::string> boot_class_path,
                                               const std::string& image_location,
                                               InstructionSet image_isa,
                                               ImageSpaceLoadingOrder order,
                                               /*out*/std::string* error_msg);

  // Returns the checksums for the boot image and extra boot class path dex files,
  // based on the boot image and boot class path dex files loaded in memory.
  static std::string GetBootClassPathChecksums(const std::vector<ImageSpace*>& image_spaces,
                                               const std::vector<const DexFile*>& boot_class_path);

  // Expand a single image location to multi-image locations based on the dex locations.
  static std::vector<std::string> ExpandMultiImageLocations(
      const std::vector<std::string>& dex_locations,
      const std::string& image_location);

  // Returns true if the dex checksums in the given oat file match the
  // checksums of the original dex files on disk. This is intended to be used
  // to validate the boot image oat file, which may contain dex entries from
  // multiple different (possibly multidex) dex files on disk. Prefer the
  // OatFileAssistant for validating regular app oat files because the
  // OatFileAssistant caches dex checksums that are reused to check both the
  // oat and odex file.
  //
  // This function is exposed for testing purposes.
  static bool ValidateOatFile(const OatFile& oat_file, std::string* error_msg);

  // Return the end of the image which includes non-heap objects such as ArtMethods and ArtFields.
  uint8_t* GetImageEnd() const {
    return Begin() + GetImageHeader().GetImageSize();
  }

  void DumpSections(std::ostream& os) const;

  // De-initialize the image-space by undoing the effects in Init().
  virtual ~ImageSpace();

  void DisablePreResolvedStrings() REQUIRES_SHARED(Locks::mutator_lock_);
  void ReleaseMetadata() REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  // Tries to initialize an ImageSpace from the given image path, returning null on error.
  //
  // If validate_oat_file is false (for /system), do not verify that image's OatFile is up-to-date
  // relative to its DexFile inputs. Otherwise (for /data), validate the inputs and generate the
  // OatFile in /data/dalvik-cache if necessary. If the oat_file is null, it uses the oat file from
  // the image.
  static std::unique_ptr<ImageSpace> Init(const char* image_filename,
                                          const char* image_location,
                                          bool validate_oat_file,
                                          const OatFile* oat_file,
                                          std::string* error_msg)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static Atomic<uint32_t> bitmap_index_;

  std::unique_ptr<accounting::ContinuousSpaceBitmap> live_bitmap_;

  ImageSpace(const std::string& name,
             const char* image_location,
             MemMap&& mem_map,
             std::unique_ptr<accounting::ContinuousSpaceBitmap> live_bitmap,
             uint8_t* end);

  // The OatFile associated with the image during early startup to
  // reserve space contiguous to the image. It is later released to
  // the ClassLinker during it's initialization.
  std::unique_ptr<OatFile> oat_file_;

  // There are times when we need to find the boot image oat file. As
  // we release ownership during startup, keep a non-owned reference.
  const OatFile* oat_file_non_owned_;

  const std::string image_location_;

  friend class Space;

 private:
  // Internal overload that takes ArrayRef<> instead of vector<>.
  static std::vector<std::string> ExpandMultiImageLocations(
      ArrayRef<const std::string> dex_locations,
      const std::string& image_location);

  class BootImageLoader;
  template <typename ReferenceVisitor>
  class ClassTableVisitor;
  class Loader;
  template <typename PatchObjectVisitor>
  class PatchArtFieldVisitor;
  template <PointerSize kPointerSize, typename PatchObjectVisitor, typename PatchCodeVisitor>
  class PatchArtMethodVisitor;
  template <PointerSize kPointerSize, typename HeapVisitor, typename NativeVisitor>
  class PatchObjectVisitor;

  DISALLOW_COPY_AND_ASSIGN(ImageSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_IMAGE_SPACE_H_
