// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_PEG_H
#define BITCOIN_PEG_H

#include <list>
#include <functional>

#include "bignum.h"
#include "tinyformat.h"

class CTxDB;
class CPegDB;
class CBlock;
class CBlockIndex;
class CTxIndex;
class CTransaction;
typedef std::map<uint256, std::pair<CTxIndex, CTransaction> > MapPrevTx;

extern int nPegStartHeight;
extern int nPegMaxSupplyIndex;

// functors for messagings
typedef std::function<void(const std::string &)> LoadMsg;

class CPegFractions {
public:
    unsigned int nFlags;
    enum
    {
        PEG_VALUE   = 1,
        PEG_STD     = 2,
        PEG_RATE    = 100,
        PEG_SIZE    = 1200,
        SER_VALUE   = 0,
        SER_ZDELTA  = 1,
        SER_RAW     = 2
    };
    int64_t f[PEG_SIZE];

    CPegFractions();
    CPegFractions(int64_t);
    CPegFractions(const CPegFractions &);

    bool Pack(CDataStream &) const;
    bool Unpack(CDataStream &);

    CPegFractions Std() const;
    CPegFractions Reserve(int supply, int64_t* total);
    CPegFractions Liquidity(int supply, int64_t* total);
    CPegFractions RatioPart(int64_t part, int64_t of_total);

    CPegFractions& operator+=(const CPegFractions& b);
    CPegFractions& operator-=(const CPegFractions& b);

private:
    void ToStd();
    void ToDeltas(int64_t* deltas) const;
    void FromDeltas(const int64_t* deltas);
};

typedef std::map<uint320, CPegFractions> MapPrevFractions;

bool SetBlocksIndexesReadyForPeg(CTxDB & ctxdb,
                                 LoadMsg load_msg);
bool CalculateVotesForPeg(CTxDB & ctxdb,
                          LoadMsg load_msg);
bool CalculateBlockPegVotes(const CBlock & cblock,
                            CBlockIndex* pindex);
bool WriteFractionsForPegTest(int nStartHeight,
                              CTxDB & ctxdb,
                              LoadMsg load_msg);

bool CalculateTransactionFractions(const CTransaction & tx,
                                   const CBlockIndex* pindexBlock,
                                   MapPrevTx & inputs,
                                   MapPrevFractions& finputs,
                                   std::map<uint256, CTxIndex>& mapTestPool,
                                   std::map<uint320, CPegFractions>& mapTestFractionsPool,
                                   bool *ok =nullptr);

#define PegFail(...) PegReport(__VA_ARGS__)
#define PegReportf(...) PegReport(__VA_ARGS__)
bool PegReport(const char* format);

int PegPrintStr(const std::string &str);

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_PEG_LOG_FUNC(n)                                        \
    /*   Log peg error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool PegError(const char* format, TINYFORMAT_VARARGS(n))                     \
    {                                                                         \
        PegPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n"); \
        return false;                                                         \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_PEG_LOG_FUNC)

static inline bool PegError(const char* format)
{
    PegPrintStr(std::string("ERROR: ") + format + "\n");
    return false;
}

#endif
