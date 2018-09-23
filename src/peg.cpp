// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

#include <leveldb/env.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#include <memenv/memenv.h>

#include "peg.h"
#include "txdb-leveldb.h"
#include "pegdb-leveldb.h"
#include "util.h"
#include "main.h"
#include "base58.h"

#include <zconf.h>
#include <zlib.h>

using namespace std;
using namespace boost;

extern leveldb::DB *txdb; // global pointer for LevelDB object instance

bool SetBlocksIndexesReadyForPeg(int nStartHeight, CTxDB & ctxdb, LoadMsg load_msg) {
    if (!ctxdb.TxnBegin())
        return error("SetBlocksIndexesReadyForPeg() : TxnBegin failed");

    leveldb::Iterator *iterator = txdb->NewIterator(leveldb::ReadOptions());
    // Seek to start key.
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256(0));
    iterator->Seek(ssStartKey.str());
    // Now read each entry.
    int indexCount = 0;
    while (iterator->Valid())
    {
        boost::this_thread::interruption_point();
        // Unpack keys and values.
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iterator->key().data(), iterator->key().size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(iterator->value().data(), iterator->value().size());
        string strType;
        ssKey >> strType;
        // Did we reach the end of the data to read?
        if (strType != "blockindex")
            break;
        CDiskBlockIndex diskindex;
        ssValue >> diskindex;

        uint256 blockHash = diskindex.GetBlockHash();
        CBlockIndex* pindexNew = ctxdb.InsertBlockIndex(blockHash);

        if (pindexNew->nHeight >= nStartHeight) {
            pindexNew->SetPeg(true);
            diskindex.SetPeg(true);
            ctxdb.WriteBlockIndex(diskindex);
        }

        iterator->Next();

        indexCount++;
        if (indexCount % 10000 == 0) {
            load_msg(std::string(" update indexes for peg: ")+std::to_string(indexCount));
        }
    }
    delete iterator;


    if (!ctxdb.WriteBlockIndexIsPegReady(true))
        return error("SetBlocksIndexesReadyForPeg() : flag write failed");

    if (!ctxdb.TxnCommit())
        return error("SetBlocksIndexesReadyForPeg() : TxnCommit failed");

    return true;
}

bool CalculateVotesForPeg(int nStartHeight, CTxDB & ctxdb, LoadMsg load_msg) {
    if (!ctxdb.TxnBegin())
        return error("CalculateVotesForPeg() : TxnBegin failed");

    // now calculate peg votes
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nStartHeight)
        pblockindex = pblockindex->pprev;

    CBlock block;
    while (pblockindex && pblockindex->nHeight <= nBestHeight) {
        uint256 hash = *pblockindex->phashBlock;
        pblockindex = mapBlockIndex[hash];

        if (pblockindex->nHeight % 10000 == 0) {
            load_msg(std::string(" calculate votes: ")+std::to_string(pblockindex->nHeight));
        }

        // calc votes per block
        block.ReadFromDisk(pblockindex, true);
        CalculateBlockPegVotes(block, pblockindex);
        ctxdb.WriteBlockIndex(CDiskBlockIndex(pblockindex));

        pblockindex = pblockindex->pnext;
    }

    if (!ctxdb.WritePegVotesAreReady(true))
        return error("CalculateVotesForPeg() : flag write failed");

    if (!ctxdb.TxnCommit())
        return error("CalculateVotesForPeg() : TxnCommit failed");

    return true;
}

bool CalculateBlockPegVotes(const CBlock & cblock, CBlockIndex* pindex)
{
    if (!cblock.IsProofOfStake()) {
        pindex->nPegSupplyIndex =0;
        pindex->nPegVotesInflate =0;
        pindex->nPegVotesDeflate =0;
        pindex->nPegVotesNochange =0;
        return true;
    }

    if (pindex->nHeight % 200 == 0) {
        pindex->nPegVotesInflate =0;
        pindex->nPegVotesDeflate =0;
        pindex->nPegVotesNochange =0;
    }
    else if (pindex->pprev) {
        pindex->nPegVotesInflate = pindex->pprev->nPegVotesInflate;
        pindex->nPegVotesDeflate = pindex->pprev->nPegVotesDeflate;
        pindex->nPegVotesNochange = pindex->pprev->nPegVotesNochange;
    }

    const CTransaction & tx = cblock.vtx[1];
    for(const CTxOut & out : tx.vout) {
        const CScript& scriptPubKey = out.scriptPubKey;

        txnouttype type;
        vector<CTxDestination> addresses;
        int nRequired;

        if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
            continue;
        }

        bool voted = false;
        for(const CTxDestination& addr : addresses) {
            std::string str_addr = CBitcoinAddress(addr).ToString();
            if (str_addr == "bNyZrPLQAMPvYedrVLDcBSd8fbLdNgnRPz") {
                pindex->nPegVotesInflate++;
                voted = true;
                break;
            }
            else if (str_addr == "bNyZrP2SbrV6v5HqeBoXZXZDE2e4fe6STo") {
                pindex->nPegVotesDeflate++;
                voted = true;
                break;
            }
            else if (str_addr == "bNyZrPeFFNP6GFJZCkE82DDN7JC4K5Vrkk") {
                pindex->nPegVotesNochange++;
                voted = true;
                break;
            }
        }

        if (voted) // only one vote to count
            break;
    }

    return true;
}

bool WriteFractionsForPegTest(int nStartHeight, CTxDB & ctxdb, LoadMsg load_msg) {
    // now calculate peg votes
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nStartHeight)
        pblockindex = pblockindex->pprev;

    CPegDB pegdb("cr+");
    if (!pegdb.TxnBegin())
        return error("LoadBlockIndex() : peg TxnBegin failed");

    CBlock block;
    while (pblockindex && pblockindex->nHeight <= nBestHeight) {
        uint256 hash = *pblockindex->phashBlock;
        pblockindex = mapBlockIndex[hash];

        if (pblockindex->nHeight % 1000 == 0) {
            load_msg(std::string(" save fractions: ")+std::to_string(pblockindex->nHeight));

            if (!pegdb.TxnCommit())
                return error("LoadBlockIndex() :peg db update failed");
            if (!pegdb.TxnBegin())
                return error("LoadBlockIndex() : peg TxnBegin failed");
        }

        // calc votes per block
        block.ReadFromDisk(pblockindex, true);
        WriteBlockPegFractions(block, pegdb);

        pblockindex = pblockindex->pnext;
    }

    if (!pegdb.TxnCommit())
        return error("LoadBlockIndex() :peg db update failed");

    //return error("LoadBlockIndex() : votes are not ready");
    return true;
}

static void value_to_fractions(int64_t v, int64_t *fs) {
    int64_t vf = 0;
    for(int i=0;i<1200;i++) {
        int64_t f = v/100;
        int64_t fm = v % 100;
        if (fm >= 45) f++;
        v -= f;
        vf += f;
        fs[i] = f;
    }
    int64_t r = v-vf;
    for(int i=0;i<r;i++) {
        fs[i]++;
    }
}

static void fractions_to_deltas(int64_t *f, int64_t *fd) {
    int64_t fp = 0;
    for(int i=0; i<1200; i++) {
        if (i==0) {
            fp = fd[0] = f[0];
            continue;
        }
        fd[i] = f[i]-fp*99/100;
        fp = f[i];
    }
}

bool WriteBlockPegFractions(const CBlock & block, CPegDB& pegdb) {
    for(const CTransaction & tx : block.vtx) {
        int vout_idx = 0;
        for(const CTxOut & out : tx.vout) {
            uint256 hash = tx.GetHash();
            CDataStream ssPegFractions(SER_DISK, CLIENT_VERSION);
            int nFlags = 0;
            ssPegFractions << nFlags;

            int64_t f64[1200];
            int64_t f64d[1200];

            value_to_fractions(out.nValue, f64);
            fractions_to_deltas(f64, f64d);

            //QByteArray compressed_bytes = qCompress((const uchar *)(f64d), 1200*8, 9);
            //ssPegFractions.write(compressed_bytes.data(), compressed_bytes.size());

            int compressionLevel = 9;
            ulong nbytes = 1200*8;
            ulong len = nbytes + nbytes / 100 + 13;
            char zout[len+4];
            const unsigned char * data = (const unsigned char *)(f64d);

            int res;
            do {
                res = ::compress2((unsigned char*)zout+4, &len, data, nbytes, compressionLevel);
                switch (res) {
                case Z_OK:
                    zout[0] = (nbytes & 0xff000000) >> 24;
                    zout[1] = (nbytes & 0x00ff0000) >> 16;
                    zout[2] = (nbytes & 0x0000ff00) >> 8;
                    zout[3] = (nbytes & 0x000000ff);
                    break;
                case Z_MEM_ERROR:
                    return false;
                case Z_BUF_ERROR:
                    return false;
                }
            } while (res == Z_BUF_ERROR);

            int zout_len = len+4;

            ssPegFractions.write((const char *)zout, zout_len);

            pegdb.Write(make_pair(hash.ToString(), vout_idx), ssPegFractions);
            vout_idx++;
        }
    }
    return true;
}

