#ifndef ART_RUNTIME_NIEL_HISTOGRAM_H_
#define ART_RUNTIME_NIEL_HISTOGRAM_H_

#include <string>

namespace art {

namespace niel {

namespace inst {

class Histogram {
  public:
    Histogram(int numBins, double min, double max);
    virtual ~Histogram();
    virtual void Add(double num) final;
    virtual void AddMultiple(double num, long count) final;
    virtual long Count() final;
    virtual void Clear() final;
    virtual double GetAverage() final;
    virtual std::string Print(bool scaled, bool separateLines) final;
  protected:
    /*
     * These methods specify how a specific kind of histogram divides its range
     * into buckets.
     */
    virtual int GetBinIndex(double num) = 0;
    virtual double GetBinStart(int binIndex) = 0;
    virtual double GetBinEnd(int binIndex) = 0;

    /*
     * These variables are needed by subclasses to implement the virtual
     * methods above.
     */
    int numBins_;
    double min_;
    double max_;
  private:
    static const std::string NEWLINE_DELIM_;
    static const std::string SPACE_DELIM_;

    /* state that is reset on Clear() */
    long * bins_;
    long belowMin_;
    long aboveMax_;
    double sum_; // used for average
};

/*
 * A normal histogram with evenly-sized bins.
 */
class LinearHistogram : public Histogram {
  public:
    LinearHistogram(int numBins, double min, double max);
  protected:
    int GetBinIndex(double num) override;
    double GetBinStart(int binIndex) override;
    double GetBinEnd(int binIndex) override;
};

/*
 * A histogram whose range is divided using a log-scale.
 */
class LogHistogram : public Histogram {
  public:
    LogHistogram(double base, int minPower, int maxPower);
  protected:
    int GetBinIndex(double num) override;
    double GetBinStart(int binIndex) override;
    double GetBinEnd(int binIndex) override;
  private:
    double base_;
};

} // namespace inst
} // namespace niel
} // namespace art

#endif // ART_RUNTIME_NIEL_HISTOGRAM_H_