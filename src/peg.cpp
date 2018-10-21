// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>
#include <set>
#include <cstdint>
#include <type_traits>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/multiprecision/cpp_int.hpp>

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

int nPegStartHeight = 1837000;
int nPegMaxSupplyIndex = 1199;

static set<string> vPegWhitelist = {
    "bbiUkiqFUsTciMsBdnYETgE2DMg4k3fMdP",
    "bFFE6UMxmVqFDfsAXS4XH52DGUuTKrDtp8",
    "bEQUsGaa2ZVW4au3gSsVADhEhFYJr3gCkT",
    "bZXmqJqPJ9fyLyCPKAYEaKJqGwBA6jaQHR",
    "bMkmLGdNxbr4wpbZUr1k67Tdq5yYLE9jV2",
    "bNfFbrFau9upJ69VQrmSV3EsDwShKpb2pw",
    "bHGcVEP6piBNTnLoeyW5s18AAPbT2K8XJC",
    "bXnG4VTHFyy7Xt7sUJmKuWVerm7T4gKWSj",
    "bXxHTFPRXD5JbFzxcypMkrBCN9HNULsvfG",
    "bHkh3qrEhyJ7wnMvgzLzhrghLaX3WPpLEX",
    "bT73XQJSMhpwHnFDuC2vvUbXBGNvRk37L5",
    "bSbxMhBN2u9EmTeugpu7EQT2DAwikym64K",
    "bEnDRACm7n3pPJQhfEJ1kC2zT59fMxTeDh",
    "bQMxcnPpuga2TdxTVFr7wSCaUb7hVpfnpN",
    "bPoxnznczDN5GcryGLZ7bGpFFJtsE3RHww",
    "bLbk2cj78mUDpKvJF7xyVtVoYd1AX7PaRd",
    "bVuGpma6c8Yz5mUxswtHcZk1fQt36q1zvA",
    "bJvjfxvBQ3sc5Gj8sTQ6vLbAMsg6CfVVcz"
};

extern leveldb::DB *txdb; // global pointer for LevelDB object instance

bool SetBlocksIndexesReadyForPeg(CTxDB & ctxdb, LoadMsg load_msg) {
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

        pindexNew->SetPeg(pindexNew->nHeight >= nPegStartHeight);
        diskindex.SetPeg(pindexNew->nHeight >= nPegStartHeight);
        ctxdb.WriteBlockIndex(diskindex);

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

bool CalculateVotesForPeg(CTxDB & ctxdb, CPegDB& pegdb, LoadMsg load_msg) {
    if (!ctxdb.TxnBegin())
        return error("CalculateVotesForPeg() : TxnBegin failed");

    // now calculate peg votes
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nPegStartHeight)
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
        CalculateBlockPegIndex(block, pblockindex, pegdb);
        CalculateBlockPegVotes(block, pblockindex, pegdb);
        ctxdb.WriteBlockIndex(CDiskBlockIndex(pblockindex));

        pblockindex = pblockindex->pnext;
    }

    if (!ctxdb.WritePegVotesAreReady(true))
        return error("CalculateVotesForPeg() : flag write failed");

    if (!ctxdb.TxnCommit())
        return error("CalculateVotesForPeg() : TxnCommit failed");

    return true;
}

bool CalculateBlockPegIndex(const CBlock & cblock, CBlockIndex* pindex, CPegDB& pegdb)
{
    if (!cblock.IsProofOfStake() || pindex->nHeight < nPegStartHeight) {
        pindex->nPegSupplyIndex =0;
        return true;
    }

    if (pindex->nHeight % PEG_INTERVAL == 0) {
        // back to 2 intervals and -1 to count voice of back-third interval, as votes sum at PEG_INTERVAL-1
        auto usevotesindex = pindex;
        while (usevotesindex->nHeight > (pindex->nHeight - PEG_INTERVAL*2 -1))
            usevotesindex = usevotesindex->pprev;

        // back to 3 intervals and -1 for votes calculations of 2x and 3x
        auto prevvotesindex = pindex;
        while (prevvotesindex->nHeight > (pindex->nHeight - PEG_INTERVAL*3 -1))
            prevvotesindex = prevvotesindex->pprev;

        int inflate = usevotesindex->nPegVotesInflate;
        int deflate = usevotesindex->nPegVotesDeflate;
        int nochange = usevotesindex->nPegVotesNochange;

        int inflate_prev = prevvotesindex->nPegVotesInflate;
        int deflate_prev = prevvotesindex->nPegVotesDeflate;
        int nochange_prev = prevvotesindex->nPegVotesNochange;

        pindex->nPegSupplyIndex = pindex->pprev->nPegSupplyIndex;
        if (deflate > inflate && deflate > nochange) {
            pindex->nPegSupplyIndex++;
//            if (deflate > 2*inflate_prev && deflate > 2*nochange_prev) pindex->nPegSupplyIndex++;
//            if (deflate > 3*inflate_prev && deflate > 3*nochange_prev) pindex->nPegSupplyIndex++;
        }
        if (inflate > deflate && inflate > nochange) {
            pindex->nPegSupplyIndex--;
//            if (inflate > 2*deflate_prev && inflate > 2*nochange_prev) pindex->nPegSupplyIndex--;
//            if (inflate > 3*deflate_prev && inflate > 3*nochange_prev) pindex->nPegSupplyIndex--;
        }

        if (pindex->nPegSupplyIndex >= nPegMaxSupplyIndex)
            pindex->nPegSupplyIndex = pindex->pprev->nPegSupplyIndex;
        else if (pindex->nPegSupplyIndex <0)
            pindex->nPegSupplyIndex = 0;
    }
    else if (pindex->pprev) {
        pindex->nPegSupplyIndex = pindex->pprev->nPegSupplyIndex;
    }

    return true;
}

bool CalculateBlockPegVotes(const CBlock & cblock, CBlockIndex* pindex, CPegDB& pegdb)
{
    if (!cblock.IsProofOfStake() || pindex->nHeight < nPegStartHeight) {
        pindex->nPegVotesInflate =0;
        pindex->nPegVotesDeflate =0;
        pindex->nPegVotesNochange =0;
        return true;
    }

    if (pindex->nHeight % PEG_INTERVAL == 0) {
        pindex->nPegVotesInflate =0;
        pindex->nPegVotesDeflate =0;
        pindex->nPegVotesNochange =0;
    }
    else if (pindex->pprev) {
        pindex->nPegVotesInflate = pindex->pprev->nPegVotesInflate;
        pindex->nPegVotesDeflate = pindex->pprev->nPegVotesDeflate;
        pindex->nPegVotesNochange = pindex->pprev->nPegVotesNochange;
    }

    int nVoteWeight=1;

    const CTransaction & tx = cblock.vtx[1];

    size_t n_vin = tx.vin.size();
    if (tx.IsCoinBase()) n_vin = 0;
    for (unsigned int i = 0; i < n_vin; i++)
    {
        auto fractions = CFractions(0);
        const COutPoint & prevout = tx.vin[i].prevout;
        if (!pegdb.Read(uint320(prevout.hash, prevout.n), fractions)) {
            continue;
        }

        int64_t nReserveWeight=0;
        int64_t nLiquidityWeight=0;

        fractions.Reserve(pindex->nPegSupplyIndex, &nReserveWeight);
        fractions.Liquidity(pindex->nPegSupplyIndex, &nLiquidityWeight);

        if (nLiquidityWeight > INT_LEAST64_MAX/(pindex->nPegSupplyIndex+2)) {
            // check for rare extreme case when user stake more than about 100M coins
            // in this case multiplication is very close int64_t overflow (int64 max is ~92 GCoins)
            multiprecision::uint128_t nLiquidityWeight128(nLiquidityWeight);
            multiprecision::uint128_t nPegSupplyIndex128(pindex->nPegSupplyIndex);
            multiprecision::uint128_t nPegMaxSupplyIndex128(nPegMaxSupplyIndex);
            multiprecision::uint128_t f128 = (nLiquidityWeight128*nPegSupplyIndex128)/nPegMaxSupplyIndex128;
            nLiquidityWeight -= f128.convert_to<int64_t>();
        }
        else // usual case, fast calculations
            nLiquidityWeight -= nLiquidityWeight * pindex->nPegSupplyIndex / nPegMaxSupplyIndex;

        int nWeightMultiplier = pindex->nPegSupplyIndex/120+1;
        if (nLiquidityWeight > (nReserveWeight*4)) {
            nVoteWeight = 4*nWeightMultiplier;
        }
        else if (nLiquidityWeight > (nReserveWeight*3)) {
            nVoteWeight = 3*nWeightMultiplier;
        }
        else if (nLiquidityWeight > (nReserveWeight*2)) {
            nVoteWeight = 2*nWeightMultiplier;
        }
        break;
    }

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
                pindex->nPegVotesInflate += nVoteWeight;
                voted = true;
                break;
            }
            else if (str_addr == "bNyZrP2SbrV6v5HqeBoXZXZDE2e4fe6STo") {
                pindex->nPegVotesDeflate += nVoteWeight;
                voted = true;
                break;
            }
            else if (str_addr == "bNyZrPeFFNP6GFJZCkE82DDN7JC4K5Vrkk") {
                pindex->nPegVotesNochange += nVoteWeight;
                voted = true;
                break;
            }
        }

        if (voted) // only one vote to count
            break;
    }

    return true;
}

CFractions::CFractions()
    :nFlags(PEG_VALUE)
{
    f[0] = 0; // fast init first item
}
CFractions::CFractions(int64_t value)
    :nFlags(PEG_VALUE)
{
    f[0] = value; // fast init first item
}
CFractions::CFractions(const CFractions & o)
    :nFlags(o.nFlags)
{
    for(int i=0; i< PEG_SIZE; i++) {
        f[i] = o.f[i];
    }
}

void CFractions::ToDeltas(int64_t* deltas) const
{
    int64_t fp = 0;
    for(int i=0; i<PEG_SIZE; i++) {
        if (i==0) {
            fp = deltas[0] = f[0];
            continue;
        }
        deltas[i] = f[i]-fp*(PEG_RATE-1)/PEG_RATE;
        fp = f[i];
    }
}

void CFractions::FromDeltas(const int64_t* deltas)
{
    int64_t fp = 0;
    for(int i=0; i<PEG_SIZE; i++) {
        if (i==0) {
            fp = f[0] = deltas[0];
            continue;
        }
        f[i] = deltas[i]+fp*(PEG_RATE-1)/PEG_RATE;
        fp = f[i];
    }
}

bool CFractions::Pack(CDataStream& out, unsigned long* report_len) const
{
    if (nFlags == PEG_VALUE) {
        if (report_len) *report_len = sizeof(int64_t);
        out << int(SER_VALUE);
        out << f[0];
    } else {
        int64_t deltas[PEG_SIZE];
        ToDeltas(deltas);

        int zlevel = 9;
        unsigned char zout[2*PEG_SIZE*sizeof(int64_t)];
        unsigned long n = PEG_SIZE*sizeof(int64_t);
        unsigned long zlen = PEG_SIZE*2*sizeof(int64_t);
        auto src = reinterpret_cast<const unsigned char *>(deltas);
        int res = ::compress2(zout, &zlen, src, n, zlevel);
        if (res == Z_OK) {
            if (report_len) *report_len = zlen;
            out << int(SER_ZDELTA);
            auto ser = reinterpret_cast<const char *>(zout);
            out << zlen;
            out.write(ser, zlen);
        }
        else {
            if (report_len) *report_len = PEG_SIZE*sizeof(int64_t);
            out << int(SER_RAW);
            auto ser = reinterpret_cast<const char *>(f);
            out.write(ser, PEG_SIZE*sizeof(int64_t));
        }
    }
    return true;
}

bool CFractions::Unpack(CDataStream& inp)
{
    int nSerFlags = 0;
    inp >> nSerFlags;
    if (nSerFlags == SER_VALUE) {
        nFlags = PEG_VALUE;
        inp >> f[0];
    }
    else if (nSerFlags == SER_ZDELTA) {
        unsigned long zlen = 0;
        inp >> zlen;

        if (zlen>(2*PEG_SIZE*sizeof(int64_t))) {
            // data are broken, no read
            return false;
        }

        unsigned char zinp[2*PEG_SIZE*sizeof(int64_t)];
        unsigned long n = PEG_SIZE*sizeof(int64_t);
        auto ser = reinterpret_cast<char *>(zinp);
        inp.read(ser, zlen);

        int64_t deltas[PEG_SIZE];
        auto src = reinterpret_cast<const unsigned char *>(ser);
        auto dst = reinterpret_cast<unsigned char *>(deltas);
        int res = ::uncompress(dst, &n, src, zlen);
        if (res != Z_OK) {
            // data are broken, can not uncompress
            return false;
        }
        FromDeltas(deltas);
        nFlags = PEG_STD;
    }
    else if (nSerFlags == SER_RAW) {
        auto ser = reinterpret_cast<char *>(f);
        inp.read(ser, PEG_SIZE*sizeof(int64_t));
        nFlags = PEG_STD;
    }
    return true;
}

CFractions CFractions::Std() const
{
    if (nFlags!=PEG_VALUE)
        return *this;

    CFractions fstd;
    fstd.nFlags = PEG_STD;

    int64_t v = f[0];
    for(int i=0;i<PEG_SIZE;i++) {
        if (i == PEG_SIZE-1) {
            fstd.f[i] = v;
            break;
        }
        int64_t frac = v/PEG_RATE;
        fstd.f[i] = frac;
        v -= frac;
    }
    return fstd;
}

int64_t CFractions::Total() const
{
    int64_t nValue =0;
    if (nFlags == PEG_VALUE)
        return f[0];

    for(int i=0;i<PEG_SIZE;i++) {
        nValue += f[i];
    }
    return nValue;
}

void CFractions::ToStd()
{
    if (nFlags!=PEG_VALUE)
        return;

    nFlags = PEG_STD;
    int64_t v = f[0];
    for(int i=0;i<PEG_SIZE;i++) {
        if (i == PEG_SIZE-1) {
            f[i] = v;
            break;
        }
        int64_t frac = v/PEG_RATE;
        f[i] = frac;
        v -= frac;
    }
}

CFractions CFractions::Reserve(int supply, int64_t* total) const
{
    if (nFlags != PEG_STD) {
        return Std().Reserve(supply, total);
    }
    CFractions freserve = CFractions(0).Std();
    for(int i=0; i<supply; i++) {
        if (total) *total += f[i];
        freserve.f[i] += f[i];
    }
    return freserve;
}
CFractions CFractions::Liquidity(int supply, int64_t* total) const
{
    if (nFlags != PEG_STD) {
        return Std().Liquidity(supply, total);
    }
    CFractions fliquidity = CFractions(0).Std();
    for(int i=supply; i<PEG_SIZE; i++) {
        if (total) *total += f[i];
        fliquidity.f[i] += f[i];
    }
    return fliquidity;
}

/** Take a part as ration part/total where part is also value (sum fraction)
 *  Returned fractions are also adjusted for part for rounding differences.
 */
CFractions CFractions::RatioPart(int64_t nPartValue,
                                       int64_t nTotalValue,
                                       int adjust_from) {
    ToStd();
    int64_t nPartValueSum = 0;
    CFractions fPart = CFractions(0).Std();
    for(int i=0; i<PEG_SIZE; i++) {
        int64_t v = f[i];

        bool has_overflow = false;
        if (std::is_same<int64_t,long>()) {
            long m_test;
            has_overflow = __builtin_smull_overflow(v, nPartValue, &m_test);
        } else if (std::is_same<int64_t,long long>()) {
            long long m_test;
            has_overflow = __builtin_smulll_overflow(v, nPartValue, &m_test);
        } else {
            has_overflow = false; // todo: compile error
        }

        if (has_overflow) {
            multiprecision::uint128_t v128(v);
            multiprecision::uint128_t part128(nPartValue);
            multiprecision::uint128_t f128 = (v128*part128)/nTotalValue;
            fPart.f[i] = f128.convert_to<int64_t>();
        }
        else {
            fPart.f[i] = (v*nPartValue)/nTotalValue;
        }

        nPartValueSum += fPart.f[i];
    }
    for (int64_t i=nPartValueSum; i<nPartValue; i++) {
        fPart.f[adjust_from + ((i-nPartValueSum) % (PEG_SIZE-adjust_from))]++;
    }
    return fPart;
}

CFractions& CFractions::operator+=(const CFractions& b)
{
    for(int i=0; i<PEG_SIZE; i++) {
        f[i] += b.f[i];
    }
    return *this;
}

CFractions& CFractions::operator-=(const CFractions& b)
{
    for(int i=0; i<PEG_SIZE; i++) {
        f[i] -= b.f[i];
    }
    return *this;
}

// temp:peg reporting

bool PegReport(const char* format)
{
    LogPrintStr(std::string("ERROR: ") + format + "\n");
    return false;
}

int PegPrintStr(const std::string &str)
{
    LogPrintStr("PEG:"+str);
    return 0;
}

bool IsPegWhiteListed(const CTransaction & tx,
                      MapPrevTx & inputs)
{
    size_t n_vin = tx.vin.size();
    if (tx.IsCoinBase()) n_vin = 0;
    for (unsigned int i = 0; i < n_vin; i++)
    {
        const COutPoint & prevout = tx.vin[i].prevout;
        CTransaction& txPrev = inputs[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size()) {
            continue;
        }

        int nRequired;
        txnouttype type;
        vector<CTxDestination> addresses;
        const CScript& scriptPubKey = txPrev.vout[prevout.n].scriptPubKey;
        if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
            for(const CTxDestination& addr : addresses) {
                std::string str_addr = CBitcoinAddress(addr).ToString();
                if (vPegWhitelist.count(str_addr))
                    return true;
            }
        }
    }
    return false;
}

bool CalculateTransactionFractions(const CTransaction & tx,
                                   const CBlockIndex* pindexBlock,
                                   MapPrevTx & inputs,
                                   MapPrevFractions& fInputs,
                                   map<uint256, CTxIndex>& mapTestPool,
                                   map<uint320, CFractions>& mapTestFractionsPool,
                                   CFractions& feesFractions,
                                   std::vector<int>& vOutputsTypes)
{
    size_t n_vin = tx.vin.size();
    size_t n_vout = tx.vout.size();
    vOutputsTypes.resize(n_vout);

    if (!IsPegWhiteListed(tx, inputs))
        return true;

    // calculate liquidity and total pools
    int64_t nValueIn = 0;
    int64_t nLiquidityTotal =0;

    auto fValueInPool = CFractions(0).Std();
    auto fLiquidityPool = CFractions(0).Std();

    int supply = pindexBlock->nPegSupplyIndex;
    set<string> sInputAddresses;

    if (tx.IsCoinBase()) n_vin = 0;
    for (unsigned int i = 0; i < n_vin; i++)
    {
        const COutPoint & prevout = tx.vin[i].prevout;

        auto fkey = uint320(prevout.hash, prevout.n);
        if (fInputs.find(fkey) == fInputs.end()) {
            return false;
        }

        auto fInp = fInputs[fkey].Std();

        fValueInPool += fInp;
        fLiquidityPool += fInp.Liquidity(supply, &nLiquidityTotal);

        CTransaction& txPrev = inputs[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size()) {
            return false;
        }

        if (fInp.Total() != txPrev.vout[prevout.n].nValue) {
            return false; // input mismatch
        }

        nValueIn += txPrev.vout[prevout.n].nValue;

        int nRequired;
        txnouttype type;
        vector<CTxDestination> addresses;
        const CScript& scriptPubKey = txPrev.vout[prevout.n].scriptPubKey;
        if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
            for(const CTxDestination& addr : addresses) {
                std::string str_addr = CBitcoinAddress(addr).ToString();
                if (!str_addr.empty()) sInputAddresses.insert(str_addr);
            }
        }
    }

    auto nRemainsTotal = nValueIn;
    auto fRemainsPool = fValueInPool;

    // Reveal outs destination type
    for (unsigned int i = 0; i < n_vout; i++)
    {
        vOutputsTypes[i] = PEG_DEST_OUT;

        int nRequired;
        txnouttype type;
        vector<CTxDestination> addresses;
        const CScript& scriptPubKey = tx.vout[i].scriptPubKey;
        if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
            for(const CTxDestination& addr : addresses) {
                std::string str_addr = CBitcoinAddress(addr).ToString();
                if (sInputAddresses.count(str_addr) >0) {
                    vOutputsTypes[i] = PEG_DEST_SELF;
                }
            }
        }
    }

    int64_t nValueOut = 0;

    // Calculation of liquidity outputs
    // These are substracted from totals
    for (unsigned int i = 0; i < n_vout; i++)
    {
        int64_t nValue = tx.vout[i].nValue;
        nValueOut += nValue;

        auto fkey = uint320(tx.GetHash(), i);

        if (vOutputsTypes[i] == PEG_DEST_OUT) {
            if (nLiquidityTotal == 0) {
                mapTestFractionsPool[fkey] = CFractions(0);
                continue;
            }

            if (nValue > nLiquidityTotal) {
                return false; // violate peg
            }
            auto fOut = fLiquidityPool.RatioPart(nValue, nLiquidityTotal, supply);
            mapTestFractionsPool[fkey] = fOut;
            fRemainsPool -= fOut;
            nRemainsTotal -= nValue;

            if (fOut.Total() != tx.vout[i].nValue) {
                return false; // output mismatch
            }
        }
    }

    // Calculation of reserve outputs
    // These are substracted from remains pool
    for (unsigned int i = 0; i < n_vout; i++)
    {
        int64_t nValue = tx.vout[i].nValue;
        auto fkey = uint320(tx.GetHash(), i);

        if (vOutputsTypes[i] == PEG_DEST_SELF) {
            if (nRemainsTotal == 0) {
                mapTestFractionsPool[fkey] = CFractions(0);
                continue;
            }

            if (nValue > nRemainsTotal) {
                return false; // violate peg
            }

            auto fOut = fRemainsPool.RatioPart(nValue, nRemainsTotal, 0);
            mapTestFractionsPool[fkey] = fOut;

            if (fOut.Total() != tx.vout[i].nValue) {
                return false; // output mismatch
            }
        }
    }

    // Calculation of fees
    // These are substracted from remains pool
    {
        if (nRemainsTotal == 0) {
            return false; // no remains for fee?
        }
        int64_t nValueFee = nValueIn - nValueOut;
        if (nValueFee > nRemainsTotal) {
            return false; // violate peg
        }
        auto fFee = fRemainsPool.RatioPart(nValueFee, nRemainsTotal, 0);
        feesFractions += fFee;
    }

    return true;
}

bool CalculateStakingFractions(const CTransaction & tx,
                               const CBlockIndex* pindexBlock,
                               MapPrevTx & inputs,
                               MapPrevFractions& fInputs,
                               std::map<uint256, CTxIndex>& mapTestPool,
                               std::map<uint320, CFractions>& mapTestFractionsPool,
                               const CFractions& feesFractions,
                               int64_t nCalculatedStakeRewardWithoutFees,
                               std::vector<int>& vOutputsTypes)
{
    size_t n_vin = tx.vin.size();
    size_t n_vout = tx.vout.size();
    vOutputsTypes.resize(n_vout);

    if (!IsPegWhiteListed(tx, inputs))
        return true;

    // calculate pools
    int64_t nValueIn = 0;

    auto fValueInPool = CFractions(0).Std();

    for (unsigned int i = 0; i < n_vin; i++)
    {
        const COutPoint & prevout = tx.vin[i].prevout;

        auto fkey = uint320(prevout.hash, prevout.n);
        if (fInputs.find(fkey) == fInputs.end()) {
            return false;
        }

        auto fInp = fInputs[fkey].Std();

        fValueInPool += fInp;

        CTransaction& txPrev = inputs[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size()) {
            return false;
        }

        if (fInp.Total() != txPrev.vout[prevout.n].nValue) {
            return false; // input mismatch
        }

        nValueIn += txPrev.vout[prevout.n].nValue;
    }

    auto fValueOutPool = fValueInPool;
    CFractions fStakeReward(nCalculatedStakeRewardWithoutFees);
    fValueOutPool += fStakeReward.Std();
    fValueOutPool += feesFractions;

    auto nValueOut = nValueIn;
    nValueOut += nCalculatedStakeRewardWithoutFees;
    nValueOut += feesFractions.Total();

    // Calculation of outputs fractions just as ratio
    for (unsigned int i = 0; i < n_vout; i++)
    {
        int64_t nValue = tx.vout[i].nValue;
        auto fkey = uint320(tx.GetHash(), i);

        if (nValue > nValueOut) {
            return false; // violate peg
        }

        auto fOut = fValueOutPool.RatioPart(nValue, nValueOut, 0);
        mapTestFractionsPool[fkey] = fOut;

        if (fOut.Total() != tx.vout[i].nValue) {
            return false; // output mismatch
        }
    }

    return true;
}


