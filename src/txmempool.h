// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include "core.h"
#include "peg.h"
#include "sync.h"

/*
 * CTxMemPool stores valid-according-to-the-current-best-chain
 * transactions that may be included in the next block.
 *
 * Transactions are added when they are seen on the network
 * (or created by the local node), but not all transactions seen
 * are added to the pool: if a new transaction double-spends
 * an input of a transaction in the pool, it is dropped,
 * as are non-standard transactions.
 */
class CTxMemPool {
private:
	uint32_t nTransactionsUpdated;

public:
	mutable CCriticalSection        cs;
	std::map<uint256, CTransaction> mapTx;
	std::map<uint256, MapPrevOut>   mapPrevOuts;
	std::map<COutPoint, CInPoint>   mapNextTx;
	std::map<uint320, std::string>  mapPackedFractions;  // #NOTE3

	CTxMemPool();

	bool     addUnchecked(const uint256&    hash,
	                      CTransaction&     tx,
	                      const MapPrevOut& mapPrevOuts,
	                      MapFractions&     mapOutputsFractions);
	bool     remove(const CTransaction& tx, bool fRecursive = false);
	bool     removeConflicts(const CTransaction& tx);
	void     reviewOnPegChange();
	void     reviewOnPegChange(CTransaction&, std::vector<uint256>& vRemove);
	void     clear();
	void     queryHashes(std::vector<uint256>& vtxid);
	uint32_t GetTransactionsUpdated() const;
	void     AddTransactionsUpdated(uint32_t n);

	unsigned long size() const {
		LOCK(cs);
		return mapTx.size();
	}

	bool exists(uint256 hash) const {
		LOCK(cs);
		return (mapTx.count(hash) != 0);
	}

	bool lookup(uint256 hash, CTransaction& result, MapFractions&) const;
	bool lookup(uint256 hash, size_t n, CFractions&) const;
};

#endif /* BITCOIN_TXMEMPOOL_H */
