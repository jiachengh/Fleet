#include "niel_bivariate_histogram.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace art {

namespace niel {

namespace inst {

#define BIN_FMT std::defaultfloat << std::setprecision(6)
#define SCALED_NUM_FMT std::fixed << std::setprecision(3)

BivariateHistogram::BivariateHistogram(int numBinsX, double minX, double maxX,
                                       int numBinsY, double minY, double maxY) {
    numBinsX_ = numBinsX;
    minX_ = minX;
    maxX_ = maxX;

    numBinsY_ = numBinsY;
    minY_ = minY;
    maxY_ = maxY;

    bins_ = new long*[numBinsX_ + 2];
    for (int i = 0; i < numBinsX_ + 2; i++) {
        bins_[i] = new long[numBinsY_ + 2];
    }

    Clear();
}

BivariateHistogram::~BivariateHistogram() {
    for (int i = 0; i < numBinsX_ + 2; i++) {
        delete[] bins_[i];
    }
    delete[] bins_;
}

void BivariateHistogram::Add(double valX, double valY) {
    int binNumX = (int)std::floor(((valX - minX_) * numBinsX_) / (maxX_ - minX_));
    int binNumY = (int)std::floor(((valY - minY_) * numBinsY_) / (maxY_ - minY_));

    int indexX = binNumX + 1;
    int indexY = binNumY + 1;

    if (indexX < 0) {
        indexX = 0;
    }
    else if (indexX > numBinsX_ + 1) {
        indexX = numBinsX_ + 1;
    }

    if (indexY < 0) {
        indexY = 0;
    }
    else if (indexY > numBinsY_ + 1) {
        indexY = numBinsY_ + 1;
    }

    bins_[indexX][indexY]++;
}

long BivariateHistogram::TotalCount() {
    long count = 0;
    for (int i = 0; i < numBinsX_ + 2; i++) {
        for (int j = 0; j < numBinsY_ + 2; j++) {
            count += bins_[i][j];
        }
    }
    return count;
}

std::string BivariateHistogram::GetBinLabel(int binNum, int numBins, double min, double max) {
    std::stringstream result;

    double binWidth = (max - min) / numBins;
    double binStart = min + binNum * binWidth;
    double binEnd = min + (binNum + 1) * binWidth;

    result << BIN_FMT << "[";
    if (binStart < min) {
        result << "-inf";
    }
    else {
        result << binStart;
    }
    result << ",";
    if (binEnd > max) {
        result << "inf";
    }
    else {
        result << binEnd;
    }
    result << "):";

    return result.str();
}

int BivariateHistogram::CalcMaxXBinLabelLength() {
    int max = 0;
    for (int i = -1; i < numBinsX_ + 1; i++) {
        int length = GetBinLabel(i, numBinsX_, minX_, maxX_).length();
        if (length > max) {
            max = length;
        }
    }
    return max;
}

int BivariateHistogram::CalcMaxYBinLabelLength() {
    int max = 0;
    for (int i = -1; i < numBinsY_ + 1; i++) {
        int length = GetBinLabel(i, numBinsY_, minY_, maxY_).length();
        if (length > max) {
            max = length;
        }
    }
    return max;
}

std::string BivariateHistogram::StringOfSpaces(int numSpaces) {
    std::stringstream result;
    for (int i = 0; i < numSpaces; i++) {
        result << " ";
    }
    return result.str();
}

std::string BivariateHistogram::Print(bool scaled) {
    long count = BivariateHistogram::TotalCount();
    int maxXBinLabelLength = CalcMaxXBinLabelLength();
    std::stringstream output;

    std::string axisLabel("X\\Y:");
    output << axisLabel;
    output << StringOfSpaces(maxXBinLabelLength - axisLabel.length() + 1);

    for (int k = 0; k < numBinsY_ + 2; k++) {
        int binNumY = k - 1;
        output << GetBinLabel(binNumY, numBinsY_, minY_, maxY_) << " ";
    }
    output << std::endl;

    for (int i = 0; i < numBinsX_ + 2; i++) {
        int binNumX = i - 1;
        std::string label = GetBinLabel(binNumX, numBinsX_, minX_, maxX_);
        output << label;
        output << StringOfSpaces(maxXBinLabelLength - label.length() + 1);

        for (int j = 0; j < numBinsY_ + 2; j++) {
            if (scaled) {
                output << SCALED_NUM_FMT;
                if (count == 0) {
                    output << 0;
                }
                else {
                    output << (double)bins_[i][j] / count;
                }
            }
            else {
                output << bins_[i][j];
            }

            if (j < numBinsY_ + 1) {
                output << "\t";
            }
        }
        if (i < numBinsX_ + 1) {
            output << std::endl;
        }
    }
    return output.str();
}

void BivariateHistogram::Clear() {
    for (int i = 0; i < numBinsX_ + 2; i++) {
        for (int j = 0; j < numBinsY_ + 2; j++) {
            bins_[i][j] = 0;
        }
    }
}

} // namespace inst
} // namespace niel
} // namespace art