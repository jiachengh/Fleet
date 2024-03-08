// jiacheng start

#ifndef ART_RUNTIME_JIACHENG_GLOBAL_H_
#define ART_RUNTIME_JIACHENG_GLOBAL_H_

#include "base/globals.h"
#include "jiacheng_debug.h"

namespace art {
namespace jiacheng {

constexpr bool ENABLE_ACCESS_BARRIER = false;
constexpr bool ENABLE_ALLOCATION_BARRIER = false;

constexpr bool ENABLE_APGC = true;
constexpr bool ENABLE_NRO = true;
constexpr bool ENABLE_FYO = true;

constexpr bool ENABLE_BGC = true;

constexpr uint32_t WINDOW_SIZE_HOT_LAUNCH = 5; 
constexpr uint32_t WINDOW_SIZE_BACKGROUND_WS = 10;

constexpr int32_t NEAR_TO_ROOT_THRESHOLD = 2;

constexpr int32_t HOTNESS_LAUNCH = 9;
constexpr int32_t HOTNESS_WORKING_SET = 8;
constexpr int32_t HOTNESS_COLD = -1;
constexpr int32_t HOTNESS_NONE = 0;

const constexpr uint8_t MADV_COLD_RUNTIME = 233;
const constexpr uint8_t MADV_HOT_RUNTIME = 234;

} // namespace jiacheng
} // namespace art

#endif

// jiacheng end