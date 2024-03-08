#include "niel_trace.h"

#include <fstream>
#include <mutex>
#include <string>

#include "mirror/object.h"
#include "niel_common.h"

namespace art {

namespace niel {

namespace trace {

std::fstream traceFile;
std::mutex traceFileMutex;
bool traceActive = false;

void InitializeTrace() {
    std::string packageName = getPackageName();

    // jiacheng start
    // if (appOnCommonBlacklist(packageName)) {
    //     LOG(ERROR) << "NIELERROR stopping trace initialization due to blacklisted app"
    //                << " (package name " << packageName << ")";
    //     return;
    // }
    if (!appOnCommonWhitelist()) {
        LOG(ERROR) << "NIELERROR stopping trace initialization due to blacklisted app"
                   << " (package name " << packageName << ")";
        return;
    }
    // jiacheng end

    CHECK(!traceFile.is_open());
    std::string traceFilePath("/data/data/" + packageName + "/tracefile");
    traceFileMutex.lock();
    openFile(traceFilePath, traceFile);
    traceActive = !checkStreamError(traceFile, "opening trace file");
    traceFileMutex.unlock();
}

void TraceGetField(mirror::Object * obj) {
    if (traceActive) {
        traceFileMutex.lock();
        traceFile << "GF\t" << obj << std::endl;
        traceFileMutex.unlock();
    }
}

void TraceGetFieldObject(mirror::Object * obj, mirror::Object * ref) {
    if (traceActive) {
        traceFileMutex.lock();
        traceFile << "GFO\t" << obj << "\t" << ref << std::endl;
        traceFileMutex.unlock();
    }
}

void TraceSetField(mirror::Object * obj) {
    if (traceActive) {
        traceFileMutex.lock();
        traceFile << "SF\t" << obj << std::endl;
        traceFileMutex.unlock();
    }
}

void TraceSetFieldObject(mirror::Object * obj, mirror::Object * ref) {
    if (traceActive) {
        traceFileMutex.lock();
        traceFile << "SFO\t" << obj << "\t" << ref << std::endl;
        traceFileMutex.unlock();
    }
}

}
}
}