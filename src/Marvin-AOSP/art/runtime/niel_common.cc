#include "niel_common.h"

// jiacheng start
extern char *__progname;
// jiacheng end

namespace art {

namespace niel {

void openFile(const std::string & path, std::fstream & stream) {
    stream.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!stream) {
        stream.close();
        stream.open(path, std::ios::binary | std::ios::out);
        stream.close();
        stream.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    }
}

void openFileAppend(const std::string & path, std::fstream & stream) {
    stream.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::ate);
}

bool checkStreamError(const std::ios & stream, const std::string & msg) {
    bool error = !stream;
    if (error) {
        LOG(ERROR) << "NIELERROR stream error: " << msg << " (" << stream.good() << " "
                   << stream.eof() << " " << stream.fail() << " " << stream.bad() << ")";
    }
    return error;
}

std::string getPackageName() {
    std::ifstream cmdlineFile("/proc/self/cmdline");
    std::string cmdline;
    getline(cmdlineFile, cmdline);
    cmdlineFile.close();
    return cmdline.substr(0, cmdline.find((char)0));
}

bool appOnCommonBlacklist(const std::string & packageName) {
    std::set<std::string> blacklist;
    blacklist.insert("droid.launcher3");

    for (auto it = blacklist.begin(); it != blacklist.end(); it++) {
        if (packageName.find(*it) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// jiacheng start
// bool appOnCommonWhitelist(const std::string& packageName) {
//     static const std::set<std::string> whitelist{
//             "com.twitter.android", 
//             "com.facebook.katana", 
//             "com.google.android.youtube", 
//             "com.zhiliaoapp.musically",
//             "com.amazon.mShop.android.shopping",
//             "com.google.android.apps.maps",
//             "com.android.chrome",
//             "org.mozilla.firefox",
//             "com.rovio.angrybirds",
//             "com.king.candycrushsaga",

//             "com.taobao.taobao", 
//             "com.jiacheng.activitylifecycletest",

//             "edu.washington.cs.nl35.memorywaster", 
//             "edu.washington.cs.nl35.memorywaster1",
//             "edu.washington.cs.nl35.memorywaster2",
//             "edu.washington.cs.nl35.memorywaster3",
//             "edu.washington.cs.nl35.memorywaster4",
//             "edu.washington.cs.nl35.memorywaster5",
//             "edu.washington.cs.nl35.memorywaster6",
//             "edu.washington.cs.nl35.memorywaster7",
//             "edu.washington.cs.nl35.memorywaster8",
//             "edu.washington.cs.nl35.memorywaster9",
//             "edu.washington.cs.nl35.memorywaster10",
//             "edu.washington.cs.nl35.memorywaster11",
//             "edu.washington.cs.nl35.memorywaster12",
//             "edu.washington.cs.nl35.memorywaster13",
//             "edu.washington.cs.nl35.memorywaster14",
//             "edu.washington.cs.nl35.memorywaster15",
//             "edu.washington.cs.nl35.memorywaster16",
//             "edu.washington.cs.nl35.memorywaster17",
//             "edu.washington.cs.nl35.memorywaster18",
//             "edu.washington.cs.nl35.memorywaster19",
//             "edu.washington.cs.nl35.memorywaster20",
//             "edu.washington.cs.nl35.memorywaster21",
//             "edu.washington.cs.nl35.memorywaster22",
//             "edu.washington.cs.nl35.memorywaster23",
//             "edu.washington.cs.nl35.memorywaster24",
//             "edu.washington.cs.nl35.memorywaster25",
//             "edu.washington.cs.nl35.memorywaster26",
//             "edu.washington.cs.nl35.memorywaster27",
//             "edu.washington.cs.nl35.memorywaster28",
//             "edu.washington.cs.nl35.memorywaster29",
//             "edu.washington.cs.nl35.memorywaster30"
//     };
//     // Runtime* runtime = Runtime::Current();
//     // if ((!runtime->GetStartupCompleted()) || runtime->IsSystemServer()) {
//     //     return false;
//     // }

//     // if (whitelist.find(packageName) != whitelist.end()) {
//     //     return true;
//     // } else if ("" != packageName && "zygote" != packageName && "zygote64" != packageName) {
//     //     return false;
//     // } else {
//     //     return false;
//     // }

//     if (whitelist.find(packageName) != whitelist.end()) {
//         return true;
//     } else {
//         return false;
//     }
// }


std::string GetCurrentProcessName() {
    // because android fromework would reset argv[0] to <pre-initialized>
    // so we have to modify source code in framework directory conrrespondingly
    return std::string(__progname);
}


bool appOnCommonWhitelist() {
    static std::atomic<int> white(0);
    static std::set<std::string> white_app_set{
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
        "edu.washington.cs.nl35.memorywaster30"
    };
    
    if (white.load(std::memory_order_relaxed) == 0) {
        std::string process_name = GetCurrentProcessName();
        if (white_app_set.find(process_name) != white_app_set.end()) {
            white.store(1, std::memory_order_relaxed);
        } else if ("" != process_name && "zygote" != process_name && "zygote64" != process_name) {
            white.store(-1, std::memory_order_relaxed);
        }
    }
    return white.load(std::memory_order_relaxed) == 1;
}
// jiacheng end


}
}