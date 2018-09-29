// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_PEG_H
#define BITCOIN_PEG_H

#include <list>
#include <functional>

class CTxDB;
class CPegDB;
class CBlock;
class CBlockIndex;

extern int nPegStartHeight;

// functors for messagings
typedef std::function<void(const std::string &)> LoadMsg;

class CPegFractions {
public:
};

bool SetBlocksIndexesReadyForPeg(int nStartHeight,
								 CTxDB & ctxdb,
								 LoadMsg load_msg);
bool CalculateVotesForPeg(int nStartHeight,
						  CTxDB & ctxdb,
						  LoadMsg load_msg);
bool CalculateBlockPegVotes(const CBlock & cblock,
							CBlockIndex* pindex);
bool WriteFractionsForPegTest(int nStartHeight,
							  CTxDB & ctxdb,
							  LoadMsg load_msg);
bool WriteBlockPegFractions(const CBlock & block,
							CPegDB & pegdb);

#endif
