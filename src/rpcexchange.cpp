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

static string packpegdata(const CFractions & fractions,
                          const CPegLevel & peglevel)
{
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    fractions.Pack(fout);
    peglevel.Pack(fout);
    int64_t nReserve = fractions.Low(peglevel);
    int64_t nLiquid = fractions.High(peglevel);
    fout << nReserve;
    fout << nLiquid;
    return EncodeBase64(fout.str());
}

static string packpegbalance(const CFractions & fractions,
                             const CPegLevel & peglevel,
                             int64_t nReserve,
                             int64_t nLiquid)
{
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    fractions.Pack(fout);
    peglevel.Pack(fout);
    fout << nReserve;
    fout << nLiquid;
    return EncodeBase64(fout.str());
}

static void unpackpegdata(CFractions & fractions, 
                          const string & pegdata64,
                          string tag)
{
    if (pegdata64.empty()) 
        return;
    
    string pegdata = DecodeBase64(pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    
    if (!fractions.Unpack(finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, 
                            "Can not unpack '"+tag+"' pegdata");
    }
}

static void unpackbalance(CFractions & fractions, 
                             CPegLevel & peglevel,
                             const string & pegdata64,
                             string tag)
{
    if (pegdata64.empty()) 
        return;
    
    string pegdata = DecodeBase64(pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    
    if (!fractions.Unpack(finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, 
                            "Can not unpack '"+tag+"' pegdata");
    }
    
    try { 
    
        CPegLevel peglevel_copy = peglevel;
        if (!peglevel_copy.Unpack(finp)) {
//             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, 
//                                "Can not unpack '"+tag+"' peglevel");
        }
        else peglevel = peglevel_copy;
    
    }
    catch (std::exception &) { ; }
}

static void printpegshift(const CFractions & frPegShift, 
                          const CPegLevel & peglevel,
                          Object & result,
                          bool print_pegdata) 
{
    int64_t nPegShiftValue = frPegShift.Positive(nullptr).Total();
    int64_t nPegShiftLiquid = nPegShiftValue;
    int64_t nPegShiftReserve = nPegShiftValue;
    CFractions frPegShiftLiquid = frPegShift.HighPart(peglevel, nullptr);
    CFractions frPegShiftReserve = frPegShift.LowPart(peglevel, nullptr);
    int64_t nPegShiftLiquidNegative = 0;
    int64_t nPegShiftLiquidPositive = 0;
    frPegShiftLiquid.Negative(&nPegShiftLiquidNegative);
    frPegShiftLiquid.Positive(&nPegShiftLiquidPositive);
    int64_t nPegShiftReserveNegative = 0;
    int64_t nPegShiftReservePositive = 0;
    frPegShiftReserve.Negative(&nPegShiftReserveNegative);
    frPegShiftReserve.Positive(&nPegShiftReservePositive);
    nPegShiftLiquid = std::min(nPegShiftLiquidPositive, -nPegShiftLiquidNegative);
    nPegShiftReserve = std::min(nPegShiftReservePositive, -nPegShiftReserveNegative);

    result.push_back(Pair("pegshift_value", nPegShiftValue));
    result.push_back(Pair("pegshift_liquid", nPegShiftLiquid));
    result.push_back(Pair("pegshift_reserve", nPegShiftReserve));
    if (print_pegdata) {
        result.push_back(Pair("pegshift_pegdata", packpegdata(frPegShift, peglevel)));
    }
}

static void printpeglevel(const CPegLevel & peglevel,
                          Object & result) 
{
    result.push_back(Pair("peglevel", peglevel.ToString()));
    
    Object info;
    info.push_back(Pair("cycle", peglevel.nCycle));
    info.push_back(Pair("peg", peglevel.nSupply));
    info.push_back(Pair("pegnext", peglevel.nSupplyNext));
    info.push_back(Pair("pegnextnext", peglevel.nSupplyNextNext));
    info.push_back(Pair("shift", peglevel.nShift));
    info.push_back(Pair("shiftlastpart", peglevel.nShiftLastPart));
    info.push_back(Pair("shiftlasttotal", peglevel.nShiftLastTotal));
    info.push_back(Pair("shiftliquidity", peglevel.nShiftLiquidity));
    result.push_back(Pair("peglevel_info", info));
}

static void printpegbalance(const CFractions & frBalance,
                            const CPegLevel & peglevel,
                            Object & result,
                            string prefix,
                            bool print_pegdata) 
{
    int64_t nValue      = frBalance.Total();
    int64_t nLiquid     = frBalance.High(peglevel);
    int64_t nReserve    = frBalance.Low(peglevel);
    int64_t nNChange    = frBalance.NChange(peglevel);

    result.push_back(Pair(prefix+"value", nValue));
    result.push_back(Pair(prefix+"liquid", nLiquid));
    result.push_back(Pair(prefix+"reserve", nReserve));
    result.push_back(Pair(prefix+"nchange", nNChange));
    if (print_pegdata) {
        result.push_back(Pair(prefix+"pegdata", packpegdata(frBalance, peglevel)));
    }
}

static void consumepegshift(CFractions & frBalance, 
                            CFractions & frExchange, 
                            CFractions & frPegShift,
                            const CFractions & frPegShiftInput) {
    int64_t nPegShiftPositive = 0;
    int64_t nPegShiftNegative = 0;
    CFractions frPegShiftPositive = frPegShiftInput.Positive(&nPegShiftPositive);
    CFractions frPegShiftNegative = frPegShiftInput.Negative(&nPegShiftNegative);
    CFractions frPegShiftNegativeConsume = frPegShiftNegative & (-frBalance);
    int64_t nPegShiftNegativeConsume = frPegShiftNegativeConsume.Total();
    int64_t nPegShiftPositiveConsume = frPegShiftPositive.Total();
    if ((-nPegShiftNegativeConsume) > nPegShiftPositiveConsume) {
        CFractions frToPositive = -frPegShiftNegativeConsume; 
        frToPositive = frToPositive.RatioPart(nPegShiftPositiveConsume);
        frPegShiftNegativeConsume = -frToPositive;
        nPegShiftNegativeConsume = frPegShiftNegativeConsume.Total();
    }
    nPegShiftPositiveConsume = -nPegShiftNegativeConsume;
    CFractions frPegShiftPositiveConsume = frPegShiftPositive.RatioPart(nPegShiftPositiveConsume);
    CFractions frPegShiftConsume = frPegShiftNegativeConsume + frPegShiftPositiveConsume;
    
    frBalance += frPegShiftConsume;
    frExchange += frPegShiftConsume;
    frPegShift -= frPegShiftConsume;
}

static void consumereservepegshift(CFractions & frBalance, 
                                   CFractions & frExchange, 
                                   CFractions & frPegShift,
                                   const CPegLevel & peglevel) 
{
    CFractions frPegShiftReserve = frPegShift.LowPart(peglevel, nullptr);
    consumepegshift(frBalance, frExchange, frPegShift, frPegShiftReserve);
}

static void consumeliquidpegshift(CFractions & frBalance, 
                                  CFractions & frExchange, 
                                  CFractions & frPegShift,
                                  const CPegLevel & peglevel) 
{
    CFractions frPegShiftLiquid = frPegShift.HighPart(peglevel, nullptr);
    consumepegshift(frBalance, frExchange, frPegShift, frPegShiftLiquid);
}

// API calls

Value getpeglevel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "getpeglevel "
                "<exchange_pegdata_base64> "
                "<pegshift_pegdata_base64> "
                "<previous_cycle>\n"
            );
    
    string exchange_pegdata64 = params[0].get_str();
    string pegshift_pegdata64 = params[1].get_str();
    int nCyclePrev = params[2].get_int();
    
    CFractions frExchange(0, CFractions::VALUE);
    CFractions frPegShift(0, CFractions::VALUE);

    unpackpegdata(frExchange, exchange_pegdata64, "exchange");
    unpackpegdata(frPegShift, pegshift_pegdata64, "pegshift");
    
    frExchange = frExchange.Std();
    frPegShift = frPegShift.Std();

    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    // exchange peglevel
    CPegLevel peglevel(nCycleNow,
                       nCyclePrev,
                       nSupplyNow+3,
                       nSupplyNext+3,
                       nSupplyNextNext+3,
                       frExchange,
                       frPegShift);

//    CPegLevel peglevel(nCycleNow,
//                       nCyclePrev,
//                       nSupplyNow,
//                       nSupplyNext,
//                       nSupplyNextNext);
    
    // network peglevel
    CPegLevel peglevel_net(nCycleNow,
                           nCyclePrev,
                           nSupplyNow,
                           nSupplyNext,
                           nSupplyNextNext);
    
    CFractions frPegPool = frExchange.HighPart(peglevel, nullptr);
    
    Object result;
    result.push_back(Pair("cycle", peglevel.nCycle));

    printpeglevel(peglevel, result);
    printpegbalance(frPegPool, peglevel_net, result, "pegpool_", true);
    printpegbalance(frExchange, peglevel_net, result, "exchange_", false);
    printpegshift(frPegShift, peglevel_net, result, false);
    
    return result;
}

Value registerdeposit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "registerdeposit "
                "<txid:nout> "
                "<balance_pegdata_base64> "
                "<exchange_pegdata_base64> "
                "<peglevel_hex>\n"
            );
    
    string sTxout = params[0].get_str();
    vector<string> vTxoutArgs;
    boost::split(vTxoutArgs, sTxout, boost::is_any_of(":"));
    
    if (vTxoutArgs.size() != 2) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, format txid:nout");
    }
    
    string sTxid = vTxoutArgs[0];
    char * pEnd = nullptr;
    long nout = strtol(vTxoutArgs[1].c_str(), &pEnd, 0);
    if (pEnd == vTxoutArgs[1].c_str()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, nout is not number");
    }

    string balance_pegdata64 = params[1].get_str();
    string exchange_pegdata64 = params[2].get_str();
    
    CFractions frBalance(0, CFractions::VALUE);
    CFractions frExchange(0, CFractions::VALUE);

    unpackpegdata(frBalance, balance_pegdata64, "balance");
    unpackpegdata(frExchange, exchange_pegdata64, "exchange");

    frBalance = frBalance.Std();
    frExchange = frExchange.Std();
    
    string peglevel_hex = params[3].get_str();
    
    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    CPegLevel peglevel(nCycleNow,
                       nCycleNow-1,
                       nSupplyNow,
                       nSupplyNext,
                       nSupplyNextNext);
    // network peglevel
    CPegLevel peglevel_net = peglevel;
    
    if (!peglevel_hex.empty()) {
        CPegLevel peglevel_load(peglevel_hex);
        if (peglevel_load.IsValid()) {
            peglevel = peglevel_load;
        }
    }
    
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
    frExchange += frDeposit;
    
    result.push_back(Pair("deposited", true));
    result.push_back(Pair("status", "Registered"));
    result.push_back(Pair("atblock", nRegisterHeight));
    
    result.push_back(Pair("cycle", nCycleNow));
    
    printpeglevel(peglevel, result);
    printpegbalance(frBalance, peglevel, result, "balance_", true);
    printpegbalance(frExchange, peglevel_net, result, "exchange_", true);
    
    return result;
}

Value updatepegbalances(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 3))
        throw runtime_error(
            "updatepegbalances "
                "<balance_pegdata_base64> "
                "<pegpool_pegdata_base64> "
                "<peglevel_hex>\n"
            );
    
    string balance_pegdata64 = params[0].get_str();
    string pegpool_pegdata64 = params[1].get_str();
    string peglevel_hex = params[2].get_str();
    
    CPegLevel peglevel_old("");
    CPegLevel peglevel_new(peglevel_hex);
    if (!peglevel_new.IsValid()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Can not unpack peglevel");
    }
    
    CFractions frBalance(0, CFractions::VALUE);
    CFractions frPegPool(0, CFractions::VALUE);
    unpackpegdata(frPegPool, pegpool_pegdata64, "pegpool");
    unpackbalance(frBalance, peglevel_old, balance_pegdata64, "balance");

    frBalance = frBalance.Std();
    frPegPool = frPegPool.Std();
    
    if (peglevel_old.nCycle >= peglevel_new.nCycle) { // already up-to-dated
        
        Object result;
    
        result.push_back(Pair("completed", true));
        result.push_back(Pair("cycle", peglevel_old.nCycle));
        
        printpeglevel(peglevel_old, result);
        printpegbalance(frBalance, peglevel_old, result, "balance_", true);
        printpegbalance(frPegPool, peglevel_old, result, "pegpool_", true);
        
        return result;
    }
    
    if (peglevel_old.nCycle != 0 && 
        peglevel_old.nCycle != peglevel_new.nCyclePrev) {
        Object result;
        result.push_back(Pair("completed", false));
        result.push_back(Pair("cycle_old", peglevel_old.nCycle));
        result.push_back(Pair("cycle_new", peglevel_new.nCycle));
        result.push_back(Pair("status", strprintf(
            "Cycles mismatch for peglevel_new.nCyclePrev:%d vs peglevel_old.nCycle:%d", 
            peglevel_new.nCyclePrev,
            peglevel_old.nCycle)));
        return result;
    }
    
    int64_t nValue = frBalance.Total();
    
    CFractions frLiquid(0, CFractions::STD);
    CFractions frReserve(0, CFractions::STD);
    
    int64_t nReserve = 0;
    // current part of balance turns to reserve
    // the balance is to be updated at previous cycle
    frReserve = frBalance.LowPart(peglevel_new, &nReserve);
    
    if (nReserve != frReserve.Total()) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Reserve mimatch on LowPart %d vs %d",
                                     frReserve.Total(),
                                     nValue));
    }
    
    // liquid is just normed to pool
    int64_t nLiquid = nValue - nReserve;
    if (nLiquid > frPegPool.Total()) {
        // exchange liquidity mismatch
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on 'pool' to balance %d",
                                     frPegPool.Total(),
                                     nLiquid));
    }
    frLiquid = CFractions(0, CFractions::STD);
    frPegPool.MoveRatioPartTo(nLiquid, frLiquid);
    
    if (nLiquid != frLiquid.Total()) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Liquid mimatch on MoveRatioPartTo %d vs %d",
                                     frLiquid.Total(),
                                     nValue));
    }
    
    frBalance = frReserve + frLiquid;
    
    if (nValue != frBalance.Total()) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Balance mimatch after update %d vs %d",
                                     frBalance.Total(),
                                     nValue));
    }
    
    Object result;

    result.push_back(Pair("completed", true));
    result.push_back(Pair("cycle", peglevel_new.nCycle));
    
    printpeglevel(peglevel_new, result);
    printpegbalance(frBalance, peglevel_new, result, "balance_", true);
    printpegbalance(frPegPool, peglevel_new, result, "pegpool_", true);
    
    return result;
}

Value movecoins(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "movecoins "
                "<amount> "
                "<src_pegdata_base64> "
                "<dst_pegdata_base64> "
                "<peglevel_hex>\n"
            );
    
    int64_t move_amount = params[0].get_int64();
    string src_pegdata64 = params[1].get_str();
    string dst_pegdata64 = params[2].get_str();
    string peglevel_hex = params[3].get_str();
    
    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    CPegLevel peglevel(nCycleNow,
                       nCycleNow-1,
                       nSupplyNow,
                       nSupplyNext,
                       nSupplyNextNext);
    
    if (!peglevel_hex.empty()) {
        CPegLevel peglevel_load(peglevel_hex);
        if (peglevel_load.IsValid()) {
            peglevel = peglevel_load;
        }
    }
    
    CPegLevel peglevel_src = peglevel;
    CFractions frSrc(0, CFractions::VALUE);
    unpackbalance(frSrc, peglevel_src, src_pegdata64, "src");
    
    if (peglevel != peglevel_src) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Outdated 'src' of cycle of %d, current %d",
                                     peglevel_src.nCycle,
                                     peglevel.nCycle));
    }
    
    int64_t src_total = frSrc.Total();
    if (src_total < move_amount) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough amount %d on 'src' to move %d",
                                     src_total,
                                     move_amount));
    }

    CPegLevel peglevel_dst = peglevel;
    CFractions frDst(0, CFractions::VALUE);
    unpackbalance(frDst, peglevel_dst, dst_pegdata64, "dst");
    
    if (peglevel != peglevel_dst) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Outdated 'dst' of cycle of %d, current %d",
                                     peglevel_dst.nCycle,
                                     peglevel.nCycle));
    }
    
    frSrc = frSrc.Std();
    frDst = frDst.Std();
    
    CFractions frAmount = frSrc;
    CFractions frMove = frAmount.RatioPart(move_amount);
    
    frSrc -= frMove;
    frDst += frMove;
    
    Object result;
    
    result.push_back(Pair("cycle", peglevel.nCycle));
    
    printpeglevel(peglevel, result);
    printpegbalance(frSrc, peglevel, result, "src_", true);
    printpegbalance(frDst, peglevel, result, "dst_", true);
    
    return result;
}

Value moveliquid(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "moveliquid "
                "<liquid> "
                "<src_pegdata_base64> "
                "<dst_pegdata_base64> "
                "<peglevel_hex>\n"
            );
    
    int64_t move_liquid = params[0].get_int64();
    string src_pegdata64 = params[1].get_str();
    string dst_pegdata64 = params[2].get_str();
    string peglevel_hex = params[3].get_str();
    
    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    CPegLevel peglevel(nCycleNow,
                       nCycleNow-1,
                       nSupplyNow,
                       nSupplyNext,
                       nSupplyNextNext);
    
    if (!peglevel_hex.empty()) {
        CPegLevel peglevel_load(peglevel_hex);
        if (peglevel_load.IsValid()) {
            peglevel = peglevel_load;
        }
    }
    
    CPegLevel peglevel_src = peglevel;
    CFractions frSrc(0, CFractions::VALUE);
    unpackbalance(frSrc, peglevel_src, src_pegdata64, "src");

    if (peglevel != peglevel_src) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Outdated 'src' of cycle of %d, current %d",
                                     peglevel_src.nCycle,
                                     peglevel.nCycle));
    }
    
    int64_t src_liquid = frSrc.High(peglevel);
    if (src_liquid < move_liquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on 'src' to move %d",
                                     src_liquid,
                                     move_liquid));
    }

    CPegLevel peglevel_dst = peglevel;
    CFractions frDst(0, CFractions::VALUE);
    unpackbalance(frDst, peglevel_dst, dst_pegdata64, "dst");
    
    if (peglevel != peglevel_dst) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Outdated 'dst' of cycle of %d, current %d",
                                     peglevel_dst.nCycle,
                                     peglevel.nCycle));
    }
    
    frSrc = frSrc.Std();
    frDst = frDst.Std();
    
    CFractions frLiquid = frSrc.HighPart(peglevel, nullptr);
    CFractions frMove = frLiquid.RatioPart(move_liquid);
    
    frSrc -= frMove;
    frDst += frMove;
    
    Object result;
    
    result.push_back(Pair("cycle", peglevel.nCycle));
    
    printpeglevel(peglevel, result);
    printpegbalance(frSrc, peglevel, result, "src_", true);
    printpegbalance(frDst, peglevel, result, "dst_", true);
    
    return result;
}

Value movereserve(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "movereserve "
                "<reserve> "
                "<src_pegdata_base64> "
                "<dst_pegdata_base64> "
                "<peglevel_hex>\n"
            );
    
    int64_t move_reserve = params[0].get_int64();
    string src_pegdata64 = params[1].get_str();
    string dst_pegdata64 = params[2].get_str();
    string peglevel_hex = params[3].get_str();
    
    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    CPegLevel peglevel(nCycleNow,
                       nCycleNow-1,
                       nSupplyNow,
                       nSupplyNext,
                       nSupplyNextNext);
    
    if (!peglevel_hex.empty()) {
        CPegLevel peglevel_load(peglevel_hex);
        if (peglevel_load.IsValid()) {
            peglevel = peglevel_load;
        }
    }
    
    CPegLevel peglevel_src = peglevel;
    CFractions frSrc(0, CFractions::VALUE);
    unpackbalance(frSrc, peglevel_src, src_pegdata64, "src");

    if (peglevel != peglevel_src) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Outdated 'src' of cycle of %d, current %d",
                                     peglevel_src.nCycle,
                                     peglevel.nCycle));
    }
    
    int64_t src_reserve = frSrc.Low(peglevel);
    if (src_reserve < move_reserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough reserve %d on 'src' to move %d",
                                     src_reserve,
                                     move_reserve));
    }

    CPegLevel peglevel_dst = peglevel;
    CFractions frDst(0, CFractions::VALUE);
    unpackbalance(frDst, peglevel_dst, dst_pegdata64, "dst");

    if (peglevel != peglevel_dst) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Outdated 'dst' of cycle of %d, current %d",
                                     peglevel_dst.nCycle,
                                     peglevel.nCycle));
    }
    
    frSrc = frSrc.Std();
    frDst = frDst.Std();
    
    CFractions frReserve = frSrc.LowPart(peglevel, nullptr);
    CFractions frMove = frReserve.RatioPart(move_reserve);
    
    frSrc -= frMove;
    frDst += frMove;
    
    Object result;
    
    result.push_back(Pair("cycle", peglevel.nCycle));
    
    printpeglevel(peglevel, result);
    printpegbalance(frSrc, peglevel, result, "src_", true);
    printpegbalance(frDst, peglevel, result, "dst_", true);
    
    return result;
}

class CCoinToUse
{
public:
    uint256     txhash;
    uint64_t    i;
    int64_t     nValue;
    int64_t     nAvailableValue;
    CScript     scriptPubKey;
    int         nCycle;

    CCoinToUse() : i(0),nValue(0),nAvailableValue(0),nCycle(0) {}
    
    friend bool operator<(const CCoinToUse &a, const CCoinToUse &b) { 
        if (a.txhash < b.txhash) return true;
        if (a.txhash == b.txhash && a.i < b.i) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue < b.nValue) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue == b.nValue && a.nAvailableValue < b.nAvailableValue) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue == b.nValue && a.nAvailableValue == b.nAvailableValue && a.scriptPubKey < b.scriptPubKey) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue == b.nValue && a.nAvailableValue == b.nAvailableValue && a.scriptPubKey == b.scriptPubKey && a.nCycle < b.nCycle) return true;
        return false;
    }
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(txhash);
        READWRITE(i);
        READWRITE(nValue);
        READWRITE(scriptPubKey);
        READWRITE(nCycle);
    )
};

static bool sortByAddress(const CCoinToUse &lhs, const CCoinToUse &rhs) { 
    CScript lhs_script = lhs.scriptPubKey;
    CScript rhs_script = rhs.scriptPubKey;
    
    CTxDestination lhs_dst;
    CTxDestination rhs_dst;
    bool lhs_ok1 = ExtractDestination(lhs_script, lhs_dst);
    bool rhs_ok1 = ExtractDestination(rhs_script, rhs_dst);
    
    if (!lhs_ok1 || !rhs_ok1) {
        if (lhs_ok1 == rhs_ok1) 
            return lhs_script < rhs_script;
        return lhs_ok1 < rhs_ok1;
    }
    
    string lhs_addr = CBitcoinAddress(lhs_dst).ToString();
    string rhs_addr = CBitcoinAddress(rhs_dst).ToString();
    
    return lhs_addr < rhs_addr;
}

static bool sortByDestination(const CTxDestination &lhs, const CTxDestination &rhs) { 
    string lhs_addr = CBitcoinAddress(lhs).ToString();
    string rhs_addr = CBitcoinAddress(rhs).ToString();
    return lhs_addr < rhs_addr;
}

Value prepareliquidwithdraw(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 8)
        throw runtime_error(
            "prepareliquidwithdraw "
                "<balance_pegdata_base64> "
                "<exchange_pegdata_base64> "
                "<pegshift_pegdata_base64> "
                "<amount_with_fee> "
                "<address> "
                "<peglevel_hex> "
                "<consumed_inputs> "
                "<provided_outputs>\n"
            );
    
    string balance_pegdata64 = params[0].get_str();
    string exchange_pegdata64 = params[1].get_str();
    string pegshift_pegdata64 = params[2].get_str();
    int64_t nAmountWithFee = params[3].get_int64();

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid BitBay address");
    
    string peglevel_hex = params[5].get_str();
    
    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    // exchange peglevel
    CPegLevel peglevel(nCycleNow,
                       nCycleNow-1,
                       nSupplyNow,
                       nSupplyNext,
                       nSupplyNextNext);
    // network peglevel
    CPegLevel peglevel_net = peglevel;
    
    if (!peglevel_hex.empty()) {
        CPegLevel peglevel_load(peglevel_hex);
        if (peglevel_load.IsValid()) {
            peglevel = peglevel_load;
        }
    }
    
    CFractions frBalance(0, CFractions::VALUE);
    CFractions frExchange(0, CFractions::VALUE);
    CFractions frPegShift(0, CFractions::VALUE);
    
    unpackpegdata(frBalance, balance_pegdata64, "balance");
    unpackpegdata(frExchange, exchange_pegdata64, "exchange");
    unpackpegdata(frPegShift, pegshift_pegdata64, "pegshift");

    frBalance = frBalance.Std();
    frExchange = frExchange.Std();
    frPegShift = frPegShift.Std();
    
    int64_t nBalanceLiquid = 0;
    CFractions frBalanceLiquid = frBalance.HighPart(peglevel, &nBalanceLiquid);
    if (nAmountWithFee > nBalanceLiquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on 'balance' to withdraw %d",
                                     nBalanceLiquid,
                                     nAmountWithFee));
    }
    CFractions frAmount = frBalanceLiquid.RatioPart(nAmountWithFee);

    // inputs, outputs
    string sConsumedInputs = params[6].get_str();
    string sProvidedOutputs = params[7].get_str();

    set<uint320> setAllOutputs;
    set<uint320> setConsumedInputs;
    map<uint320,CCoinToUse> mapProvidedOutputs;
    vector<string> vConsumedInputsArgs;
    vector<string> vProvidedOutputsArgs;
    boost::split(vConsumedInputsArgs, sConsumedInputs, boost::is_any_of(","));
    boost::split(vProvidedOutputsArgs, sProvidedOutputs, boost::is_any_of(","));
    for(string sConsumedInput : vConsumedInputsArgs) {
        setConsumedInputs.insert(uint320(sConsumedInput));
    }
    for(string sProvidedOutput : vProvidedOutputsArgs) {
        vector<unsigned char> outData(ParseHex(sProvidedOutput));
        CDataStream ssData(outData, SER_NETWORK, PROTOCOL_VERSION);
        CCoinToUse out;
        try { ssData >> out; }
        catch (std::exception &) { continue; }
        if (out.nCycle != nCycleNow) { continue; }
        auto fkey = uint320(out.txhash, out.i);
        if (setConsumedInputs.count(fkey)) { continue; }
        mapProvidedOutputs[fkey] = out;
        setAllOutputs.insert(fkey);
    }
    
    if (!pindexBest) {
        throw JSONRPCError(RPC_MISC_ERROR, "Blockchain is not in sync");
    }
    
    assert(pwalletMain != NULL);
   
    CTxDB txdb("r");
    CPegDB pegdb("r");
    
    // make list of 'rated' outputs, multimap with key 'distortion'
    // they are rated to be less distorted towards coins to withdraw
    
    map<uint320,CCoinToUse> mapAllOutputs = mapProvidedOutputs;
    set<uint320> setWalletOutputs;
    
    map<uint320,int64_t> mapAvailableLiquid;
    
    vector<COutput> vecCoins;
    pwalletMain->AvailableCoins(vecCoins, false, true, NULL);
    for(const COutput& coin : vecCoins)
    {
        auto txhash = coin.tx->GetHash();
        auto fkey = uint320(txhash, coin.i);
        setAllOutputs.insert(fkey);
        setWalletOutputs.insert(fkey);
        if (setConsumedInputs.count(fkey)) continue; // already used
        CCoinToUse & out = mapAllOutputs[fkey];
        out.i = coin.i;
        out.txhash = txhash;
        out.nValue = coin.tx->vout[coin.i].nValue;
        out.scriptPubKey = coin.tx->vout[coin.i].scriptPubKey;
        out.nCycle = nCycleNow;
    }
    
    // clean-up consumed, intersect with (wallet+provided)
    set<uint320> setConsumedInputsNew;
    std::set_intersection(setConsumedInputs.begin(), setConsumedInputs.end(),
                          setAllOutputs.begin(), setAllOutputs.end(),
                          std::inserter(setConsumedInputsNew,setConsumedInputsNew.begin()));
    sConsumedInputs.clear();
    for(const uint320& fkey : setConsumedInputsNew) {
        if (!sConsumedInputs.empty()) sConsumedInputs += ",";
        sConsumedInputs += fkey.GetHex();
    }
    
    // clean-up provided, remove what is already in wallet
    {
        map<uint320,CCoinToUse> mapProvidedOutputsNew;
        for(const pair<uint320,CCoinToUse> & item : mapProvidedOutputs) {
            if (setWalletOutputs.count(item.first)) continue;
            mapProvidedOutputsNew.insert(item);
        }
        mapProvidedOutputs = mapProvidedOutputsNew;
    }
    
    // read avaialable coin fractions to rate
    // also consider only coins with are not less than 5% (20 inputs max)
    multimap<double,CCoinToUse> ratedOutputs;
    for(const pair<uint320,CCoinToUse>& item : mapAllOutputs) {
        uint320 fkey = item.first;
        CFractions frOut(0, CFractions::VALUE);
        if (!pegdb.ReadFractions(fkey, frOut, true)) {
            if (!mempool.lookup(fkey.b1(), fkey.b2(), frOut)) {
                continue;
            }
        }
        
        int64_t nAvailableLiquid = 0;
        frOut = frOut.HighPart(peglevel.nSupplyNext, &nAvailableLiquid);
        
        if (nAvailableLiquid < (nAmountWithFee / 20)) {
            continue;
        }
        
        double distortion = frOut.Distortion(frAmount);
        ratedOutputs.insert(pair<double,CCoinToUse>(distortion, item.second));
        mapAvailableLiquid[fkey] = nAvailableLiquid;
    }

    // get available value for selected coins
    set<CCoinToUse> setCoins;
    int64_t nLeftAmount = nAmountWithFee;
    auto it = ratedOutputs.begin();
    for (; it != ratedOutputs.end(); ++it) {
        CCoinToUse out = (*it).second;
        auto txhash = out.txhash;
        auto fkey = uint320(txhash, out.i);
        
        nLeftAmount -= mapAvailableLiquid[fkey];
        out.nAvailableValue = mapAvailableLiquid[fkey];
        setCoins.insert(out);
        
        if (nLeftAmount <= 0) {
            break;
        }
    }
    
    if (nLeftAmount > 0) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid or coins are too fragmented  on 'exchange' to withdraw %d",
                                     nAmountWithFee));
    }
    
    int64_t nFeeRet = 1000000 /*temp fee*/;
    int64_t nAmount = nAmountWithFee - nFeeRet;
    
    vector<pair<CScript, int64_t> > vecSend;
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address.Get());
    vecSend.push_back(make_pair(scriptPubKey, nAmount));
    
    int64_t nValue = 0;
    for(const pair<CScript, int64_t>& s : vecSend) {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    
    size_t nNumInputs = 1;

    CTransaction rawTx;
    
    nNumInputs = setCoins.size();
    if (!nNumInputs) return false;
    
    // Inputs to be sorted by address
    vector<CCoinToUse> vCoins;
    for(const CCoinToUse& coin : setCoins) {
        vCoins.push_back(coin);
    }
    sort(vCoins.begin(), vCoins.end(), sortByAddress);
    
    // Collect input addresses
    // Prepare maps for input,available,take
    set<CTxDestination> setInputAddresses;
    vector<CTxDestination> vInputAddresses;
    map<CTxDestination, int64_t> mapAvailableValuesAt;
    map<CTxDestination, int64_t> mapInputValuesAt;
    map<CTxDestination, int64_t> mapTakeValuesAt;
    int64_t nValueToTakeFromChange = 0;
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        setInputAddresses.insert(address); // sorted due to vCoins
        mapAvailableValuesAt[address] = 0;
        mapInputValuesAt[address] = 0;
        mapTakeValuesAt[address] = 0;
    }
    // Get sorted list of input addresses
    for(const CTxDestination& address : setInputAddresses) {
        vInputAddresses.push_back(address);
    }
    sort(vInputAddresses.begin(), vInputAddresses.end(), sortByDestination);
    // Input and available values can be filled in
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        int64_t& nValueAvailableAt = mapAvailableValuesAt[address];
        nValueAvailableAt += coin.nAvailableValue;
        int64_t& nValueInputAt = mapInputValuesAt[address];
        nValueInputAt += coin.nValue;
    }
            
    // vouts to the payees
    for(const pair<CScript, int64_t>& s : vecSend) {
        rawTx.vout.push_back(CTxOut(s.second, s.first));
    }
    
    CReserveKey reservekey(pwalletMain);
    reservekey.ReturnKey();

    // Available values - liquidity
    // Compute values to take from each address (liquidity is common)
    int64_t nValueLeft = nValue;
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        int64_t nValueAvailable = coin.nAvailableValue;
        int64_t nValueTake = nValueAvailable;
        if (nValueTake > nValueLeft) {
            nValueTake = nValueLeft;
        }
        int64_t& nValueTakeAt = mapTakeValuesAt[address];
        nValueTakeAt += nValueTake;
        nValueLeft -= nValueTake;
    }
    
    // Calculate change (minus fee and part taken from change)
    int64_t nTakeFromChangeLeft = nValueToTakeFromChange + nFeeRet;
    for (const CTxDestination& address : vInputAddresses) {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address);
        int64_t nValueTake = mapTakeValuesAt[address];
        int64_t nValueInput = mapInputValuesAt[address];
        int64_t nValueChange = nValueInput - nValueTake;
        if (nValueChange > nTakeFromChangeLeft) {
            nValueChange -= nTakeFromChangeLeft;
            nTakeFromChangeLeft = 0;
        }
        if (nValueChange < nTakeFromChangeLeft) {
            nTakeFromChangeLeft -= nValueChange;
            nValueChange = 0;
        }
        if (nValueChange == 0) continue;
        rawTx.vout.push_back(CTxOut(nValueChange, scriptPubKey));
    }
    
    // Fill vin
    for(const CCoinToUse& coin : vCoins) {
        rawTx.vin.push_back(CTxIn(coin.txhash,coin.i));
    }
    
    // Calculate peg
    MapPrevOut mapInputs;
    MapPrevTx mapTxInputs;
    MapFractions mapInputsFractions;
    MapFractions mapOutputFractions;
    CFractions feesFractions(0, CFractions::STD);
    string sPegFailCause;
    
    size_t n_vin = rawTx.vin.size();
        
    for (unsigned int i = 0; i < n_vin; i++)
    {
        const COutPoint & prevout = rawTx.vin[i].prevout;
        auto fkey = uint320(prevout.hash, prevout.n);
        
        if (mapAllOutputs.count(fkey)) {
            const CCoinToUse& coin = mapAllOutputs[fkey];
            CTxOut out(coin.nValue, coin.scriptPubKey);
            mapInputs[fkey] = out;
        }
        else {
            // Read txindex
            CTxIndex& txindex = mapTxInputs[prevout.hash].first;
            if (!txdb.ReadTxIndex(prevout.hash, txindex)) {
                continue;
            }
            // Read txPrev
            CTransaction& txPrev = mapTxInputs[prevout.hash].second;
            if (!txPrev.ReadFromDisk(txindex.pos)) {
                continue;
            }  
            
            if (prevout.n >= txPrev.vout.size()) {
                continue;
            }
            
            mapInputs[fkey] = txPrev.vout[prevout.n];
        }
        
        CFractions& fractions = mapInputsFractions[fkey];
        fractions = CFractions(mapInputs[fkey].nValue, CFractions::VALUE);
        pegdb.ReadFractions(fkey, fractions);
    }
    
    // signing the transaction to get it ready for broadcast
    int nIn = 0;
    for(const CCoinToUse& coin : vCoins) {
        if (!SignSignature(*pwalletMain, coin.scriptPubKey, rawTx, nIn++)) {
            throw JSONRPCError(RPC_MISC_ERROR, 
                               strprintf("Fail on signing input (%d)", nIn-1));
        }
    }
    
    bool peg_ok = CalculateStandardFractions(rawTx, 
                                             peglevel.nSupplyNext,
                                             pindexBest->nTime,
                                             mapInputs, 
                                             mapInputsFractions,
                                             mapOutputFractions,
                                             feesFractions,
                                             sPegFailCause);
    if (!peg_ok) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Fail on calculations of tx fractions (cause=%s)",
                                     sPegFailCause.c_str()));
    }
        
    string txhash = rawTx.GetHash().GetHex();
    
    // without payment is the first out
    auto fkey = uint320(rawTx.GetHash(), 0);
    if (!mapOutputFractions.count(fkey)) {
        throw JSONRPCError(RPC_MISC_ERROR, "No withdraw fractions");
    }
    CFractions frProcessed = mapOutputFractions[fkey] + feesFractions;
    CFractions frRequested = frAmount;

    if (frRequested.Total() != nAmountWithFee) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch requested and amount_with_fee (%d - %d)",
                                     frRequested.Total(), nAmountWithFee));
    }
    if (frProcessed.Total() != nAmountWithFee) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch processed and amount_with_fee (%d - %d)",
                                     frProcessed.Total(), nAmountWithFee));
    }
    
    // get list of consumed inputs
    for (size_t i=0; i< rawTx.vin.size(); i++) {
        const COutPoint & prevout = rawTx.vin[i].prevout;
        auto fkey = uint320(prevout.hash, prevout.n);
        if (mapProvidedOutputs.count(fkey)) mapProvidedOutputs.erase(fkey);
        if (!sConsumedInputs.empty()) sConsumedInputs += ",";
        sConsumedInputs += fkey.GetHex();
    }
    
    // get list of provided outputs and save fractions
    sProvidedOutputs.clear();
    for(const pair<uint320,CCoinToUse> & item : mapProvidedOutputs) {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << item.second;
        if (!sProvidedOutputs.empty()) sProvidedOutputs += ",";
        sProvidedOutputs += HexStr(ss.begin(), ss.end());
    }
    // get list of changes and add to current provided outputs
    map<string, int64_t> mapTxChanges;
    {
        CPegDB pegdbrw;
        for (size_t i=1; i< rawTx.vout.size(); i++) { // skip 0 (withdraw)
            // make map of change outputs
            string txout = rawTx.GetHash().GetHex()+":"+itostr(i);
            mapTxChanges[txout] = rawTx.vout[i].nValue;
            // save these outputs in pegdb, so they can be used in next withdraws
            auto fkey = uint320(rawTx.GetHash(), i);
            pegdbrw.WriteFractions(fkey, mapOutputFractions[fkey]);
            // serialize to sProvidedOutputs
            CCoinToUse out;
            out.i = i;
            out.txhash = rawTx.GetHash();
            out.nValue = rawTx.vout[i].nValue;
            out.scriptPubKey = rawTx.vout[i].scriptPubKey;
            out.nCycle = nCycleNow;
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << out;
            if (!sProvidedOutputs.empty()) sProvidedOutputs += ",";
            sProvidedOutputs += HexStr(ss.begin(), ss.end());
        }
    }
    
    frBalance -= frRequested;
    frExchange -= frRequested;
    frPegShift += (frRequested - frProcessed);
    
    // consume liquid part of pegshift by balance
    // as computation were completed by pegnext it may use fractions
    // of current reserves - at current supply not to consume these fractions
    consumeliquidpegshift(frBalance, frExchange, frPegShift, peglevel);
    
    if (frPegShift.Positive(nullptr).Total() != -frPegShift.Negative(nullptr).Total()) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch pegshift parts (%d - %d)",
                                     frPegShift.Positive(nullptr).Total(), 
                                     frPegShift.Negative(nullptr).Total()));
    }
    
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    
    string txstr = HexStr(ss.begin(), ss.end());
    
    Object result;
    result.push_back(Pair("completed", true));
    result.push_back(Pair("txhash", txhash));
    result.push_back(Pair("rawtx", txstr));

    result.push_back(Pair("consumed_inputs", sConsumedInputs));
    result.push_back(Pair("provided_outputs", sProvidedOutputs));

    result.push_back(Pair("created_on_peg", peglevel_net.nSupply));
    result.push_back(Pair("broadcast_on_peg", peglevel_net.nSupplyNext));
    
    printpegbalance(frBalance, peglevel, result, "balance_", true);
    printpegbalance(frProcessed, peglevel, result, "processed_", true);
    printpegbalance(frExchange, peglevel_net, result, "exchange_", true);
    
    printpegshift(frPegShift, peglevel_net, result, true);
    
    Array changes;
    for(const pair<string, int64_t> & item : mapTxChanges) {
        Object obj;
        obj.push_back(Pair("txout", item.first));
        obj.push_back(Pair("amount", item.second));
        changes.push_back(obj);
    }
    result.push_back(Pair("changes", changes));
    
    return result;
}

Value preparereservewithdraw(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 8)
        throw runtime_error(
            "preparereservewithdraw "
                "<balance_pegdata_base64> "
                "<exchange_pegdata_base64> "
                "<pegshift_pegdata_base64> "
                "<amount_with_fee> "
                "<address> "
                "<peglevel_hex> "
                "<consumed_inputs> "
                "<provided_outputs>\n"
            );
    
    string balance_pegdata64 = params[0].get_str();
    string exchange_pegdata64 = params[1].get_str();
    string pegshift_pegdata64 = params[2].get_str();
    int64_t nAmountWithFee = params[3].get_int64();

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid BitBay address");
    
    string peglevel_hex = params[5].get_str();
    
    int nSupplyNow = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    int nSupplyNextNext = pindexBest ? pindexBest->GetNextNextIntervalPegSupplyIndex() : 0;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;
    
    // exchange peglevel
    CPegLevel peglevel(nCycleNow,
                       nCycleNow-1,
                       nSupplyNow,
                       nSupplyNext,
                       nSupplyNextNext);
    // network peglevel
    CPegLevel peglevel_net = peglevel;
    
    if (!peglevel_hex.empty()) {
        CPegLevel peglevel_load(peglevel_hex);
        if (peglevel_load.IsValid()) {
            peglevel = peglevel_load;
        }
    }
    
    CFractions frBalance(0, CFractions::VALUE);
    CFractions frExchange(0, CFractions::VALUE);
    CFractions frPegShift(0, CFractions::VALUE);
    
    unpackpegdata(frBalance, balance_pegdata64, "balance");
    unpackpegdata(frExchange, exchange_pegdata64, "exchange");
    unpackpegdata(frPegShift, pegshift_pegdata64, "pegshift");

    frBalance = frBalance.Std();
    frExchange = frExchange.Std();
    frPegShift = frPegShift.Std();
    
    int64_t nBalanceReserve = 0;
    CFractions frBalanceReserve = frBalance.LowPart(peglevel.nSupplyNext, &nBalanceReserve);
    if (nAmountWithFee > nBalanceReserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough reserve %d on 'balance' to withdraw %d",
                                     nBalanceReserve,
                                     nAmountWithFee));
    }
    CFractions frAmount = frBalanceReserve.RatioPart(nAmountWithFee);

    // inputs, outputs
    string sConsumedInputs = params[6].get_str();
    string sProvidedOutputs = params[7].get_str();

    set<uint320> setAllOutputs;
    set<uint320> setConsumedInputs;
    map<uint320,CCoinToUse> mapProvidedOutputs;
    vector<string> vConsumedInputsArgs;
    vector<string> vProvidedOutputsArgs;
    boost::split(vConsumedInputsArgs, sConsumedInputs, boost::is_any_of(","));
    boost::split(vProvidedOutputsArgs, sProvidedOutputs, boost::is_any_of(","));
    for(string sConsumedInput : vConsumedInputsArgs) {
        setConsumedInputs.insert(uint320(sConsumedInput));
    }
    for(string sProvidedOutput : vProvidedOutputsArgs) {
        vector<unsigned char> outData(ParseHex(sProvidedOutput));
        CDataStream ssData(outData, SER_NETWORK, PROTOCOL_VERSION);
        CCoinToUse out;
        try { ssData >> out; }
        catch (std::exception &) { continue; }
        if (out.nCycle != nCycleNow) { continue; }
        auto fkey = uint320(out.txhash, out.i);
        if (setConsumedInputs.count(fkey)) { continue; }
        mapProvidedOutputs[fkey] = out;
        setAllOutputs.insert(fkey);
    }
    
    if (!pindexBest) {
        throw JSONRPCError(RPC_MISC_ERROR, "Blockchain is not in sync");
    }
    
    assert(pwalletMain != NULL);
   
    CTxDB txdb("r");
    CPegDB pegdb("r");
    
    // make list of 'rated' outputs, multimap with key 'distortion'
    // they are rated to be less distorted towards coins to withdraw
    
    map<uint320,CCoinToUse> mapAllOutputs = mapProvidedOutputs;
    set<uint320> setWalletOutputs;
        
    vector<COutput> vecCoins;
    pwalletMain->AvailableCoins(vecCoins, false, true, NULL);
    for(const COutput& coin : vecCoins)
    {
        auto txhash = coin.tx->GetHash();
        auto fkey = uint320(txhash, coin.i);
        setAllOutputs.insert(fkey);
        setWalletOutputs.insert(fkey);
        if (setConsumedInputs.count(fkey)) continue; // already used
        CCoinToUse & out = mapAllOutputs[fkey];
        out.i = coin.i;
        out.txhash = txhash;
        out.nValue = coin.tx->vout[coin.i].nValue;
        out.scriptPubKey = coin.tx->vout[coin.i].scriptPubKey;
        out.nCycle = nCycleNow;
    }
    
    // clean-up consumed, intersect with (wallet+provided)
    set<uint320> setConsumedInputsNew;
    std::set_intersection(setConsumedInputs.begin(), setConsumedInputs.end(),
                          setAllOutputs.begin(), setAllOutputs.end(),
                          std::inserter(setConsumedInputsNew,setConsumedInputsNew.begin()));
    sConsumedInputs.clear();
    for(const uint320& fkey : setConsumedInputsNew) {
        if (!sConsumedInputs.empty()) sConsumedInputs += ",";
        sConsumedInputs += fkey.GetHex();
    }
    
    // clean-up provided, remove what is already in wallet
    {
        map<uint320,CCoinToUse> mapProvidedOutputsNew;
        for(const pair<uint320,CCoinToUse> & item : mapProvidedOutputs) {
            if (setWalletOutputs.count(item.first)) continue;
            mapProvidedOutputsNew.insert(item);
        }
        mapProvidedOutputs = mapProvidedOutputsNew;
    }
    
    // read avaialable coin fractions to rate
    // also consider only coins with are not less than 10% (10 inputs max)
    map<uint320,int64_t> mapAvailableReserve;
    multimap<double,CCoinToUse> ratedOutputs;
    for(const pair<uint320,CCoinToUse>& item : mapAllOutputs) {
        uint320 fkey = item.first;
        CFractions frOut(0, CFractions::VALUE);
        if (!pegdb.ReadFractions(fkey, frOut, true)) {
            if (!mempool.lookup(fkey.b1(), fkey.b2(), frOut)) {
                continue;
            }
        }
        
        int64_t nAvailableReserve = 0;
        frOut = frOut.LowPart(peglevel.nSupplyNext, &nAvailableReserve);
        
        if (nAvailableReserve < (nAmountWithFee / 20)) {
            continue;
        }
        
        double distortion = frOut.Distortion(frAmount);
        ratedOutputs.insert(pair<double,CCoinToUse>(distortion, item.second));
        mapAvailableReserve[fkey] = nAvailableReserve;
    }

    // get available value for selected coins
    set<CCoinToUse> setCoins;
    int64_t nLeftAmount = nAmountWithFee;
    auto it = ratedOutputs.begin();
    for (; it != ratedOutputs.end(); ++it) {
        CCoinToUse out = (*it).second;
        auto txhash = out.txhash;
        auto fkey = uint320(txhash, out.i);
        
        nLeftAmount -= mapAvailableReserve[fkey];
        out.nAvailableValue = mapAvailableReserve[fkey];
        setCoins.insert(out);
        
        if (nLeftAmount <= 0) {
            break;
        }
    }
    
    if (nLeftAmount > 0) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough reserve or coins are too fragmented on 'exchange' to withdraw %d",
                                     nAmountWithFee));
    }
    
    int64_t nFeeRet = 1000000 /*temp fee*/;
    int64_t nAmount = nAmountWithFee - nFeeRet;
    
    vector<pair<CScript, int64_t> > vecSend;
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address.Get());
    vecSend.push_back(make_pair(scriptPubKey, nAmount));
    
    int64_t nValue = 0;
    for(const pair<CScript, int64_t>& s : vecSend) {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    
    size_t nNumInputs = 1;

    CTransaction rawTx;
    
    nNumInputs = setCoins.size();
    if (!nNumInputs) return false;
    
    // Inputs to be sorted by address
    vector<CCoinToUse> vCoins;
    for(const CCoinToUse& coin : setCoins) {
        vCoins.push_back(coin);
    }
    sort(vCoins.begin(), vCoins.end(), sortByAddress);
    
    // Collect input addresses
    // Prepare maps for input,available,take
    set<CTxDestination> setInputAddresses;
    vector<CTxDestination> vInputAddresses;
    map<CTxDestination, int64_t> mapAvailableValuesAt;
    map<CTxDestination, int64_t> mapInputValuesAt;
    map<CTxDestination, int64_t> mapTakeValuesAt;
    int64_t nValueToTakeFromChange = 0;
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        setInputAddresses.insert(address); // sorted due to vCoins
        mapAvailableValuesAt[address] = 0;
        mapInputValuesAt[address] = 0;
        mapTakeValuesAt[address] = 0;
    }
    // Get sorted list of input addresses
    for(const CTxDestination& address : setInputAddresses) {
        vInputAddresses.push_back(address);
    }
    sort(vInputAddresses.begin(), vInputAddresses.end(), sortByDestination);
    // Input and available values can be filled in
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        int64_t& nValueAvailableAt = mapAvailableValuesAt[address];
        nValueAvailableAt += coin.nAvailableValue;
        int64_t& nValueInputAt = mapInputValuesAt[address];
        nValueInputAt += coin.nValue;
    }
    
    // Notations for frozen **F**
    {
        // prepare indexes to freeze
        size_t nCoins = vCoins.size();
        size_t nPayees = vecSend.size();
        string out_indexes;
        if (nPayees == 1) { // trick to have triple to use sort
            auto out_index = std::to_string(0+nCoins);
            out_indexes = out_index+":"+out_index+":"+out_index;
        }
        else if (nPayees == 2) { // trick to have triple to use sort
            auto out_index1 = std::to_string(0+nCoins);
            auto out_index2 = std::to_string(1+nCoins);
            out_indexes = out_index1+":"+out_index1+":"+out_index2+":"+out_index2;
        }
        else {
            for(size_t i=0; i<nPayees; i++) {
                if (!out_indexes.empty())
                    out_indexes += ":";
                out_indexes += std::to_string(i+nCoins);
            }
        }
        // Fill vout with freezing instructions
        for(size_t i=0; i<nCoins; i++) {
            CScript scriptPubKey;
            scriptPubKey.push_back(OP_RETURN);
            unsigned char len_bytes = out_indexes.size();
            scriptPubKey.push_back(len_bytes+5);
            scriptPubKey.push_back('*');
            scriptPubKey.push_back('*');
            scriptPubKey.push_back('F');
            scriptPubKey.push_back('*');
            scriptPubKey.push_back('*');
            for (size_t j=0; j< out_indexes.size(); j++) {
                scriptPubKey.push_back(out_indexes[j]);
            }
            rawTx.vout.push_back(CTxOut(PEG_MAKETX_FREEZE_VALUE, scriptPubKey));
        }
        // Value for notary is first taken from reserves sorted by address
        int64_t nValueLeft = nCoins*PEG_MAKETX_FREEZE_VALUE;
        // take reserves in defined order
        for(const CTxDestination& address : vInputAddresses) {
            int64_t nValueAvailableAt = mapAvailableValuesAt[address];
            int64_t& nValueTakeAt = mapTakeValuesAt[address];
            int64_t nValueLeftAt = nValueAvailableAt-nValueTakeAt;
            if (nValueAvailableAt ==0) continue;
            int64_t nValueToTake = nValueLeft;
            if (nValueToTake > nValueLeftAt)
                nValueToTake = nValueLeftAt;
            
            nValueTakeAt += nValueToTake;
            nValueLeft -= nValueToTake;
            
            if (nValueLeft == 0) break;
        }
        // if nValueLeft is left - need to be taken from change (liquidity)
        nValueToTakeFromChange += nValueLeft;
    }
    
    // vouts to the payees
    for(const pair<CScript, int64_t>& s : vecSend) {
        rawTx.vout.push_back(CTxOut(s.second, s.first));
    }
    
    CReserveKey reservekey(pwalletMain);
    reservekey.ReturnKey();
    
    // Available values - reserves per address
    // vecSend - outputs to be frozen reserve parts
    
    // Prepare order of inputs
    // For **F** the first is referenced (last input) then others are sorted
    vector<CTxDestination> vAddressesForFrozen;
    CTxDestination addressFrozenRef = vInputAddresses.back();
    vAddressesForFrozen.push_back(addressFrozenRef);
    for(const CTxDestination & address : vInputAddresses) {
        if (address == addressFrozenRef) continue;
        vAddressesForFrozen.push_back(address);
    }
    
    // Follow outputs and compute taken values
    for(const pair<CScript, int64_t>& s : vecSend) {
        int64_t nValueLeft = s.second;
        // take reserves in defined order
        for(const CTxDestination& address : vAddressesForFrozen) {
            int64_t nValueAvailableAt = mapAvailableValuesAt[address];
            int64_t& nValueTakeAt = mapTakeValuesAt[address];
            int64_t nValueLeftAt = nValueAvailableAt-nValueTakeAt;
            if (nValueAvailableAt ==0) continue;
            int64_t nValueToTake = nValueLeft;
            if (nValueToTake > nValueLeftAt)
                nValueToTake = nValueLeftAt;

            nValueTakeAt += nValueToTake;
            nValueLeft -= nValueToTake;
            
            if (nValueLeft == 0) break;
        }
        // if nValueLeft is left then is taken from change (liquidity)
        nValueToTakeFromChange += nValueLeft;
    }
    
    // Calculate change (minus fee and part taken from change)
    int64_t nTakeFromChangeLeft = nValueToTakeFromChange + nFeeRet;
    for (const CTxDestination& address : vInputAddresses) {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address);
        int64_t nValueTake = mapTakeValuesAt[address];
        int64_t nValueInput = mapInputValuesAt[address];
        int64_t nValueChange = nValueInput - nValueTake;
        if (nValueChange > nTakeFromChangeLeft) {
            nValueChange -= nTakeFromChangeLeft;
            nTakeFromChangeLeft = 0;
        }
        if (nValueChange < nTakeFromChangeLeft) {
            nTakeFromChangeLeft -= nValueChange;
            nValueChange = 0;
        }
        if (nValueChange == 0) continue;
        rawTx.vout.push_back(CTxOut(nValueChange, scriptPubKey));
    }
    
    // Fill vin
    for(const CCoinToUse& coin : vCoins) {
        rawTx.vin.push_back(CTxIn(coin.txhash,coin.i));
    }
    
    // Calculate peg
    MapPrevOut mapInputs;
    MapPrevTx mapTxInputs;
    MapFractions mapInputsFractions;
    MapFractions mapOutputFractions;
    CFractions feesFractions(0, CFractions::STD);
    string sPegFailCause;
    
    size_t n_vin = rawTx.vin.size();
        
    for (unsigned int i = 0; i < n_vin; i++)
    {
        const COutPoint & prevout = rawTx.vin[i].prevout;
        auto fkey = uint320(prevout.hash, prevout.n);
        
        if (mapAllOutputs.count(fkey)) {
            const CCoinToUse& coin = mapAllOutputs[fkey];
            CTxOut out(coin.nValue, coin.scriptPubKey);
            mapInputs[fkey] = out;
        }
        else {
            // Read txindex
            CTxIndex& txindex = mapTxInputs[prevout.hash].first;
            if (!txdb.ReadTxIndex(prevout.hash, txindex)) {
                continue;
            }
            // Read txPrev
            CTransaction& txPrev = mapTxInputs[prevout.hash].second;
            if (!txPrev.ReadFromDisk(txindex.pos)) {
                continue;
            }  
            
            if (prevout.n >= txPrev.vout.size()) {
                continue;
            }
            
            mapInputs[fkey] = txPrev.vout[prevout.n];
        }
        
        CFractions& fractions = mapInputsFractions[fkey];
        fractions = CFractions(mapInputs[fkey].nValue, CFractions::VALUE);
        pegdb.ReadFractions(fkey, fractions);
    }
    
    // signing the transaction to get it ready for broadcast
    int nIn = 0;
    for(const CCoinToUse& coin : vCoins) {
        if (!SignSignature(*pwalletMain, coin.scriptPubKey, rawTx, nIn++)) {
            throw JSONRPCError(RPC_MISC_ERROR, 
                               strprintf("Fail on signing input (%d)", nIn-1));
        }
    }
    
    bool peg_ok = CalculateStandardFractions(rawTx, 
                                             peglevel.nSupplyNext,
                                             pindexBest->nTime,
                                             mapInputs, 
                                             mapInputsFractions,
                                             mapOutputFractions,
                                             feesFractions,
                                             sPegFailCause);
    if (!peg_ok) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Fail on calculations of tx fractions (cause=%s)",
                                     sPegFailCause.c_str()));
    }
        
    string txhash = rawTx.GetHash().GetHex();
    
    // without payment is the first out after F notations (same num as size inputs)
    auto fkey = uint320(rawTx.GetHash(), rawTx.vin.size());
    if (!mapOutputFractions.count(fkey)) {
        throw JSONRPCError(RPC_MISC_ERROR, "No withdraw fractions");
    }
    CFractions frProcessed = mapOutputFractions[fkey] + feesFractions;
    CFractions frRequested = frAmount;

    if (frRequested.Total() != nAmountWithFee) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch requested and amount_with_fee (%d - %d)",
                                     frRequested.Total(), nAmountWithFee));
    }
    if (frProcessed.Total() != nAmountWithFee) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch processed and amount_with_fee (%d - %d)",
                                     frProcessed.Total(), nAmountWithFee));
    }
    
    // get list of consumed inputs
    for (size_t i=0; i< rawTx.vin.size(); i++) {
        const COutPoint & prevout = rawTx.vin[i].prevout;
        auto fkey = uint320(prevout.hash, prevout.n);
        if (mapProvidedOutputs.count(fkey)) mapProvidedOutputs.erase(fkey);
        if (!sConsumedInputs.empty()) sConsumedInputs += ",";
        sConsumedInputs += fkey.GetHex();
    }
    
    // get list of provided outputs and save fractions
    sProvidedOutputs.clear();
    for(const pair<uint320,CCoinToUse> & item : mapProvidedOutputs) {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << item.second;
        if (!sProvidedOutputs.empty()) sProvidedOutputs += ",";
        sProvidedOutputs += HexStr(ss.begin(), ss.end());
    }
    // get list of changes and add to current provided outputs
    map<string, int64_t> mapTxChanges;
    {
        CPegDB pegdbrw;
        for (size_t i=rawTx.vin.size()+1; i< rawTx.vout.size(); i++) { // skip notations and withdraw
            // make map of change outputs
            string txout = rawTx.GetHash().GetHex()+":"+itostr(i);
            mapTxChanges[txout] = rawTx.vout[i].nValue;
            // save these outputs in pegdb, so they can be used in next withdraws
            auto fkey = uint320(rawTx.GetHash(), i);
            pegdbrw.WriteFractions(fkey, mapOutputFractions[fkey]);
            // serialize to sProvidedOutputs
            CCoinToUse out;
            out.i = i;
            out.txhash = rawTx.GetHash();
            out.nValue = rawTx.vout[i].nValue;
            out.scriptPubKey = rawTx.vout[i].scriptPubKey;
            out.nCycle = nCycleNow;
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << out;
            if (!sProvidedOutputs.empty()) sProvidedOutputs += ",";
            sProvidedOutputs += HexStr(ss.begin(), ss.end());
        }
    }
    
    frBalance -= frRequested;
    frExchange -= frRequested;
    frPegShift += (frRequested - frProcessed);
    
    // consume reserve part of pegshift by balance
    // as computation were completed by pegnext it may use fractions
    // of current liquid - at current supply not to consume these fractions
    consumereservepegshift(frBalance, frExchange, frPegShift, peglevel);
    
    if (frPegShift.Positive(nullptr).Total() != -frPegShift.Negative(nullptr).Total()) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch pegshift parts (%d - %d)",
                                     frPegShift.Positive(nullptr).Total(), 
                                     frPegShift.Negative(nullptr).Total()));
    }
    
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    
    string txstr = HexStr(ss.begin(), ss.end());
    
    Object result;
    result.push_back(Pair("completed", true));
    result.push_back(Pair("txhash", txhash));
    result.push_back(Pair("rawtx", txstr));

    result.push_back(Pair("consumed_inputs", sConsumedInputs));
    result.push_back(Pair("provided_outputs", sProvidedOutputs));

    result.push_back(Pair("created_on_peg", peglevel_net.nSupply));
    result.push_back(Pair("broadcast_on_peg", peglevel_net.nSupplyNext));
    
    printpegbalance(frBalance, peglevel, result, "balance_", true);
    printpegbalance(frProcessed, peglevel, result, "processed_", true);
    printpegbalance(frExchange, peglevel_net, result, "exchange_", true);
    
    printpegshift(frPegShift, peglevel_net, result, true);
    
    Array changes;
    for(const pair<string, int64_t> & item : mapTxChanges) {
        Object obj;
        obj.push_back(Pair("txout", item.first));
        obj.push_back(Pair("amount", item.second));
        changes.push_back(obj);
    }
    result.push_back(Pair("changes", changes));
    
    return result;
}




#ifdef ENABLE_FAUCET

Value faucet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "faucet <address>\n"
            );
    
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid BitBay address");

    Object result;
    bool completed_liquid = true;
    bool completed_reserve = true;
    string status = "Unknown";
    
    int64_t amount = 100000000000;
    CFractions fr(amount, CFractions::VALUE);
    fr = fr.Std();

    int nSupply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int64_t liquid = fr.High(nSupply);
    int64_t reserve = fr.Low(nSupply);
    
    if (liquid >0) {
        PegTxType txType = PEG_MAKETX_SEND_LIQUIDITY;
        
        vector<pair<CScript, int64_t> > vecSend;
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        vecSend.push_back(make_pair(scriptPubKey, liquid));
        
        CWalletTx wtx;
        CReserveKey keyChange(pwalletMain);
        int64_t nFeeRequired = 0;
        bool fCreated = pwalletMain->CreateTransaction(txType, vecSend, wtx, keyChange, nFeeRequired, nullptr);
        if (fCreated) {
            
            bool fCommitted = pwalletMain->CommitTransaction(wtx, keyChange);
            if (fCommitted) {
                completed_liquid = true;
            } else {
                completed_liquid = false;
                status = "Failed to commit a liquid transaction";
            }
            
        } else {
            completed_liquid = false;
            status = "Failed to create a liquid transaction";
        }
    }
    
    if (reserve >0) {
        PegTxType txType = PEG_MAKETX_SEND_RESERVE;
        
        vector<pair<CScript, int64_t> > vecSend;
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        vecSend.push_back(make_pair(scriptPubKey, reserve));
        
        CWalletTx wtx;
        CReserveKey keyChange(pwalletMain);
        int64_t nFeeRequired = 0;
        bool fCreated = pwalletMain->CreateTransaction(txType, vecSend, wtx, keyChange, nFeeRequired, nullptr);
        if (fCreated) {
            
            bool fCommitted = pwalletMain->CommitTransaction(wtx, keyChange);
            if (fCommitted) {
                completed_reserve = true;
            } else {
                completed_reserve = false;
                status = "Failed to commit a reserve transaction";
            }
            
        } else {
            completed_reserve = false;
            status = "Failed to create a reserve transaction";
        }
    }
    result.push_back(Pair("completed", completed_reserve && completed_liquid));
    result.push_back(Pair("completed_liquid", completed_liquid));
    result.push_back(Pair("completed_reserve", completed_reserve));
    result.push_back(Pair("status", status));
    
    return result;
}

#endif

#endif
