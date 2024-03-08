#include "niel_histogram.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "base/logging.h"

namespace art {

namespace niel {

namespace inst {

#define BIN_FMT std::defaultfloat << std::setprecision(6)
#define SCALED_NUM_FMT std::fixed << std::setprecision(6)

const std::string Histogram::NEWLINE_DELIM_("\n");
const std::string Histogram::SPACE_DELIM_(" ");

Histogram::Histogram(int numBins, double min, double max) {
    bins_ = new long[numBins];
    belowMin_ = 0;
    aboveMax_ = 0;
    min_ = min;
    max_ = max;
    numBins_ = numBins;
    Clear();
}

Histogram::~Histogram() {
    delete bins_;
}


void Histogram::Add(double num) {
    AddMultiple(num, 1);
}
void Histogram::AddMultiple(double num, long count) {
    int binIndex = GetBinIndex(num);
    if (binIndex < 0) {
        belowMin_ += count;
    }
    else if (binIndex >= numBins_) {
        aboveMax_ += count;
    }
    else {
        bins_[binIndex] += count;
    }

    sum_ += (num * count);
}

void Histogram::Clear() {
    for (int i = 0; i < numBins_; i++) {
        bins_[i] = 0;
    }
    belowMin_ = 0;
    aboveMax_ = 0;
    sum_ = 0;
}

long Histogram::Count() {
    long count = 0;
    for (int i = 0; i < numBins_; i++) {
        count += bins_[i];
    }
    count += belowMin_;
    count += aboveMax_;
    return count;
}

double Histogram::GetAverage() {
    return sum_ / Count();
}

std::string Histogram::Print(bool scaled, bool separateLines) {
    long count = Count();

    std::string delim = (separateLines ? NEWLINE_DELIM_ : SPACE_DELIM_);

    std::stringstream output;

    output << BIN_FMT << "[-inf," << min_ << "): ";
    if (scaled) {
        output << SCALED_NUM_FMT;
        if (count == 0) {
            output << 0;
        }
        else {
            output << (double)belowMin_ / count;
        }
    }
    else {
        output << belowMin_;
    }
    output << delim;

    for (int i = 0; i < numBins_; i++) {
        double binStart = GetBinStart(i);
        double binEnd = GetBinEnd(i);
        output << BIN_FMT << "[" << binStart << "," << binEnd << "): ";
        if (scaled) {
            output << SCALED_NUM_FMT;
            if (count == 0) {
                output << 0;
            }
            else {
                output << (double)bins_[i] / count;
            }
        }
        else {
            output << bins_[i];
        }
        output << delim;
    }

    output << BIN_FMT << "[" << max_ << ",inf): ";
    if (scaled) {
        output << SCALED_NUM_FMT;
        if (count == 0) {
            output << 0;
        }
        else {
            output << (double)aboveMax_ / count;
        }
    }
    else {
        output << aboveMax_;
    }

    return output.str();
}

LinearHistogram::LinearHistogram(int numBins, double min, double max)
    : Histogram(numBins, min, max) { }

int LinearHistogram::GetBinIndex(double num) {
    return (int)std::floor(((num - min_) * numBins_) / (max_ - min_));
}

double LinearHistogram::GetBinStart(int binIndex) {
    double binWidth = (max_ - min_) / numBins_;
    double binStart = min_ + binIndex * binWidth;
    return binStart;
}

double LinearHistogram::GetBinEnd(int binIndex) {
    double binWidth = (max_ - min_) / numBins_;
    double binEnd = min_ + (binIndex + 1) * binWidth;
    return binEnd;
}

LogHistogram::LogHistogram(double base, int minPower, int maxPower)
    : Histogram((maxPower - minPower), std::pow(base, minPower), std::pow(base, maxPower)),
      base_(base)
    {
        CHECK(base > 1);
    }

int LogHistogram::GetBinIndex(double num) {
    return std::floor(std::log(num / min_) / std::log(base_));
}

double LogHistogram::GetBinStart(int binIndex) {
    return min_ * std::pow(base_, binIndex);
}

double LogHistogram::GetBinEnd(int binIndex) {
    return min_ * std::pow(base_, binIndex + 1);
}

} // namespace inst
} // namespace niel
} // namespace art