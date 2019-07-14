// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITBAY_PEGDATA_H
#define BITBAY_PEGDATA_H

#include "bignum.h"

enum
{
    PEG_RATE                = 100,
    PEG_SIZE                = 1200,
    PEG_MAKETX_FREEZE_VALUE = 5590,
    PEG_MAKETX_VFREEZE_VALUE= 5590,
    PEG_MAKETX_FEE_INP_OUT  = 5000,
    PEG_MAKETX_VOTE_VALUE   = 5554,
    PEG_SUBPREMIUM_RATING   = 200,
    PEG_DB_CHECK1           = 1, // testnet: update1 for votes calculation
    PEG_DB_CHECK2           = 2, // testnet: update2 for stake liquidity calculation
    PEG_PRUNE_INTERVAL      = 10000
};

class CPegLevel;
class CFractions;

int64_t RatioPart(int64_t nValue,
                  int64_t nPartValue,
                  int64_t nTotalValue);

class CPegLevel {
public:
    int64_t nCycle          = 0;
    int64_t nCyclePrev      = 0;
    int16_t nSupply         = 0;
    int16_t nSupplyNext     = 0;
    int16_t nSupplyNextNext = 0;
    int16_t nShift          = 0;
    int64_t nShiftLastPart  = 0;
    int64_t nShiftLastTotal = 0;
    int64_t nShiftLiquidity = 0;

    CPegLevel(int cycle,
              int cycle_prev,
              int supply,
              int supply_next,
              int supply_next_next);
    CPegLevel(int cycle,
              int cycle_prev,
              int supply,
              int supply_next,
              int supply_next_next,
              const CFractions & fractions, 
              const CFractions & distortion);
    CPegLevel(std::string);
    
    friend bool operator<(const CPegLevel &a, const CPegLevel &b);
    friend bool operator==(const CPegLevel &a, const CPegLevel &b);
    friend bool operator!=(const CPegLevel &a, const CPegLevel &b);
    
    bool IsValid() const;
    bool HasShift() const { return nShift != 0 || nShiftLastPart != 0; }
    bool Pack(CDataStream &) const;
    bool Unpack(CDataStream &);
    std::string ToString() const;
};

class CFractions {
public:
    uint32_t nFlags;
    uint64_t nLockTime = 0;
    enum
    {
        VALUE       = (1 << 0),
        STD         = (1 << 1),
        NOTARY_F    = (1 << 2),
        NOTARY_L    = (1 << 3),
        NOTARY_V    = (1 << 4)
    };
    enum {
        SER_MASK    = 0xffff,
        SER_VALUE   = (1 << 16),
        SER_ZDELTA  = (1 << 17),
        SER_RAW     = (1 << 18)
    };
    int64_t f[PEG_SIZE];

    CFractions();
    CFractions(int64_t, uint32_t flags);
    CFractions(const CFractions &);

    bool Pack(CDataStream &, unsigned long* len =nullptr) const;
    bool Unpack(CDataStream &);

    CFractions Std() const;
    CFractions Positive(int64_t* total) const;
    CFractions Negative(int64_t* total) const;
    CFractions LowPart(int supply, int64_t* total) const;
    CFractions HighPart(int supply, int64_t* total) const;
    CFractions LowPart(const CPegLevel &, int64_t* total) const;
    CFractions HighPart(const CPegLevel &, int64_t* total) const;
    CFractions MidPart(const CPegLevel &, const CPegLevel &) const;
    CFractions RatioPart(int64_t part) const;

    CFractions& operator+=(const CFractions& b);
    CFractions& operator-=(const CFractions& b);
    CFractions operator+(const CFractions & b) const;
    CFractions operator-(const CFractions & b) const;
    CFractions operator&(const CFractions& b) const;
    CFractions operator-() const;

    int64_t MoveRatioPartTo(int64_t nPartValue,
                            CFractions& b);
    
    void ToDeltas(int64_t* deltas) const;
    void FromDeltas(const int64_t* deltas);

    int64_t Low(int supply) const;
    int64_t High(int supply) const;
    int64_t Low(const CPegLevel &) const;
    int64_t High(const CPegLevel &) const;
    int64_t NChange(const CPegLevel &) const;
    int64_t NChange(int src_supply, int dst_supply) const;
    int64_t Total() const;
    double Distortion(const CFractions& b) const;
    bool IsPositive() const;
    bool IsNegative() const;

private:
    void ToStd();
};

typedef std::map<uint320, CFractions> MapFractions;

#endif
