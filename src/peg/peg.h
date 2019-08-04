// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITBAY_PEG_H
#define BITBAY_PEG_H

#include <list>
#include <functional>

#include "bignum.h"
#include "tinyformat.h"
#include "pegdata.h"

class CTxDB;
class CPegDB;
class CBlock;
class CBlockIndex;
class CTxOut;
class CTxIndex;
class CTransaction;
class CPegLevel;
class CFractions;
typedef std::map<uint256, std::pair<CTxIndex, CTransaction> > MapPrevTx;
typedef std::map<uint320, CTxOut> MapPrevOut;

extern int nPegStartHeight;
extern int nPegMaxSupplyIndex;
extern bool fPegIsActivatedViaTx;
extern bool fPegDemoMode;
extern std::set<std::string> vPegWhitelist;
extern uint256 pegWhiteListHash;

// functors for messagings
typedef std::function<void(const std::string &)> LoadMsg;

enum PegTxType {
    PEG_MAKETX_SEND_RESERVE     = 0,
    PEG_MAKETX_SEND_LIQUIDITY   = 1,
    PEG_MAKETX_FREEZE_RESERVE   = 2,
    PEG_MAKETX_FREEZE_LIQUIDITY = 3,
    PEG_MAKETX_SEND_TOCOLD      = 4,
    PEG_MAKETX_SEND_FROMCOLD    = 5
};

enum PegVoteType {
    PEG_VOTE_NONE       = 0,
    PEG_VOTE_AUTO       = 1,
    PEG_VOTE_INFLATE    = 2,
    PEG_VOTE_DEFLATE    = 3,
    PEG_VOTE_NOCHANGE   = 4
};

enum PegRewardType {
    PEG_REWARD_5    = 0,
    PEG_REWARD_10   = 1,
    PEG_REWARD_20   = 2,
    PEG_REWARD_40   = 3,
    PEG_REWARD_LAST
};

struct FrozenTxOut {
    int64_t nValue                      = 0;
    bool fIsColdOutput                  = false;
    long nFairWithdrawFromEscrowIndex1  = -1;
    long nFairWithdrawFromEscrowIndex2  = -1;
    std::string sAddress;
    CFractions fractions;
};

bool ReadWhitelistInfo();
bool SetBlocksIndexesReadyForPeg(CTxDB & ctxdb,
                                 LoadMsg load_msg);
bool CalculateBlockPegIndex(CBlockIndex* pindex);
bool CalculateBlockPegVotes(const CBlock & cblock,
                            CBlockIndex* pindex,
                            CPegDB& pegdb);
int CalculatePegVotes(const CFractions & fractions, 
                      int nPegSupplyIndex);

bool IsPegWhiteListed(const CTransaction & tx, MapPrevTx & inputs);
bool IsPegWhiteListed(const CTransaction & tx, MapPrevOut & inputs);

bool CalculateStandardFractions(const CTransaction & tx,
                                int nSupply,
                                unsigned int nTime,
                                MapPrevTx & inputs,
                                MapFractions& finputs,
                                MapFractions& mapTestFractionsPool,
                                CFractions& feesFractions,
                                std::string& sPegFailCause);

bool CalculateStandardFractions(const CTransaction & tx,
                                int nSupply,
                                unsigned int nTime,
                                MapPrevOut & inputs,
                                MapFractions& finputs,
                                MapFractions& mapTestFractionsPool,
                                CFractions& feesFractions,
                                std::string& sPegFailCause);

bool CalculateStakingFractions(const CTransaction & tx,
                               const CBlockIndex* pindexBlock,
                               MapPrevTx & inputs,
                               MapFractions& finputs,
                               std::map<uint256, CTxIndex>& mapTestPool,
                               MapFractions& mapTestFractionsPool,
                               const CFractions& feesFractions,
                               int64_t nCalculatedStakeRewardWithoutFees,
                               std::string& sPegFailCause);
void PrunePegForBlock(const CBlock&, CPegDB&);

#endif
