#ifndef RISK_FREE_H
#define RISK_FREE_H

#include "base_strategy.h"

class RiskFree : public BaseStrategy
{
    Q_OBJECT

    const double threshold;

public:
    RiskFree(double threshold, DepthMarketCollection *pDMC, QObject *parent = nullptr);
    ~RiskFree();

    void onUnderlyingChanged(int underlyingIdx);
    void onOptionChanged(int underlyingIdx, OPTION_TYPE type, int kIdx);

protected:
   // Argitrage strategies
   void findCheapCallOptions(int underlyingIdx);
   void checkCheapCallOptions(int underlyingIdx, int kIdx);
   void findCheapPutOptions(int underlyingIdx);
   void checkCheapPutOptions(int underlyingIdx, int kIdx);

   void findReversedCallOptions(int underlyingIdx, int kIdxToCheck);
   void checkReversedCallOptions(int underlyingIdx, int lowKIdx, int highKIdx);
   void findReversedPutOptions(int underlyingIdx, int kIdxToCheck);
   void checkReversedPutOptions(int underlyingIdx, int lowKIdx, int highKIdx);
};

#endif // RISK_FREE_H
