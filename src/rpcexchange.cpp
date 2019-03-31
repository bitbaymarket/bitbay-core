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
            "registerdeposit <txid:nout> <balance_liquid> <balance_reserve> <balance_pegdata> <balance_pegsupply>\n"
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
    
    CFractions frBalance(balance_liquid+balance_reserve, CFractions::VALUE);
    if ((balance_liquid+balance_reserve) >0) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
        }
        if (frBalance.High(balance_supply) != balance_liquid) {
            throw JSONRPCError(RPC_MISC_ERROR, 
                               strprintf("Provided liquid balance %d does not match pegdata %d of balance supply %d",
                                         balance_liquid,
                                         frBalance.High(balance_supply),
                                         balance_supply));
        }
        if (frBalance.Low(balance_supply) != balance_reserve) {
            throw JSONRPCError(RPC_MISC_ERROR, 
                               strprintf("Provided reserve balance %d does not match pegdata %d of balance supply %d",
                                         balance_reserve,
                                         frBalance.Low(balance_supply),
                                         balance_supply));
        }
    }
    frBalance = frBalance.Std();
    
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
    int nCycleNow = nBestHeight / nPegInterval;

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

    int nSupply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    
    CPegDB pegdb("r");
    auto fkey = uint320(txhash, nout);
    CFractions frDeposit(tx.vout[nout].nValue, CFractions::VALUE);
    if (!pegdb.ReadFractions(fkey, frDeposit, false)) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", "No peg data read failed"));
        return result;
    }
    
    frDeposit = frDeposit.Std();
    frBalance += frDeposit;
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout);

    result.push_back(Pair("deposited", true));
    result.push_back(Pair("atblock", nRegisterHeight));
    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("cycle", nCycleNow));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(nSupply)));
    result.push_back(Pair("reserve", frBalance.Low(nSupply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout.str())));
    result.push_back(Pair("status", "Registered"));
    
    return result;
}

Value updatepegbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "updatepegbalances <balance_liquid> <balance_reserve> <balance_pegdata> <balance_pegsupply>\n"
            );
    
    int64_t balance_liquid = params[0].get_int64();
    int64_t balance_reserve = params[1].get_int64();
    string balance_pegdata64 = params[2].get_str();
    int balance_supply = params[3].get_int();
    
    CFractions frBalance(balance_liquid+balance_reserve, CFractions::VALUE);
    if ((balance_liquid+balance_reserve) >0) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
        }
        if (frBalance.High(balance_supply) != balance_liquid) {
            throw JSONRPCError(RPC_MISC_ERROR, 
                               strprintf("Provided liquid balance %d does not match pegdata %d of balance supply %d",
                                         balance_liquid,
                                         frBalance.High(balance_supply),
                                         balance_supply));
        }
        if (frBalance.Low(balance_supply) != balance_reserve) {
            throw JSONRPCError(RPC_MISC_ERROR, 
                               strprintf("Provided reserve balance %d does not match pegdata %d of balance supply %d",
                                         balance_reserve,
                                         frBalance.Low(balance_supply),
                                         balance_supply));
        }
    }
    frBalance = frBalance.Std();
    
    Object result;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;

    int nSupply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout);

    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("cycle", nCycleNow));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(nSupply)));
    result.push_back(Pair("reserve", frBalance.Low(nSupply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout.str())));
    
    return result;
}

Value takereservefrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
        throw runtime_error(
            "takereservefrom <reserve> <balance_liquid> <balance_reserve> <balance_pegdata> <balance_pegsupply>\n"
            );
    
    int64_t take_reserve = params[0].get_int64();
    int64_t balance_liquid = params[1].get_int64();
    int64_t balance_reserve = params[2].get_int64();
    string balance_pegdata64 = params[3].get_str();
    int balance_supply = params[4].get_int();
    
    CFractions frBalance(balance_liquid+balance_reserve, CFractions::VALUE);

    string pegdata = DecodeBase64(balance_pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frBalance.Unpack(finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
    }
    if (frBalance.High(balance_supply) != balance_liquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Provided liquid balance %d does not match pegdata %d of balance supply %d",
                                     balance_liquid,
                                     frBalance.High(balance_supply),
                                     balance_supply));
    }
    if (frBalance.Low(balance_supply) != balance_reserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Provided reserve balance %d does not match pegdata %d of balance supply %d",
                                     balance_reserve,
                                     frBalance.Low(balance_supply),
                                     balance_supply));
    }
    if (balance_reserve < take_reserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough reserve %d on balance to take %d",
                                     balance_reserve,
                                     take_reserve));
    }

    frBalance = frBalance.Std();
    CFractions frReserve = frBalance.LowPart(balance_supply, nullptr);
    CFractions frTake = frReserve.RatioPart(take_reserve, balance_reserve, 0);
    frBalance -= frTake;
    
    Object result;
        
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout_taken(SER_DISK, CLIENT_VERSION);
    frTake.Pack(fout_taken);
    CDataStream fout_balance(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout_balance);

    result.push_back(Pair("supply", balance_supply));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(balance_supply)));
    result.push_back(Pair("reserve", frBalance.Low(balance_supply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout_balance.str())));
    result.push_back(Pair("taken", EncodeBase64(fout_taken.str())));
    
    return result;
}

Value takeliquidfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
        throw runtime_error(
            "takeliquidfrom <liquid> <balance_liquid> <balance_reserve> <balance_pegdata> <balance_pegsupply>\n"
            );
    
    int64_t take_liquid = params[0].get_int64();
    int64_t balance_liquid = params[1].get_int64();
    int64_t balance_reserve = params[2].get_int64();
    string balance_pegdata64 = params[3].get_str();
    int balance_supply = params[4].get_int();
    
    CFractions frBalance(balance_liquid+balance_reserve, CFractions::VALUE);

    string pegdata = DecodeBase64(balance_pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frBalance.Unpack(finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
    }
    if (frBalance.High(balance_supply) != balance_liquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Provided liquid balance %d does not match pegdata %d of balance supply %d",
                                     balance_liquid,
                                     frBalance.High(balance_supply),
                                     balance_supply));
    }
    if (frBalance.Low(balance_supply) != balance_reserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Provided reserve balance %d does not match pegdata %d of balance supply %d",
                                     balance_reserve,
                                     frBalance.Low(balance_supply),
                                     balance_supply));
    }
    if (balance_liquid < take_liquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on balance to take %d",
                                     balance_liquid,
                                     take_liquid));
    }

    frBalance = frBalance.Std();
    CFractions frLiquid = frBalance.HighPart(balance_supply, nullptr);
    CFractions frTake = frLiquid.RatioPart(take_liquid, balance_liquid, balance_supply);
    frBalance -= frTake;
    
    Object result;
        
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout_taken(SER_DISK, CLIENT_VERSION);
    frTake.Pack(fout_taken);
    CDataStream fout_balance(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout_balance);

    result.push_back(Pair("supply", balance_supply));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(balance_supply)));
    result.push_back(Pair("reserve", frBalance.Low(balance_supply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout_balance.str())));
    result.push_back(Pair("taken", EncodeBase64(fout_taken.str())));
    
    return result;
}

#endif
