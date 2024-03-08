// jiacheng start
#ifndef ART_RUNTIME_JIACHENG_UTILS_H_
#define ART_RUNTIME_JIACHENG_UTILS_H_

#include "jiacheng_global.h"
#include <string>

namespace art {
namespace jiacheng {

uint64_t GenerateID();

std::string GetCurrentProcessName();

bool IsWhiteApp();

void PrintKernel(const std::string& info);

bool ColdRange(void* start, size_t size);

bool HotRange(void* start, size_t size);

} // namespace jiacheng
} // namespace art


#endif

// jiacheng end