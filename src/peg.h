// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_PEG_H
#define BITCOIN_PEG_H

#include <list>
#include <functional>

#include "bignum.h"

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
        PEG_ALL     = 2,
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

private:
    void ToDeltas(int64_t* deltas) const;
    void FromDeltas(const int64_t* deltas);
};

typedef std::map<std::pair<uint256,unsigned int>, CPegFractions> MapPrevFractions;

bool SetBlocksIndexesReadyForPeg(CTxDB & ctxdb,
                                 LoadMsg load_msg);
bool CalculateVotesForPeg(CTxDB & ctxdb,
                          LoadMsg load_msg);
bool CalculateBlockPegVotes(const CBlock & cblock,
                            CBlockIndex* pindex);
bool WriteFractionsForPegTest(int nStartHeight,
                              CTxDB & ctxdb,
                              LoadMsg load_msg);
bool WriteBlockPegFractions(const CBlock & block,
                            CPegDB & pegdb);
bool CalculateTransactionFractions(const CTransaction & tx,
                                   const CBlockIndex* pindexBlock,
                                   const MapPrevTx & inputs);

#define PegFail(...) PegReport(__VA_ARGS__)
#define PegReportf(...) PegReport(__VA_ARGS__)
bool PegReport(const char* format);

#endif
