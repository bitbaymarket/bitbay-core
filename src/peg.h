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

enum
{
    PEG_INTERVAL            = 200,
    PEG_RATE                = 100,
    PEG_SIZE                = 1200,
    PEG_DEST_OUT            = 1,
    PEG_DEST_SELF           = 2,
//    PEG_FROZEN_TIME         = (3600 * 24 * 30),
//    PEG_VFROZEN_TIME        = (3600 * 24 * 30 *4),
    PEG_FROZEN_TIME         = (3600 * 1),
    PEG_VFROZEN_TIME        = (3600 * 1 *4),
    PEG_MAKETX_FREEZE_VALUE = 5590,
    PEG_MAKETX_FEE_INP_OUT  = 5000
};

enum PegTxType {
    PEG_MAKETX_SEND_RESERVE     = 0,
    PEG_MAKETX_SEND_LIQUIDITY   = 1,
    PEG_MAKETX_FREEZE_RESERVE   = 2,
    PEG_MAKETX_FREEZE_LIQUIDITY = 3
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
    CFractions LowPart(int supply, int64_t* total) const;
    CFractions HighPart(int supply, int64_t* total) const;
    CFractions RatioPart(int64_t part, int64_t of_total, int adjust_from);

    CFractions& operator+=(const CFractions& b);
    CFractions& operator-=(const CFractions& b);

    int64_t MoveRatioPartTo(int64_t nPartValue,
                            int adjust_from,
                            CFractions& b);
    
    void ToDeltas(int64_t* deltas) const;
    void FromDeltas(const int64_t* deltas);

    int64_t Low(int supply) const;
    int64_t High(int supply) const;
    int64_t Total() const;

private:
    void ToStd();
};

typedef std::map<uint320, CFractions> MapFractions;

struct FrozenTxOut {
    int64_t nValue;
    std::string sAddress;
    CFractions fractions;
    long nFairWithdrawFromEscrowIndex1;
    long nFairWithdrawFromEscrowIndex2;
};

bool ReadWhitelistInfo();
bool SetBlocksIndexesReadyForPeg(CTxDB & ctxdb,
                                 LoadMsg load_msg);
bool CalculateVotesForPeg(CTxDB & ctxdb,
                          CPegDB& pegdb,
                          LoadMsg load_msg);
bool CalculateBlockPegIndex(CBlockIndex* pindex);
bool CalculateBlockPegVotes(const CBlock & cblock,
                            CBlockIndex* pindex,
                            CPegDB& pegdb);

bool IsPegWhiteListed(const CTransaction & tx, MapPrevTx & inputs);

bool CalculateFractions(CTxDB& txdb,
                        const CTransaction & tx,
                        const CBlockIndex* pindexBlock,
                        MapPrevTx & inputs,
                        MapFractions& finputs,
                        std::map<uint256, CTxIndex>& mapTestPool,
                        MapFractions& mapTestFractionsPool,
                        CFractions& feesFractions,
                        std::vector<int>& vOutputsTypes,
                        std::string& sPegFailCause);

bool CalculateStandardFractions(const CTransaction & tx,
                                int nSupply,
                                unsigned int nTime,
                                MapPrevTx & inputs,
                                MapFractions& finputs,
                                std::map<uint256, CTxIndex>& mapTestPool,
                                MapFractions& mapTestFractionsPool,
                                CFractions& feesFractions,
                                std::vector<int>& vOutputsTypes,
                                std::string& sPegFailCause);

bool CalculateStakingFractions(const CTransaction & tx,
                               const CBlockIndex* pindexBlock,
                               MapPrevTx & inputs,
                               MapFractions& finputs,
                               std::map<uint256, CTxIndex>& mapTestPool,
                               MapFractions& mapTestFractionsPool,
                               const CFractions& feesFractions,
                               int64_t nCalculatedStakeRewardWithoutFees,
                               std::vector<int>& vOutputsTypes,
                               std::string& sPegFailCause);

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

//static inline bool PegError(const char* format)
//{
//    PegPrintStr(std::string("ERROR: ") + format + "\n");
//    return false;
//}

#endif
