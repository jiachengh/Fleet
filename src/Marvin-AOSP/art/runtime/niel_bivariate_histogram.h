#ifndef ART_RUNTIME_NIEL_BIVARIATE_HISTOGRAM_H_
#define ART_RUNTIME_NIEL_BIVARIATE_HISTOGRAM_H_

#include <string>

namespace art {

namespace niel {

namespace inst {

class Histogram;

class BivariateHistogram {
  public:
    BivariateHistogram(int numBinsX, double minX, double maxX,
                       int numBinsY, double minY, double maxY);
    ~BivariateHistogram();
    void Add(double valX, double valY);
    void Clear();
    std::string Print(bool scaled);
    long TotalCount();
  private:
    std::string GetBinLabel(int binNum, int numBins, double min, double max);
    std::string StringOfSpaces(int numSpaces);
    int CalcMaxXBinLabelLength();
    int CalcMaxYBinLabelLength();

    /*
     * Has dimensions (numBinsX_ + 2) x (numBinsY_ + 2).
     *
     * bins_[i][j] holds the count of pairs (x, y) such that x is in bin
     * (i - 1) and y is in bin (j - 1).
     *
     * bins_[numBinsX_ + 1][j] holds the count of pairs (x, y) such that x is
     * above maxX_ and y is in bin (j - 1).
     * 
     * bins_[0][j] holds the count of pairs (x, y) such that x is below minX_
     * and y is in bin bin (j - 1).
     *
     * bins_[i][numBinsY_ + 1] holds the count of pairs (x, y) such that x is
     * in bin (i - 1) and y is above maxY_.
     *
     * bins_[i][0] holds the count of pairs (x, y) such that x is in bin
     * (i - 1) and y is below minY_.
     */
    long ** bins_;

    int numBinsX_;
    double minX_;
    double maxX_;

    int numBinsY_;
    double minY_;
    double maxY_;
};

} // namespace inst
} // namespace niel
} // namespace art

#endif // ART_RUNTIME_NIEL_BIVARIATE_HISTOGRAM_H_