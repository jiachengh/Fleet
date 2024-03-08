// jiacheng start
#include <chrono>
#include <atomic>
#include <set>
#include <thread>
#include <random>
#include <fstream>
#include <sstream>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <cstdlib>

#include <errno.h>

#include "base/time_utils.h"
#include "runtime.h"
#include "gc/heap-inl.h"

#include "jiacheng_utils.h"

extern char *__progname;

namespace art {
namespace jiacheng {


uint64_t GenerateID() {
    static std::atomic<uint64_t> id(1);
    return id.fetch_add(1, std::memory_order_relaxed);
}


bool IsWhiteApp() {
    static std::atomic<int> white(0);
    static std::unordered_set<std::string> white_app_set{
        "com.jiacheng.activitylifecycletest",

        "com.twitter.android",
        "com.facebook.katana",
        "com.instagram.android",
        "org.telegram.messenger",
        "jp.naver.line.android",
        
        "com.google.android.youtube",
        "com.ss.android.ugc.aweme",
        "com.spotify.music",
        "tv.twitch.android.app",
        "com.wemesh.android",
        "sg.bigo.live",

        "com.amazon.mShop.android.shopping",
        "com.google.android.apps.maps",
        "com.android.chrome",
        "org.mozilla.firefox",
        "com.linkedin.android",

        "com.rovio.angrybirds",
        "com.king.candycrushsaga",

        "edu.washington.cs.nl35.memorywaster", 
        "edu.washington.cs.nl35.memorywaster1",
        "edu.washington.cs.nl35.memorywaster2",
        "edu.washington.cs.nl35.memorywaster3",
        "edu.washington.cs.nl35.memorywaster4",
        "edu.washington.cs.nl35.memorywaster5",
        "edu.washington.cs.nl35.memorywaster6",
        "edu.washington.cs.nl35.memorywaster7",
        "edu.washington.cs.nl35.memorywaster8",
        "edu.washington.cs.nl35.memorywaster9",
        "edu.washington.cs.nl35.memorywaster10",
        "edu.washington.cs.nl35.memorywaster11",
        "edu.washington.cs.nl35.memorywaster12",
        "edu.washington.cs.nl35.memorywaster13",
        "edu.washington.cs.nl35.memorywaster14",
        "edu.washington.cs.nl35.memorywaster15",
        "edu.washington.cs.nl35.memorywaster16",
        "edu.washington.cs.nl35.memorywaster17",
        "edu.washington.cs.nl35.memorywaster18",
        "edu.washington.cs.nl35.memorywaster19",
        "edu.washington.cs.nl35.memorywaster20",
        "edu.washington.cs.nl35.memorywaster21",
        "edu.washington.cs.nl35.memorywaster22",
        "edu.washington.cs.nl35.memorywaster23",
        "edu.washington.cs.nl35.memorywaster24",
        "edu.washington.cs.nl35.memorywaster25",
        "edu.washington.cs.nl35.memorywaster26",
        "edu.washington.cs.nl35.memorywaster27",
        "edu.washington.cs.nl35.memorywaster28",
        "edu.washington.cs.nl35.memorywaster29",
        "edu.washington.cs.nl35.memorywaster30",

        "edu.cityu.memorywaster01"
    };
    
    // std::string process_name = GetCurrentProcessName();
    // return white_app_set.find(process_name) != white_app_set.end();

    if (white.load(std::memory_order_relaxed) == 1) {
        return true;
    }
    std::string process_name = GetCurrentProcessName();
    if(white_app_set.find(process_name) != white_app_set.end()) {
        white.store(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

void PrintKernel(const std::string& info) {
    const char *c_info = info.c_str();
    syscall(435, c_info, info.size() + 1); // SYS_jiacheng_printk = 435
}

std::string GetCurrentProcessName() {
    // because android fromework would reset argv[0] to <pre-initialized>
    // so we have to modify source code in framework directory conrrespondingly
    return std::string(__progname);
}

bool ColdRange(void* start, size_t size) {
    gc::Heap* heap = Runtime::Current()->GetHeap();
    if (heap->GetDuringGcFlag()) {
        return false;
    }
    madvise(start, size, MADV_COLD_RUNTIME);
    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    return true;
}

bool HotRange(void* start, size_t size) {
    gc::Heap* heap = Runtime::Current()->GetHeap();
    if (heap->GetDuringGcFlag()) {
        return false;
    }
    madvise(start, size, MADV_HOT_RUNTIME);
    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    return true;
}

} // namespace jiacheng
} // namespace art

// jiacheng end
