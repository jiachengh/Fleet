/*
 * Copyright (C) 2014 The Android Open Source Project
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
#include <deque>

#include "bump_pointer_space-inl.h"
#include "bump_pointer_space.h"
#include "base/dumpable.h"
#include "base/logging.h"
#include "gc/accounting/read_barrier_table.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "thread_list.h"

// jiacheng start
#include "mirror/object-refvisitor-inl.h"
#include "jiacheng_utils.h"
#include <sstream>
// jiacheng end

namespace art {
namespace gc {
namespace space {

// If a region has live objects whose size is less than this percent
// value of the region size, evaculate the region.
static constexpr uint kEvacuateLivePercentThreshold = 75U;

// Whether we protect the unused and cleared regions.
static constexpr bool kProtectClearedRegions = true;

// Wether we poison memory areas occupied by dead objects in unevacuated regions.
static constexpr bool kPoisonDeadObjectsInUnevacuatedRegions = true;

// Special 32-bit value used to poison memory areas occupied by dead
// objects in unevacuated regions. Dereferencing this value is expected
// to trigger a memory protection fault, as it is unlikely that it
// points to a valid, non-protected memory area.
static constexpr uint32_t kPoisonDeadObject = 0xBADDB01D;  // "BADDROID"

// Whether we check a region's live bytes count against the region bitmap.
static constexpr bool kCheckLiveBytesAgainstRegionBitmap = kIsDebugBuild;

MemMap RegionSpace::CreateMemMap(const std::string& name,
                                 size_t capacity,
                                 uint8_t* requested_begin) {
  CHECK_ALIGNED(capacity, kRegionSize);
  std::string error_msg;
  // Ask for the capacity of an additional kRegionSize so that we can align the map by kRegionSize
  // even if we get unaligned base address. This is necessary for the ReadBarrierTable to work.
  MemMap mem_map;
  while (true) {
    mem_map = MemMap::MapAnonymous(name.c_str(),
                                   requested_begin,
                                   capacity + kRegionSize,
                                   PROT_READ | PROT_WRITE,
                                   /*low_4gb=*/ true,
                                   /*reuse=*/ false,
                                   /*reservation=*/ nullptr,
                                   &error_msg);
    if (mem_map.IsValid() || requested_begin == nullptr) {
      break;
    }
    // Retry with no specified request begin.
    requested_begin = nullptr;
  }
  if (!mem_map.IsValid()) {
    LOG(ERROR) << "Failed to allocate pages for alloc space (" << name << ") of size "
        << PrettySize(capacity) << " with message " << error_msg;
    PrintFileToLog("/proc/self/maps", LogSeverity::ERROR);
    MemMap::DumpMaps(LOG_STREAM(ERROR));
    return MemMap::Invalid();
  }
  CHECK_EQ(mem_map.Size(), capacity + kRegionSize);
  CHECK_EQ(mem_map.Begin(), mem_map.BaseBegin());
  CHECK_EQ(mem_map.Size(), mem_map.BaseSize());
  if (IsAlignedParam(mem_map.Begin(), kRegionSize)) {
    // Got an aligned map. Since we requested a map that's kRegionSize larger. Shrink by
    // kRegionSize at the end.
    mem_map.SetSize(capacity);
  } else {
    // Got an unaligned map. Align the both ends.
    mem_map.AlignBy(kRegionSize);
  }
  CHECK_ALIGNED(mem_map.Begin(), kRegionSize);
  CHECK_ALIGNED(mem_map.End(), kRegionSize);
  CHECK_EQ(mem_map.Size(), capacity);
  return mem_map;
}

RegionSpace* RegionSpace::Create(
    const std::string& name, MemMap&& mem_map, bool use_generational_cc) {
  return new RegionSpace(name, std::move(mem_map), use_generational_cc);
}

RegionSpace::RegionSpace(const std::string& name, MemMap&& mem_map, bool use_generational_cc)
    : ContinuousMemMapAllocSpace(name,
                                 std::move(mem_map),
                                 mem_map.Begin(),
                                 mem_map.End(),
                                 mem_map.End(),
                                 kGcRetentionPolicyAlwaysCollect),
      region_lock_("Region lock", kRegionSpaceRegionLock),
      use_generational_cc_(use_generational_cc),
      time_(1U),
      num_regions_(mem_map_.Size() / kRegionSize),
      num_non_free_regions_(0U),
      num_evac_regions_(0U),
      max_peak_num_non_free_regions_(0U),
      non_free_region_index_limit_(0U),
      current_region_(&full_region_),
      // jiacheng start
      current_launch_region_(&full_region_),
      current_hot_region_(&full_region_),
      current_cold_region_(&full_region_),
      // jiachen end
      evac_region_(nullptr),
      cyclic_alloc_region_index_(0U) {
  CHECK_ALIGNED(mem_map_.Size(), kRegionSize);
  CHECK_ALIGNED(mem_map_.Begin(), kRegionSize);
  DCHECK_GT(num_regions_, 0U);
  regions_.reset(new Region[num_regions_]);
  uint8_t* region_addr = mem_map_.Begin();
  for (size_t i = 0; i < num_regions_; ++i, region_addr += kRegionSize) {
    regions_[i].Init(i, region_addr, region_addr + kRegionSize);
  }
  mark_bitmap_.reset(
      accounting::ContinuousSpaceBitmap::Create("region space live bitmap", Begin(), Capacity()));
  // jiacheng start
  foreground_live_bitmap_.reset(
    accounting::ContinuousSpaceBitmap::Create("region space foreground live bitmap", Begin(), Capacity()));
  // jiacheng end
  if (kIsDebugBuild) {
    CHECK_EQ(regions_[0].Begin(), Begin());
    for (size_t i = 0; i < num_regions_; ++i) {
      CHECK(regions_[i].IsFree());
      CHECK_EQ(static_cast<size_t>(regions_[i].End() - regions_[i].Begin()), kRegionSize);
      if (i + 1 < num_regions_) {
        CHECK_EQ(regions_[i].End(), regions_[i + 1].Begin());
      }
    }
    CHECK_EQ(regions_[num_regions_ - 1].End(), Limit());
  }
  DCHECK(!full_region_.IsFree());
  DCHECK(full_region_.IsAllocated());
  size_t ignored;
  DCHECK(full_region_.Alloc(kAlignment, &ignored, nullptr, &ignored) == nullptr);
  // Protect the whole region space from the start.
  Protect();
}

size_t RegionSpace::FromSpaceSize() {
  uint64_t num_regions = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsInFromSpace()) {
      ++num_regions;
    }
  }
  return num_regions * kRegionSize;
}

size_t RegionSpace::UnevacFromSpaceSize() {
  uint64_t num_regions = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsInUnevacFromSpace()) {
      ++num_regions;
    }
  }
  return num_regions * kRegionSize;
}

size_t RegionSpace::ToSpaceSize() {
  uint64_t num_regions = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsInToSpace()) {
      ++num_regions;
    }
  }
  return num_regions * kRegionSize;
}

void RegionSpace::Region::SetAsUnevacFromSpace(bool clear_live_bytes) {
  // Live bytes are only preserved (i.e. not cleared) during sticky-bit CC collections.
  DCHECK(GetUseGenerationalCC() || clear_live_bytes);
  DCHECK(!IsFree() && IsInToSpace());
  type_ = RegionType::kRegionTypeUnevacFromSpace;
  if (IsNewlyAllocated()) {
    // A newly allocated region set as unevac from-space must be
    // a large or large tail region.
    DCHECK(IsLarge() || IsLargeTail()) << static_cast<uint>(state_);
    // Always clear the live bytes of a newly allocated (large or
    // large tail) region.
    clear_live_bytes = true;
    // Clear the "newly allocated" status here, as we do not want the
    // GC to see it when encountering (and processing) references in the
    // from-space.
    //
    // Invariant: There should be no newly-allocated region in the
    // from-space (when the from-space exists, which is between the calls
    // to RegionSpace::SetFromSpace and RegionSpace::ClearFromSpace).
    is_newly_allocated_ = false;
  }
  if (clear_live_bytes) {
    // Reset the live bytes, as we have made a non-evacuation
    // decision (possibly based on the percentage of live bytes).
    live_bytes_ = 0;
  }
}

bool RegionSpace::Region::GetUseGenerationalCC() {
  // We are retrieving the info from Heap, instead of the cached version in
  // RegionSpace, because accessing the Heap from a Region object is easier
  // than accessing the RegionSpace.
  return art::Runtime::Current()->GetHeap()->GetUseGenerationalCC();
}

inline bool RegionSpace::Region::ShouldBeEvacuated(EvacMode evac_mode) {
  // Evacuation mode `kEvacModeNewlyAllocated` is only used during sticky-bit CC collections.
  DCHECK(GetUseGenerationalCC() || (evac_mode != kEvacModeNewlyAllocated));
  DCHECK((IsAllocated() || IsLarge()) && IsInToSpace());
  // The region should be evacuated if:
  // - the evacuation is forced (`evac_mode == kEvacModeForceAll`); or
  // - the region was allocated after the start of the previous GC (newly allocated region); or
  // - the live ratio is below threshold (`kEvacuateLivePercentThreshold`).
  if (UNLIKELY(evac_mode == kEvacModeForceAll)) {
    return true;
  }
  bool result = false;
  if (is_newly_allocated_) {
    // Invariant: newly allocated regions have an undefined live bytes count.
    DCHECK_EQ(live_bytes_, static_cast<size_t>(-1));
    if (IsAllocated()) {
      // We always evacuate newly-allocated non-large regions as we
      // believe they contain many dead objects (a very simple form of
      // the generational hypothesis, even before the Sticky-Bit CC
      // approach).
      //
      // TODO: Verify that assertion by collecting statistics on the
      // number/proportion of live objects in newly allocated regions
      // in RegionSpace::ClearFromSpace.
      //
      // Note that a side effect of evacuating a newly-allocated
      // non-large region is that the "newly allocated" status will
      // later be removed, as its live objects will be copied to an
      // evacuation region, which won't be marked as "newly
      // allocated" (see RegionSpace::AllocateRegion).
      result = true;
    } else {
      DCHECK(IsLarge());
      // We never want to evacuate a large region (and the associated
      // tail regions), except if:
      // - we are forced to do so (see the `kEvacModeForceAll` case
      //   above); or
      // - we know that the (sole) object contained in this region is
      //   dead (see the corresponding logic below, in the
      //   `kEvacModeLivePercentNewlyAllocated` case).
      // For a newly allocated region (i.e. allocated since the
      // previous GC started), we don't have any liveness information
      // (the live bytes count is -1 -- also note this region has been
      // a to-space one between the time of its allocation and now),
      // so we prefer not to evacuate it.
      result = false;
    }
  } else if (evac_mode == kEvacModeLivePercentNewlyAllocated) {
    bool is_live_percent_valid = (live_bytes_ != static_cast<size_t>(-1));
    if (is_live_percent_valid) {
      DCHECK(IsInToSpace());
      DCHECK(!IsLargeTail());
      DCHECK_NE(live_bytes_, static_cast<size_t>(-1));
      DCHECK_LE(live_bytes_, BytesAllocated());
      const size_t bytes_allocated = RoundUp(BytesAllocated(), kRegionSize);
      DCHECK_LE(live_bytes_, bytes_allocated);
      if (IsAllocated()) {
        // Side node: live_percent == 0 does not necessarily mean
        // there's no live objects due to rounding (there may be a
        // few).
        result = (live_bytes_ * 100U < kEvacuateLivePercentThreshold * bytes_allocated);
      } else {
        DCHECK(IsLarge());
        result = (live_bytes_ == 0U);
      }
    } else {
      result = false;
    }
  }
  // jiacheng start
  else if (evac_mode == kEvacModeBackgroundGen) {
    bool foreground_region = hotness_ == jiacheng::HOTNESS_LAUNCH || 
                             hotness_ == jiacheng::HOTNESS_COLD ||
                             hotness_ == jiacheng::HOTNESS_WORKING_SET;
    if (foreground_region) {
      result = false;
    } else {
      return true;
    }
  }
  // jiacheng end
  return result;
}

void RegionSpace::ZeroLiveBytesForLargeObject(mirror::Object* obj) {
  // This method is only used when Generational CC collection is enabled.
  DCHECK(use_generational_cc_);

  // This code uses a logic similar to the one used in RegionSpace::FreeLarge
  // to traverse the regions supporting `obj`.
  // TODO: Refactor.
  DCHECK(IsLargeObject(obj));
  DCHECK_ALIGNED(obj, kRegionSize);
  size_t obj_size = obj->SizeOf<kDefaultVerifyFlags>();
  DCHECK_GT(obj_size, space::RegionSpace::kRegionSize);
  // Size of the memory area allocated for `obj`.
  size_t obj_alloc_size = RoundUp(obj_size, space::RegionSpace::kRegionSize);
  uint8_t* begin_addr = reinterpret_cast<uint8_t*>(obj);
  uint8_t* end_addr = begin_addr + obj_alloc_size;
  DCHECK_ALIGNED(end_addr, kRegionSize);

  // Zero the live bytes of the large region and large tail regions containing the object.
  MutexLock mu(Thread::Current(), region_lock_);
  for (uint8_t* addr = begin_addr; addr < end_addr; addr += kRegionSize) {
    Region* region = RefToRegionLocked(reinterpret_cast<mirror::Object*>(addr));
    if (addr == begin_addr) {
      DCHECK(region->IsLarge());
    } else {
      DCHECK(region->IsLargeTail());
    }
    region->ZeroLiveBytes();
  }
  if (kIsDebugBuild && end_addr < Limit()) {
    // If we aren't at the end of the space, check that the next region is not a large tail.
    Region* following_region = RefToRegionLocked(reinterpret_cast<mirror::Object*>(end_addr));
    DCHECK(!following_region->IsLargeTail());
  }
}

// Determine which regions to evacuate and mark them as
// from-space. Mark the rest as unevacuated from-space.
void RegionSpace::SetFromSpace(accounting::ReadBarrierTable* rb_table,
                               EvacMode evac_mode,
                               bool clear_live_bytes) {
  // Live bytes are only preserved (i.e. not cleared) during sticky-bit CC collections.
  DCHECK(use_generational_cc_ || clear_live_bytes);
  ++time_;
  if (kUseTableLookupReadBarrier) {
    DCHECK(rb_table->IsAllCleared());
    rb_table->SetAll();
  }
  MutexLock mu(Thread::Current(), region_lock_);
  // Counter for the number of expected large tail regions following a large region.
  size_t num_expected_large_tails = 0U;
  // Flag to store whether the previously seen large region has been evacuated.
  // This is used to apply the same evacuation policy to related large tail regions.
  bool prev_large_evacuated = false;
  VerifyNonFreeRegionLimit();
  const size_t iter_limit = kUseTableLookupReadBarrier
      ? num_regions_
      : std::min(num_regions_, non_free_region_index_limit_);
  for (size_t i = 0; i < iter_limit; ++i) {
    Region* r = &regions_[i];
    RegionState state = r->State();
    RegionType type = r->Type();
    if (!r->IsFree()) {
      DCHECK(r->IsInToSpace());
      if (LIKELY(num_expected_large_tails == 0U)) {
        DCHECK((state == RegionState::kRegionStateAllocated ||
                state == RegionState::kRegionStateLarge) &&
               type == RegionType::kRegionTypeToSpace);
        bool should_evacuate = r->ShouldBeEvacuated(evac_mode);
        bool is_newly_allocated = r->IsNewlyAllocated();

        if (should_evacuate) {
          r->SetAsFromSpace();
          DCHECK(r->IsInFromSpace());
        } else {
          r->SetAsUnevacFromSpace(clear_live_bytes);
          DCHECK(r->IsInUnevacFromSpace());
        }

        if (UNLIKELY(state == RegionState::kRegionStateLarge &&
                     type == RegionType::kRegionTypeToSpace)) {
          prev_large_evacuated = should_evacuate;
          // In 2-phase full heap GC, this function is called after marking is
          // done. So, it is possible that some newly allocated large object is
          // marked but its live_bytes is still -1. We need to clear the
          // mark-bit otherwise the live_bytes will not be updated in
          // ConcurrentCopying::ProcessMarkStackRef() and hence will break the
          // logic.
          if (use_generational_cc_ && !should_evacuate && is_newly_allocated) {
            GetMarkBitmap()->Clear(reinterpret_cast<mirror::Object*>(r->Begin()));
          }
          num_expected_large_tails = RoundUp(r->BytesAllocated(), kRegionSize) / kRegionSize - 1;
          DCHECK_GT(num_expected_large_tails, 0U);
        }
      } else {
        DCHECK(state == RegionState::kRegionStateLargeTail &&
               type == RegionType::kRegionTypeToSpace);
        if (prev_large_evacuated) {
          r->SetAsFromSpace();
          DCHECK(r->IsInFromSpace());
        } else {
          r->SetAsUnevacFromSpace(clear_live_bytes);
          DCHECK(r->IsInUnevacFromSpace());
        }
        --num_expected_large_tails;
      }
    } else {
      DCHECK_EQ(num_expected_large_tails, 0U);
      if (kUseTableLookupReadBarrier) {
        // Clear the rb table for to-space regions.
        rb_table->Clear(r->Begin(), r->End());
      }
    }
    // Invariant: There should be no newly-allocated region in the from-space.
    DCHECK(!r->is_newly_allocated_);
  }
  DCHECK_EQ(num_expected_large_tails, 0U);
  current_region_ = &full_region_;
  // jiacheng start
  current_launch_region_ = &full_region_;
  current_hot_region_ = &full_region_;
  current_cold_region_ = &full_region_;
  // jiacheng end
  evac_region_ = &full_region_;
}

static void ZeroAndProtectRegion(uint8_t* begin, uint8_t* end) {
  ZeroAndReleasePages(begin, end - begin);
  if (kProtectClearedRegions) {
    CheckedCall(mprotect, __FUNCTION__, begin, end - begin, PROT_NONE);
  }
}

void RegionSpace::ClearFromSpace(/* out */ uint64_t* cleared_bytes,
                                 /* out */ uint64_t* cleared_objects,
                                 const bool clear_bitmap) {
  DCHECK(cleared_bytes != nullptr);
  DCHECK(cleared_objects != nullptr);
  *cleared_bytes = 0;
  *cleared_objects = 0;
  size_t new_non_free_region_index_limit = 0;
  // We should avoid calling madvise syscalls while holding region_lock_.
  // Therefore, we split the working of this function into 2 loops. The first
  // loop gathers memory ranges that must be madvised. Then we release the lock
  // and perform madvise on the gathered memory ranges. Finally, we reacquire
  // the lock and loop over the regions to clear the from-space regions and make
  // them availabe for allocation.
  std::deque<std::pair<uint8_t*, uint8_t*>> madvise_list;
  // Gather memory ranges that need to be madvised.
  {
    MutexLock mu(Thread::Current(), region_lock_);
    // Lambda expression `expand_madvise_range` adds a region to the "clear block".
    //
    // As we iterate over from-space regions, we maintain a "clear block", composed of
    // adjacent to-be-cleared regions and whose bounds are `clear_block_begin` and
    // `clear_block_end`. When processing a new region which is not adjacent to
    // the clear block (discontinuity in cleared regions), the clear block
    // is added to madvise_list and the clear block is reset (to the most recent
    // to-be-cleared region).
    //
    // This is done in order to combine zeroing and releasing pages to reduce how
    // often madvise is called. This helps reduce contention on the mmap semaphore
    // (see b/62194020).
    uint8_t* clear_block_begin = nullptr;
    uint8_t* clear_block_end = nullptr;
    auto expand_madvise_range = [&madvise_list, &clear_block_begin, &clear_block_end] (Region* r) {
      if (clear_block_end != r->Begin()) {
        if (clear_block_begin != nullptr) {
          DCHECK(clear_block_end != nullptr);
          madvise_list.push_back(std::pair(clear_block_begin, clear_block_end));
        }
        clear_block_begin = r->Begin();
      }
      clear_block_end = r->End();
    };
    for (size_t i = 0; i < std::min(num_regions_, non_free_region_index_limit_); ++i) {
      Region* r = &regions_[i];
      // The following check goes through objects in the region, therefore it
      // must be performed before madvising the region. Therefore, it can't be
      // executed in the following loop.
      if (kCheckLiveBytesAgainstRegionBitmap) {
        CheckLiveBytesAgainstRegionBitmap(r);
      }
      if (r->IsInFromSpace()) {
        expand_madvise_range(r);
      } else if (r->IsInUnevacFromSpace()) {
        // We must skip tails of live large objects.
        if (r->LiveBytes() == 0 && !r->IsLargeTail()) {
          // Special case for 0 live bytes, this means all of the objects in the region are
          // dead and we can to clear it. This is important for large objects since we must
          // not visit dead ones in RegionSpace::Walk because they may contain dangling
          // references to invalid objects. It is also better to clear these regions now
          // instead of at the end of the next GC to save RAM. If we don't clear the regions
          // here, they will be cleared in next GC by the normal live percent evacuation logic.
          expand_madvise_range(r);
          // Also release RAM for large tails.
          while (i + 1 < num_regions_ && regions_[i + 1].IsLargeTail()) {
            expand_madvise_range(&regions_[i + 1]);
            i++;
          }
        }
      }
    }
    // There is a small probability that we may reach here with
    // clear_block_{begin, end} = nullptr. If all the regions allocated since
    // last GC have been for large objects and all of them survive till this GC
    // cycle, then there will be no regions in from-space.
    if (LIKELY(clear_block_begin != nullptr)) {
      DCHECK(clear_block_end != nullptr);
      madvise_list.push_back(std::pair(clear_block_begin, clear_block_end));
    }
  }

  // Madvise the memory ranges.
  for (const auto &iter : madvise_list) {
    ZeroAndProtectRegion(iter.first, iter.second);
    if (clear_bitmap) {
      GetLiveBitmap()->ClearRange(
          reinterpret_cast<mirror::Object*>(iter.first),
          reinterpret_cast<mirror::Object*>(iter.second));
    }
  }
  madvise_list.clear();

  // Iterate over regions again and actually make the from space regions
  // available for allocation.
  MutexLock mu(Thread::Current(), region_lock_);
  VerifyNonFreeRegionLimit();

  // Update max of peak non free region count before reclaiming evacuated regions.
  max_peak_num_non_free_regions_ = std::max(max_peak_num_non_free_regions_,
                                            num_non_free_regions_);

  for (size_t i = 0; i < std::min(num_regions_, non_free_region_index_limit_); ++i) {
    Region* r = &regions_[i];
    if (r->IsInFromSpace()) {
      DCHECK(!r->IsTlab());
      *cleared_bytes += r->BytesAllocated();
      *cleared_objects += r->ObjectsAllocated();
      --num_non_free_regions_;
      r->Clear(/*zero_and_release_pages=*/false);
    } else if (r->IsInUnevacFromSpace()) {
      if (r->LiveBytes() == 0) {
        DCHECK(!r->IsLargeTail());
        *cleared_bytes += r->BytesAllocated();
        *cleared_objects += r->ObjectsAllocated();
        r->Clear(/*zero_and_release_pages=*/false);
        size_t free_regions = 1;
        // Also release RAM for large tails.
        while (i + free_regions < num_regions_ && regions_[i + free_regions].IsLargeTail()) {
          regions_[i + free_regions].Clear(/*zero_and_release_pages=*/false);
          ++free_regions;
        }
        num_non_free_regions_ -= free_regions;
        // When clear_bitmap is true, this clearing of bitmap is taken care in
        // clear_region().
        if (!clear_bitmap) {
          GetLiveBitmap()->ClearRange(
              reinterpret_cast<mirror::Object*>(r->Begin()),
              reinterpret_cast<mirror::Object*>(r->Begin() + free_regions * kRegionSize));
        }
        continue;
      }
      r->SetUnevacFromSpaceAsToSpace();
      if (r->AllAllocatedBytesAreLive()) {
        // Try to optimize the number of ClearRange calls by checking whether the next regions
        // can also be cleared.
        size_t regions_to_clear_bitmap = 1;
        while (i + regions_to_clear_bitmap < num_regions_) {
          Region* const cur = &regions_[i + regions_to_clear_bitmap];
          if (!cur->AllAllocatedBytesAreLive()) {
            DCHECK(!cur->IsLargeTail());
            break;
          }
          CHECK(cur->IsInUnevacFromSpace());
          cur->SetUnevacFromSpaceAsToSpace();
          ++regions_to_clear_bitmap;
        }

        // Optimization (for full CC only): If the live bytes are *all* live
        // in a region then the live-bit information for these objects is
        // superfluous:
        // - We can determine that these objects are all live by using
        //   Region::AllAllocatedBytesAreLive (which just checks whether
        //   `LiveBytes() == static_cast<size_t>(Top() - Begin())`.
        // - We can visit the objects in this region using
        //   RegionSpace::GetNextObject, i.e. without resorting to the
        //   live bits (see RegionSpace::WalkInternal).
        // Therefore, we can clear the bits for these objects in the
        // (live) region space bitmap (and release the corresponding pages).
        //
        // This optimization is incompatible with Generational CC, because:
        // - minor (young-generation) collections need to know which objects
        //   where marked during the previous GC cycle, meaning all mark bitmaps
        //   (this includes the region space bitmap) need to be preserved
        //   between a (minor or major) collection N and a following minor
        //   collection N+1;
        // - at this stage (in the current GC cycle), we cannot determine
        //   whether the next collection will be a minor or a major one;
        // This means that we need to be conservative and always preserve the
        // region space bitmap when using Generational CC.
        // Note that major collections do not require the previous mark bitmaps
        // to be preserved, and as matter of fact they do clear the region space
        // bitmap. But they cannot do so before we know the next GC cycle will
        // be a major one, so this operation happens at the beginning of such a
        // major collection, before marking starts.
        if (!use_generational_cc_) {
          GetLiveBitmap()->ClearRange(
              reinterpret_cast<mirror::Object*>(r->Begin()),
              reinterpret_cast<mirror::Object*>(r->Begin()
                                                + regions_to_clear_bitmap * kRegionSize));
        }
        // Skip over extra regions for which we cleared the bitmaps: we shall not clear them,
        // as they are unevac regions that are live.
        // Subtract one for the for-loop.
        i += regions_to_clear_bitmap - 1;
      } else {
        // TODO: Explain why we do not poison dead objects in region
        // `r` when it has an undefined live bytes count (i.e. when
        // `r->LiveBytes() == static_cast<size_t>(-1)`) with
        // Generational CC.
        if (!use_generational_cc_ || (r->LiveBytes() != static_cast<size_t>(-1))) {
          // Only some allocated bytes are live in this unevac region.
          // This should only happen for an allocated non-large region.
          DCHECK(r->IsAllocated()) << r->State();
          if (kPoisonDeadObjectsInUnevacuatedRegions) {
            PoisonDeadObjectsInUnevacuatedRegion(r);
          }
        }
      }
    }
    // Note r != last_checked_region if r->IsInUnevacFromSpace() was true above.
    Region* last_checked_region = &regions_[i];
    if (!last_checked_region->IsFree()) {
      new_non_free_region_index_limit = std::max(new_non_free_region_index_limit,
                                                 last_checked_region->Idx() + 1);
    }
  }
  // Update non_free_region_index_limit_.
  SetNonFreeRegionLimit(new_non_free_region_index_limit);
  evac_region_ = nullptr;
  num_non_free_regions_ += num_evac_regions_;
  num_evac_regions_ = 0;
}

void RegionSpace::CheckLiveBytesAgainstRegionBitmap(Region* r) {
  if (r->LiveBytes() == static_cast<size_t>(-1)) {
    // Live bytes count is undefined for `r`; nothing to check here.
    return;
  }

  // Functor walking the region space bitmap for the range corresponding
  // to region `r` and calculating the sum of live bytes.
  size_t live_bytes_recount = 0u;
  auto recount_live_bytes =
      [&r, &live_bytes_recount](mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_ALIGNED(obj, kAlignment);
    if (r->IsLarge()) {
      // If `r` is a large region, then it contains at most one
      // object, which must start at the beginning of the
      // region. The live byte count in that case is equal to the
      // allocated regions (large region + large tails regions).
      DCHECK_EQ(reinterpret_cast<uint8_t*>(obj), r->Begin());
      DCHECK_EQ(live_bytes_recount, 0u);
      live_bytes_recount = r->Top() - r->Begin();
    } else {
      DCHECK(r->IsAllocated())
          << "r->State()=" << r->State() << " r->LiveBytes()=" << r->LiveBytes();
      size_t obj_size = obj->SizeOf<kDefaultVerifyFlags>();
      size_t alloc_size = RoundUp(obj_size, space::RegionSpace::kAlignment);
      live_bytes_recount += alloc_size;
    }
  };
  // Visit live objects in `r` and recount the live bytes.
  GetLiveBitmap()->VisitMarkedRange(reinterpret_cast<uintptr_t>(r->Begin()),
                                    reinterpret_cast<uintptr_t>(r->Top()),
                                    recount_live_bytes);
  // Check that this recount matches the region's current live bytes count.
  DCHECK_EQ(live_bytes_recount, r->LiveBytes());
}

// Poison the memory area in range [`begin`, `end`) with value `kPoisonDeadObject`.
static void PoisonUnevacuatedRange(uint8_t* begin, uint8_t* end) {
  static constexpr size_t kPoisonDeadObjectSize = sizeof(kPoisonDeadObject);
  static_assert(IsPowerOfTwo(kPoisonDeadObjectSize) &&
                IsPowerOfTwo(RegionSpace::kAlignment) &&
                (kPoisonDeadObjectSize < RegionSpace::kAlignment),
                "RegionSpace::kAlignment should be a multiple of kPoisonDeadObjectSize"
                " and both should be powers of 2");
  DCHECK_ALIGNED(begin, kPoisonDeadObjectSize);
  DCHECK_ALIGNED(end, kPoisonDeadObjectSize);
  uint32_t* begin_addr = reinterpret_cast<uint32_t*>(begin);
  uint32_t* end_addr = reinterpret_cast<uint32_t*>(end);
  std::fill(begin_addr, end_addr, kPoisonDeadObject);
}

void RegionSpace::PoisonDeadObjectsInUnevacuatedRegion(Region* r) {
  // The live byte count of `r` should be different from -1, as this
  // region should neither be a newly allocated region nor an
  // evacuated region.
  DCHECK_NE(r->LiveBytes(), static_cast<size_t>(-1))
      << "Unexpected live bytes count of -1 in " << Dumpable<Region>(*r);

  // Past-the-end address of the previously visited (live) object (or
  // the beginning of the region, if `maybe_poison` has not run yet).
  uint8_t* prev_obj_end = reinterpret_cast<uint8_t*>(r->Begin());

  // Functor poisoning the space between `obj` and the previously
  // visited (live) object (or the beginng of the region), if any.
  auto maybe_poison = [&prev_obj_end](mirror::Object* obj) REQUIRES(Locks::mutator_lock_) {
    DCHECK_ALIGNED(obj, kAlignment);
    uint8_t* cur_obj_begin = reinterpret_cast<uint8_t*>(obj);
    if (cur_obj_begin != prev_obj_end) {
      // There is a gap (dead object(s)) between the previously
      // visited (live) object (or the beginning of the region) and
      // `obj`; poison that space.
      PoisonUnevacuatedRange(prev_obj_end, cur_obj_begin);
    }
    prev_obj_end = reinterpret_cast<uint8_t*>(GetNextObject(obj));
  };

  // Visit live objects in `r` and poison gaps (dead objects) between them.
  GetLiveBitmap()->VisitMarkedRange(reinterpret_cast<uintptr_t>(r->Begin()),
                                    reinterpret_cast<uintptr_t>(r->Top()),
                                    maybe_poison);
  // Poison memory between the last live object and the end of the region, if any.
  if (prev_obj_end < r->Top()) {
    PoisonUnevacuatedRange(prev_obj_end, r->Top());
  }
}

void RegionSpace::LogFragmentationAllocFailure(std::ostream& os,
                                               size_t /* failed_alloc_bytes */) {
  size_t max_contiguous_allocation = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  if (current_region_->End() - current_region_->Top() > 0) {
    max_contiguous_allocation = current_region_->End() - current_region_->Top();
  }
  if (num_non_free_regions_ * 2 < num_regions_) {
    // We reserve half of the regions for evaluation only. If we
    // occupy more than half the regions, do not report the free
    // regions as available.
    size_t max_contiguous_free_regions = 0;
    size_t num_contiguous_free_regions = 0;
    bool prev_free_region = false;
    for (size_t i = 0; i < num_regions_; ++i) {
      Region* r = &regions_[i];
      if (r->IsFree()) {
        if (!prev_free_region) {
          CHECK_EQ(num_contiguous_free_regions, 0U);
          prev_free_region = true;
        }
        ++num_contiguous_free_regions;
      } else {
        if (prev_free_region) {
          CHECK_NE(num_contiguous_free_regions, 0U);
          max_contiguous_free_regions = std::max(max_contiguous_free_regions,
                                                 num_contiguous_free_regions);
          num_contiguous_free_regions = 0U;
          prev_free_region = false;
        }
      }
    }
    max_contiguous_allocation = std::max(max_contiguous_allocation,
                                         max_contiguous_free_regions * kRegionSize);
  }
  os << "; failed due to fragmentation (largest possible contiguous allocation "
     <<  max_contiguous_allocation << " bytes)";
  // Caller's job to print failed_alloc_bytes.
}

void RegionSpace::Clear() {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (!r->IsFree()) {
      --num_non_free_regions_;
    }
    r->Clear(/*zero_and_release_pages=*/true);
  }
  SetNonFreeRegionLimit(0);
  DCHECK_EQ(num_non_free_regions_, 0u);
  current_region_ = &full_region_;
  // jiacheng start
  current_launch_region_ = &full_region_;
  current_hot_region_ = &full_region_;
  current_cold_region_ = &full_region_;
  // jiacheng end
  evac_region_ = &full_region_;
}

void RegionSpace::Protect() {
  if (kProtectClearedRegions) {
    CheckedCall(mprotect, __FUNCTION__, Begin(), Size(), PROT_NONE);
  }
}

void RegionSpace::Unprotect() {
  if (kProtectClearedRegions) {
    CheckedCall(mprotect, __FUNCTION__, Begin(), Size(), PROT_READ | PROT_WRITE);
  }
}

void RegionSpace::ClampGrowthLimit(size_t new_capacity) {
  MutexLock mu(Thread::Current(), region_lock_);
  CHECK_LE(new_capacity, NonGrowthLimitCapacity());
  size_t new_num_regions = new_capacity / kRegionSize;
  if (non_free_region_index_limit_ > new_num_regions) {
    LOG(WARNING) << "Couldn't clamp region space as there are regions in use beyond growth limit.";
    return;
  }
  num_regions_ = new_num_regions;
  if (kCyclicRegionAllocation && cyclic_alloc_region_index_ >= num_regions_) {
    cyclic_alloc_region_index_ = 0u;
  }
  SetLimit(Begin() + new_capacity);
  if (Size() > new_capacity) {
    SetEnd(Limit());
  }
  GetMarkBitmap()->SetHeapSize(new_capacity);
  GetMemMap()->SetSize(new_capacity);
}

void RegionSpace::Dump(std::ostream& os) const {
  os << GetName() << " "
     << reinterpret_cast<void*>(Begin()) << "-" << reinterpret_cast<void*>(Limit());
}

void RegionSpace::DumpRegionForObject(std::ostream& os, mirror::Object* obj) {
  CHECK(HasAddress(obj));
  MutexLock mu(Thread::Current(), region_lock_);
  RefToRegionUnlocked(obj)->Dump(os);
}

void RegionSpace::DumpRegions(std::ostream& os) {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    regions_[i].Dump(os);
  }
}

void RegionSpace::DumpNonFreeRegions(std::ostream& os) {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* reg = &regions_[i];
    if (!reg->IsFree()) {
      reg->Dump(os);
    }
  }
}

void RegionSpace::RecordAlloc(mirror::Object* ref) {
  CHECK(ref != nullptr);
  Region* r = RefToRegion(ref);
  r->objects_allocated_.fetch_add(1, std::memory_order_relaxed);
}

bool RegionSpace::AllocNewTlab(Thread* self, size_t min_bytes) {
  MutexLock mu(self, region_lock_);
  RevokeThreadLocalBuffersLocked(self);
  // Retain sufficient free regions for full evacuation.

  Region* r = AllocateRegion(/*for_evac=*/ false);
  if (r != nullptr) {
    r->is_a_tlab_ = true;
    r->thread_ = self;
    r->SetTop(r->End());
    self->SetTlab(r->Begin(), r->Begin() + min_bytes, r->End());
    return true;
  }
  return false;
}

size_t RegionSpace::RevokeThreadLocalBuffers(Thread* thread) {
  MutexLock mu(Thread::Current(), region_lock_);
  RevokeThreadLocalBuffersLocked(thread);
  return 0U;
}

void RegionSpace::RevokeThreadLocalBuffersLocked(Thread* thread) {
  uint8_t* tlab_start = thread->GetTlabStart();
  DCHECK_EQ(thread->HasTlab(), tlab_start != nullptr);
  if (tlab_start != nullptr) {
    DCHECK_ALIGNED(tlab_start, kRegionSize);
    Region* r = RefToRegionLocked(reinterpret_cast<mirror::Object*>(tlab_start));
    DCHECK(r->IsAllocated());
    DCHECK_LE(thread->GetThreadLocalBytesAllocated(), kRegionSize);
    r->RecordThreadLocalAllocations(thread->GetThreadLocalObjectsAllocated(),
                                    thread->GetThreadLocalBytesAllocated());
    r->is_a_tlab_ = false;
    r->thread_ = nullptr;
  }
  thread->SetTlab(nullptr, nullptr, nullptr);
}

size_t RegionSpace::RevokeAllThreadLocalBuffers() {
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::runtime_shutdown_lock_);
  MutexLock mu2(self, *Locks::thread_list_lock_);
  std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
  for (Thread* thread : thread_list) {
    RevokeThreadLocalBuffers(thread);
  }
  return 0U;
}

void RegionSpace::AssertThreadLocalBuffersAreRevoked(Thread* thread) {
  if (kIsDebugBuild) {
    DCHECK(!thread->HasTlab());
  }
}

void RegionSpace::AssertAllThreadLocalBuffersAreRevoked() {
  if (kIsDebugBuild) {
    Thread* self = Thread::Current();
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    MutexLock mu2(self, *Locks::thread_list_lock_);
    std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
    for (Thread* thread : thread_list) {
      AssertThreadLocalBuffersAreRevoked(thread);
    }
  }
}

void RegionSpace::Region::Dump(std::ostream& os) const {
  os << "Region[" << idx_ << "]="
     << reinterpret_cast<void*>(begin_)
     << "-" << reinterpret_cast<void*>(Top())
     << "-" << reinterpret_cast<void*>(end_)
     << " state=" << state_
     << " type=" << type_
     << " objects_allocated=" << objects_allocated_
     << " alloc_time=" << alloc_time_
     << " live_bytes=" << live_bytes_;

  if (live_bytes_ != static_cast<size_t>(-1)) {
    os << " ratio over allocated bytes="
       << (static_cast<float>(live_bytes_) / RoundUp(BytesAllocated(), kRegionSize));
    uint64_t longest_consecutive_free_bytes = GetLongestConsecutiveFreeBytes();
    os << " longest_consecutive_free_bytes=" << longest_consecutive_free_bytes
       << " (" << PrettySize(longest_consecutive_free_bytes) << ")";
  }

  os << " is_newly_allocated=" << std::boolalpha << is_newly_allocated_ << std::noboolalpha
     << " is_a_tlab=" << std::boolalpha << is_a_tlab_ << std::noboolalpha
     << " thread=" << thread_ << '\n';
}

uint64_t RegionSpace::Region::GetLongestConsecutiveFreeBytes() const {
  if (IsFree()) {
    return kRegionSize;
  }
  if (IsLarge() || IsLargeTail()) {
    return 0u;
  }
  uintptr_t max_gap = 0u;
  uintptr_t prev_object_end = reinterpret_cast<uintptr_t>(Begin());
  // Iterate through all live objects and find the largest free gap.
  auto visitor = [&max_gap, &prev_object_end](mirror::Object* obj)
    REQUIRES_SHARED(Locks::mutator_lock_) {
    uintptr_t current = reinterpret_cast<uintptr_t>(obj);
    uintptr_t diff = current - prev_object_end;
    max_gap = std::max(diff, max_gap);
    uintptr_t object_end = reinterpret_cast<uintptr_t>(obj) + obj->SizeOf();
    prev_object_end = RoundUp(object_end, kAlignment);
  };
  space::RegionSpace* region_space = art::Runtime::Current()->GetHeap()->GetRegionSpace();
  region_space->WalkNonLargeRegion(visitor, this);
  return static_cast<uint64_t>(max_gap);
}


size_t RegionSpace::AllocationSizeNonvirtual(mirror::Object* obj, size_t* usable_size) {
  size_t num_bytes = obj->SizeOf();
  if (usable_size != nullptr) {
    if (LIKELY(num_bytes <= kRegionSize)) {
      DCHECK(RefToRegion(obj)->IsAllocated());
      *usable_size = RoundUp(num_bytes, kAlignment);
    } else {
      DCHECK(RefToRegion(obj)->IsLarge());
      *usable_size = RoundUp(num_bytes, kRegionSize);
    }
  }
  return num_bytes;
}

void RegionSpace::Region::Clear(bool zero_and_release_pages) {
  top_.store(begin_, std::memory_order_relaxed);
  state_ = RegionState::kRegionStateFree;
  type_ = RegionType::kRegionTypeNone;
  objects_allocated_.store(0, std::memory_order_relaxed);
  alloc_time_ = 0;
  live_bytes_ = static_cast<size_t>(-1);
  if (zero_and_release_pages) {
    ZeroAndProtectRegion(begin_, end_);
  }
  is_newly_allocated_ = false;
  is_a_tlab_ = false;
  thread_ = nullptr;

  // jiacheng start
  hotness_ = 0;
  // jiacheng end
}

RegionSpace::Region* RegionSpace::AllocateRegion(bool for_evac) {
  if (!for_evac && (num_non_free_regions_ + 1) * 2 > num_regions_) {
    return nullptr;
  }
  for (size_t i = 0; i < num_regions_; ++i) {
    // When using the cyclic region allocation strategy, try to
    // allocate a region starting from the last cyclic allocated
    // region marker. Otherwise, try to allocate a region starting
    // from the beginning of the region space.
    size_t region_index = kCyclicRegionAllocation
        ? ((cyclic_alloc_region_index_ + i) % num_regions_)
        : i;
    Region* r = &regions_[region_index];
    if (r->IsFree()) {
      r->Unfree(this, time_);
      if (use_generational_cc_) {
        // TODO: Add an explanation for this assertion.
        DCHECK(!for_evac || !r->is_newly_allocated_);
      }
      if (for_evac) {
        ++num_evac_regions_;
        // Evac doesn't count as newly allocated.
      } else {
        r->SetNewlyAllocated();
        ++num_non_free_regions_;
      }
      if (kCyclicRegionAllocation) {
        // Move the cyclic allocation region marker to the region
        // following the one that was just allocated.
        cyclic_alloc_region_index_ = (region_index + 1) % num_regions_;
      }
      return r;
    }
  }
  return nullptr;
}

void RegionSpace::Region::MarkAsAllocated(RegionSpace* region_space, uint32_t alloc_time) {
  DCHECK(IsFree());
  alloc_time_ = alloc_time;
  region_space->AdjustNonFreeRegionLimit(idx_);
  type_ = RegionType::kRegionTypeToSpace;
  if (kProtectClearedRegions) {
    CheckedCall(mprotect, __FUNCTION__, Begin(), kRegionSize, PROT_READ | PROT_WRITE);
  }
}

void RegionSpace::Region::Unfree(RegionSpace* region_space, uint32_t alloc_time) {
  MarkAsAllocated(region_space, alloc_time);
  state_ = RegionState::kRegionStateAllocated;
}

void RegionSpace::Region::UnfreeLarge(RegionSpace* region_space, uint32_t alloc_time) {
  MarkAsAllocated(region_space, alloc_time);
  state_ = RegionState::kRegionStateLarge;
}

void RegionSpace::Region::UnfreeLargeTail(RegionSpace* region_space, uint32_t alloc_time) {
  MarkAsAllocated(region_space, alloc_time);
  state_ = RegionState::kRegionStateLargeTail;
}

// jiacheng start

void RegionSpace::Region::Debug() {
  LOG(INFO) << "jiacheng Region::Debug()"
            << " idx_= " <<  idx_
            << " live_bytes_= " <<  live_bytes_
            << " thread_= " <<  thread_
            << " objects_allocated_= " <<  objects_allocated_.load()
            << " alloc_time_= " <<  alloc_time_
            << " is_newly_allocated_= " <<  is_newly_allocated_
            << " is_a_tlab_= " <<  is_a_tlab_
            << " state_= " <<  state_
            << " type_= " <<  type_
            << " hotness_= " <<  hotness_
            ;
}

void RegionSpace::Debug() {
  uint32_t new_region_num = 0;
  uint32_t launch_region_num = 0;
  uint32_t working_set_region_num = 0;
  uint32_t cold_region_num = 0;
  uint32_t none_region_num = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < std::min(num_regions_, non_free_region_index_limit_); ++i) {
    Region* r = &regions_[i];
    if (r->Type() == RegionType::kRegionTypeNone) {
      continue;
    }
    int32_t hotness = r->GetHotness();
    if (r->IsNewlyAllocated()) {
      ++new_region_num;
    } else if (hotness == jiacheng::HOTNESS_LAUNCH) {
      ++launch_region_num;
    } else if (hotness == jiacheng::HOTNESS_WORKING_SET) {
      ++working_set_region_num;
    } else if (hotness == jiacheng::HOTNESS_COLD) {
      ++cold_region_num;
    } else {
      ++none_region_num;
    }
  }
  LOG(INFO) << "jiacheng RegionSpace::Debug()"
            << " Begin()= " << reinterpret_cast<void*>(Begin())
            << " End()= " << reinterpret_cast<void*>(End())
            << " launch_region_num= " << launch_region_num
            << " working_set_region_num= " << working_set_region_num
            << " cold_region_num= " << cold_region_num
            << " none_region_num= " << none_region_num
            << " new_region_num= " << new_region_num
            ;
}


uint64_t RegionSpace::Madvise() {
  uint64_t swap_out_size = 0;
  uint8_t* begin, * end;
  size_t length;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < std::min(num_regions_, non_free_region_index_limit_); ++i) {
    Region* r = &regions_[i];
    if (r->Type() != RegionType::kRegionTypeNone && !r->IsNewlyAllocated()) {
      if (r->GetHotness() == jiacheng::HOTNESS_COLD) {
        begin = r->Begin();
        // end = r->Top();
        end = r->End();
        length = std::distance(begin, end);
        // Change the permission of the region into READ ONLY
        // So as to we can know the swap in of the region through handling page fault
        // if (jiacheng::ColdRange(begin, length) && 
        //     mprotect(begin, length, PROT_READ) == 0) {
        
        if (jiacheng::ColdRange(begin, length)) {
          swap_out_size += length;
        }
      } else if (r->GetHotness() == jiacheng::HOTNESS_LAUNCH) {
        jiacheng::HotRange(begin, length);
      }
    }
  }
  LOG(INFO) << "jiacheng RegionSpace::Madvise()"
            << " size(mb)= " << float(swap_out_size)/MB;
  return swap_out_size;
}

bool RegionSpace::HandleFault(mirror::Object* obj) {
  CHECK(Contains(obj));
  bool result = false;
  Region* r = RefToRegionUnlocked(obj);
  if (r->GetHotness() < 0) {
    uint8_t* begin = r->Begin();
    // uint8_t* end = r->Top();
    uint8_t* end = r->End();
    size_t length = std::distance(begin, end);
    if (mprotect(begin, length, PROT_READ | PROT_WRITE) == 0) {
      r->SetHotness(jiacheng::HOTNESS_NONE);
      result = true;
      LOG(INFO) << " jiacheng RegionSpace::HandleFault()" 
                << " idx_= " << r->Idx()
                << " length= " << length
                << " obj= " << obj
                ;
    }
  }
  return result;
}


mirror::Object* RegionSpace::AllocLaunch(size_t num_bytes,
                                         /* out */ size_t* bytes_allocated,
                                         /* out */ size_t* usable_size,
                                         /* out */ size_t* bytes_tl_bulk_allocated) {
  DCHECK_ALIGNED(num_bytes, kAlignment);
  mirror::Object* obj = nullptr;
  if (LIKELY(num_bytes <= kRegionSize)) {
    obj = current_launch_region_->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    MutexLock mu(Thread::Current(), region_lock_);
    obj = current_launch_region_->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    Region* r = AllocateRegion(/* for_evac */ true);
    if (LIKELY(r != nullptr)) {
      obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
      CHECK(obj != nullptr);
      r->SetHotness(jiacheng::HOTNESS_LAUNCH);
      current_launch_region_ = r;
      return obj;
    }
  } else {
    // If object is large, just set as normal object
    // CHECK(false);
    return nullptr;
  }
  return obj;
}

mirror::Object* RegionSpace::AllocLargeLaunch(size_t num_bytes,
                                              /* out */ size_t* bytes_allocated,
                                              /* out */ size_t* usable_size,
                                              /* out */ size_t* bytes_tl_bulk_allocated) {
  (void)num_bytes;
  (void)bytes_allocated;
  (void)usable_size;
  (void)bytes_tl_bulk_allocated;
  CHECK(false) << "RegionSpace::AllocLargeLaunch()";
  return nullptr;
}


mirror::Object* RegionSpace::AllocWorkingSet(size_t num_bytes,
                          /* out */ size_t* bytes_allocated,
                          /* out */ size_t* usable_size,
                          /* out */ size_t* bytes_tl_bulk_allocated) {
  DCHECK_ALIGNED(num_bytes, kAlignment);
  mirror::Object* obj = nullptr;
  if (LIKELY(num_bytes <= kRegionSize)) {
    obj = current_hot_region_->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    MutexLock mu(Thread::Current(), region_lock_);
    obj = current_hot_region_->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    Region* r = AllocateRegion(/* for_evac */ true);
    if (LIKELY(r != nullptr)) {
      obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
      CHECK(obj != nullptr);
      r->SetHotness(jiacheng::HOTNESS_WORKING_SET);
      current_hot_region_ = r;
      return obj;
    }
  } else {
    // If object is large, just set as normal object
    // CHECK(false);
    return nullptr;
  }
  return obj;
}

mirror::Object* RegionSpace::AllocLargeWorkingSet(size_t num_bytes,
                                /* out */ size_t* bytes_allocated,
                                /* out */ size_t* usable_size,
                                /* out */ size_t* bytes_tl_bulk_allocated) {
  (void)num_bytes;
  (void)bytes_allocated;
  (void)usable_size;
  (void)bytes_tl_bulk_allocated;
  CHECK(false) << "RegionSpace::AllocLargeWorkingSet()";
  return nullptr;
}


mirror::Object* RegionSpace::AllocCold(size_t num_bytes,
                                       /* out */ size_t* bytes_allocated,
                                       /* out */ size_t* usable_size,
                                       /* out */ size_t* bytes_tl_bulk_allocated) {
  DCHECK_ALIGNED(num_bytes, kAlignment);
  mirror::Object* obj = nullptr;
  if (LIKELY(num_bytes <= kRegionSize)) {
    obj = current_cold_region_->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    MutexLock mu(Thread::Current(), region_lock_);
    obj = current_cold_region_->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    Region* r = AllocateRegion(/* for_evac */ true);
    if (LIKELY(r != nullptr)) {
      obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
      CHECK(obj != nullptr);
      r->SetHotness(jiacheng::HOTNESS_COLD);
      current_cold_region_ = r;
      return obj;
    }
  } else {
    // If object is large, just set as normal object
    // CHECK(false);
    return nullptr;
  }
  return obj;
}


void RegionSpace::ResetHotness() {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < std::min(num_regions_, non_free_region_index_limit_); ++i) {
    Region* r = &regions_[i];
    if (r->Type() != RegionType::kRegionTypeNone) {
      r->SetHotness(jiacheng::HOTNESS_NONE);
    }
  }

}


void RegionSpace::CopyForegroundToMarked() {
  mark_bitmap_->CopyFrom(foreground_live_bitmap_.get());
}

void RegionSpace::CopyMarkedToForeground() {
  foreground_live_bitmap_->CopyFrom(mark_bitmap_.get());
}


// jiacheng end

}  // namespace space
}  // namespace gc
}  // namespace art
