// Copyright (c) 2019 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>

#include "base58.h"
#include "rpcserver.h"
#include "txdb.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "keystore.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

#ifdef ENABLE_WALLET
Value registerdeposit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
        throw runtime_error(
            "registerdeposit <txid:nout> <balance_liquid> <balance_reserve> <pegdata> <pegsupply>\n"
            );
    
    string sTxout = params[0].get_str();
    vector<string> vTxoutArgs;
    boost::split(vTxoutArgs, sTxout, boost::is_any_of(":"));
    
    if (vTxoutArgs.size() != 2) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, no txid:nout");
    }
    
    string sTxid = vTxoutArgs[0];
    char * pEnd = nullptr;
    long nout = strtol(vTxoutArgs[1].c_str(), &pEnd, 0);
    if (pEnd == vTxoutArgs[1].c_str()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, nout is not number");
    }

    int64_t balance_liquid = params[1].get_int64();
    int64_t balance_reserve = params[2].get_int64();
    string balance_pegdata64 = params[3].get_str();
    int balance_supply = params[4].get_int();
    
    CFractions balance_fractions(balance_liquid+balance_reserve, CFractions::VALUE);
    if ((balance_liquid+balance_reserve) >0) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!balance_fractions.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
        }
        if (balance_fractions.High(balance_supply) != balance_liquid) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Provided liquid balance does not match pegdata");
        }
        if (balance_fractions.Low(balance_supply) != balance_reserve) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Provided reserve balance does not match pegdata");
        }
    }
    balance_fractions = balance_fractions.Std();
    
    uint256 txhash;
    txhash.SetHex(sTxid);
    
    Object result;
    
    CTxDB txdb("r");
    CTxIndex txindex;
    if (!txdb.ReadTxIndex(txhash, txindex)) {
        throw JSONRPCError(RPC_MISC_ERROR, "ReadTxIndex tx failed");
    }
    CTransaction tx;
    if (!tx.ReadFromDisk(txindex.pos)) {
        throw JSONRPCError(RPC_MISC_ERROR, "ReadFromDisk tx failed");
    }
    
    if (nout <0 || size_t(nout) >= tx.vout.size()) {
        throw JSONRPCError(RPC_MISC_ERROR, "nout is out of index");
    }
    
    int nDepth = txindex.GetDepthInMainChain();
    int nHeight = nBestHeight - nDepth +1;
    int nPegInterval = Params().PegInterval(nHeight);
    int nNextHeight = (nHeight / nPegInterval +1) * nPegInterval;
    int nRegisterHeight = std::max(nNextHeight, nHeight + nRecommendedConfirmations);

    if (nDepth < nRecommendedConfirmations) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", strprintf("Need at least %d confirmations", nRecommendedConfirmations)));
        return result;
    }

    if (nRegisterHeight > nBestHeight) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", strprintf("Need to wait for registration at %d block", nRegisterHeight)));
        return result;
    }

    int supply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    
    CPegDB pegdb("r");
    auto fkey = uint320(txhash, nout);
    CFractions deposit_fractions(tx.vout[nout].nValue, CFractions::VALUE);
    if (!pegdb.ReadFractions(fkey, deposit_fractions, false)) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", "No peg data read failed"));
        return result;
    }
    
    deposit_fractions = deposit_fractions.Std();
    balance_fractions += deposit_fractions;
    int64_t balance_value = balance_fractions.Total();
    
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    deposit_fractions.Pack(fout);

    result.push_back(Pair("deposited", true));
    result.push_back(Pair("atblock", nRegisterHeight));
    result.push_back(Pair("supply", supply));
    result.push_back(Pair("value", balance_value));
    result.push_back(Pair("liquid", balance_fractions.High(supply)));
    result.push_back(Pair("reserve", balance_fractions.Low(supply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout.str())));
    result.push_back(Pair("status", "Registered"));
    
    return result;
}
#endif
