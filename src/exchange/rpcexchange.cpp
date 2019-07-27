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
#include "wallet.h"

#include "pegops.h"
#include "pegdata.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

string packpegdata(const CFractions & fractions,
                   const CPegLevel & peglevel);

void unpackpegdata(CFractions & fractions,
                   const string & pegdata64,
                   string tag);

void unpackbalance(CFractions & fractions,
                      CPegLevel & peglevel,
                      const string & pegdata64,
                      string tag);

bool unpackbalance(CFractions &     fractions,
                   int64_t &        nReserve,
                   int64_t &        nLiquid,
                   CPegLevel &      peglevel,
                   const string &   pegdata64,
                   string           tag);

void printpegshift(const CFractions & frPegShift,
                   const CPegLevel & peglevel,
                   Object & result,
                   bool print_pegdata);

void printpeglevel(const CPegLevel & peglevel,
                   Object & result);

void printpegbalance(const CFractions & frBalance,
                     const CPegLevel & peglevel,
                     Object & result,
                     string prefix,
                     bool print_pegdata);

void printpegbalance(const CFractions & frBalance,
                     int64_t nReserve,
                     int64_t nLiquid,
                     const CPegLevel & peglevel,
                     Object & result,
                     string prefix,
                     bool print_pegdata);

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
    int nBuffer = 3;
    
    string peglevel_hex;
    string pegpool_pegdata64;
    string err;

    bool ok = pegops::getpeglevel(
                exchange_pegdata64,
                pegshift_pegdata64,
                nCycleNow,
                nCyclePrev,
                nSupplyNow + nBuffer,
                nSupplyNext + nBuffer,
                nSupplyNextNext + nBuffer,
                peglevel_hex,
                pegpool_pegdata64,
                err
    );
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, err);
    }
    
    int64_t nPegPoolLiquid = 0;
    int64_t nPegPoolReserve = 0;
    CPegLevel peglevel_pegpool("");
    CFractions frPegPool(0, CFractions::STD);
    unpackbalance(frPegPool, nPegPoolReserve, nPegPoolLiquid, 
                  peglevel_pegpool, pegpool_pegdata64, "pegpool");
    
    CPegLevel peglevel(peglevel_hex);
    
    Object result;
    result.push_back(Pair("cycle", peglevel.nCycle));

    printpeglevel(peglevel, result);
    printpegbalance(frPegPool, nPegPoolReserve, nPegPoolLiquid, peglevel, result, "pegpool_", true);
    printpegbalance(frExchange, peglevel, result, "exchange_", false);
    printpegshift(frPegShift, peglevel, result, false);
    
    return result;
}

Value makepeglevel(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 7)
        throw runtime_error(
            "makepeglevel "
                "<current_cycle> "
                "<previous_cycle> "
                "<pegsupplyindex> "
                "<pegsupplyindex_next> "
                "<pegsupplyindex_nextnext> "
                "<exchange_pegdata_base64> "
                "<pegshift_pegdata_base64>\n"
            );
    
    int cycle_now = params[0].get_int();
    int cycle_prev = params[1].get_int();
    int supply_now = params[2].get_int();
    int supply_next = params[3].get_int();
    int supply_next_next = params[4].get_int();
    string exchange_pegdata64 = params[5].get_str();
    string pegshift_pegdata64 = params[6].get_str();
    
    CFractions frExchange(0, CFractions::VALUE);
    CFractions frPegShift(0, CFractions::VALUE);

    unpackpegdata(frExchange, exchange_pegdata64, "exchange");
    unpackpegdata(frPegShift, pegshift_pegdata64, "pegshift");
    
    frExchange = frExchange.Std();
    frPegShift = frPegShift.Std();

    string peglevel_hex;
    string pegpool_pegdata64;
    string err;

    bool ok = pegops::getpeglevel(
                exchange_pegdata64,
                pegshift_pegdata64,
                cycle_now,
                cycle_prev,
                supply_now,
                supply_next,
                supply_next_next,
                peglevel_hex,
                pegpool_pegdata64,
                err
    );
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, err);
    }
    
    int64_t nPegPoolLiquid = 0;
    int64_t nPegPoolReserve = 0;
    CPegLevel peglevel_pegpool("");
    CFractions frPegPool(0, CFractions::STD);
    unpackbalance(frPegPool, nPegPoolReserve, nPegPoolLiquid, 
                  peglevel_pegpool, pegpool_pegdata64, "pegpool");
    
    CPegLevel peglevel(peglevel_hex);
    
    Object result;
    result.push_back(Pair("cycle", peglevel.nCycle));

    printpeglevel(peglevel, result);
    printpegbalance(frPegPool, nPegPoolReserve, nPegPoolLiquid, peglevel, result, "pegpool_", true);
    printpegbalance(frExchange, peglevel, result, "exchange_", false);
    printpegshift(frPegShift, peglevel, result, false);
    
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
    
    string inp_balance_pegdata64 = params[0].get_str();
    string inp_pegpool_pegdata64 = params[1].get_str();
    string inp_peglevel_hex = params[2].get_str();
    
    string out_balance_pegdata64;
    string out_pegpool_pegdata64;
    string out_err;
    
    bool ok = pegops::updatepegbalances(
            inp_balance_pegdata64,
            inp_pegpool_pegdata64,
            inp_peglevel_hex,
            
            out_balance_pegdata64,
            out_pegpool_pegdata64,
            out_err);
    
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, out_err);
    }

    CPegLevel peglevel_new(inp_peglevel_hex);
    CPegLevel peglevel_skip1("");
    CPegLevel peglevel_skip2("");
    
    CFractions frBalance(0, CFractions::STD);
    CFractions frPegPool(0, CFractions::STD);
    
    int64_t nBalanceLiquid = 0;
    int64_t nPegPoolLiquid = 0;
    int64_t nBalanceReserve = 0;
    int64_t nPegPoolReserve = 0;
    
    unpackbalance(frBalance, nBalanceReserve, nBalanceLiquid, peglevel_skip1, out_balance_pegdata64, "balance");
    unpackbalance(frPegPool, nPegPoolReserve, nPegPoolLiquid, peglevel_skip2, out_pegpool_pegdata64, "pegpool");
    
    Object result;

    result.push_back(Pair("completed", true));
    result.push_back(Pair("cycle", peglevel_new.nCycle));
    
    printpeglevel(peglevel_new, result);
    printpegbalance(frBalance, nBalanceReserve, nBalanceLiquid, peglevel_new, result, "balance_", true);
    printpegbalance(frPegPool, nPegPoolReserve, nPegPoolLiquid, peglevel_new, result, "pegpool_", true);
    
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
    
    int64_t inp_move_amount = params[0].get_int64();
    string inp_src_pegdata64 = params[1].get_str();
    string inp_dst_pegdata64 = params[2].get_str();
    string inp_peglevel_hex = params[3].get_str();
    
    string out_src_pegdata64;
    string out_dst_pegdata64;
    string out_err;
    
    bool ok = pegops::movecoins(
            inp_move_amount,
            inp_src_pegdata64,
            inp_dst_pegdata64,
            inp_peglevel_hex,
            true,
            
            out_src_pegdata64,
            out_dst_pegdata64,
            out_err);
    
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, out_err);
    }
    
    CPegLevel peglevel(inp_peglevel_hex);
    CPegLevel peglevel_skip1("");
    CPegLevel peglevel_skip2("");
    
    CFractions frSrc(0, CFractions::STD);
    CFractions frDst(0, CFractions::STD);
    
    int64_t nSrcLiquid = 0;
    int64_t nDstLiquid = 0;
    int64_t nSrcReserve = 0;
    int64_t nDstReserve = 0;
    
    unpackbalance(frSrc, nSrcReserve, nSrcLiquid, peglevel_skip1, out_src_pegdata64, "src");
    unpackbalance(frDst, nDstReserve, nDstLiquid, peglevel_skip2, out_dst_pegdata64, "dst");
    
    Object result;
    
    result.push_back(Pair("cycle", peglevel.nCycle));
    
    printpeglevel(peglevel, result);
    printpegbalance(frSrc, nSrcReserve, nSrcLiquid, peglevel, result, "src_", true);
    printpegbalance(frDst, nDstReserve, nDstLiquid, peglevel, result, "dst_", true);
    
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
    
    int64_t inp_move_liquid = params[0].get_int64();
    string inp_src_pegdata64 = params[1].get_str();
    string inp_dst_pegdata64 = params[2].get_str();
    string inp_peglevel_hex = params[3].get_str();
    
    string out_src_pegdata64;
    string out_dst_pegdata64;
    string out_err;
    
    bool ok = pegops::moveliquid(
            inp_move_liquid,
            inp_src_pegdata64,
            inp_dst_pegdata64,
            inp_peglevel_hex,
            
            out_src_pegdata64,
            out_dst_pegdata64,
            out_err);
    
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, out_err);
    }
    
    CPegLevel peglevel(inp_peglevel_hex);
    CPegLevel peglevel_skip1("");
    CPegLevel peglevel_skip2("");
    
    CFractions frSrc(0, CFractions::STD);
    CFractions frDst(0, CFractions::STD);
    
    int64_t nSrcLiquid = 0;
    int64_t nDstLiquid = 0;
    int64_t nSrcReserve = 0;
    int64_t nDstReserve = 0;
    
    unpackbalance(frSrc, nSrcReserve, nSrcLiquid, peglevel_skip1, out_src_pegdata64, "src");
    unpackbalance(frDst, nDstReserve, nDstLiquid, peglevel_skip2, out_dst_pegdata64, "dst");
    
    Object result;
    
    result.push_back(Pair("cycle", peglevel.nCycle));
    
    printpeglevel(peglevel, result);
    printpegbalance(frSrc, nSrcReserve, nSrcLiquid, peglevel, result, "src_", true);
    printpegbalance(frDst, nDstReserve, nDstLiquid, peglevel, result, "dst_", true);
    
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
    
    int64_t inp_move_reserve = params[0].get_int64();
    string inp_src_pegdata64 = params[1].get_str();
    string inp_dst_pegdata64 = params[2].get_str();
    string inp_peglevel_hex = params[3].get_str();
    
    string out_src_pegdata64;
    string out_dst_pegdata64;
    string out_err;
    
    bool ok = pegops::movereserve(
            inp_move_reserve,
            inp_src_pegdata64,
            inp_dst_pegdata64,
            inp_peglevel_hex,
            
            out_src_pegdata64,
            out_dst_pegdata64,
            out_err);
    
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, out_err);
    }
    
    CPegLevel peglevel(inp_peglevel_hex);
    CPegLevel peglevel_skip1("");
    CPegLevel peglevel_skip2("");
    
    CFractions frSrc(0, CFractions::STD);
    CFractions frDst(0, CFractions::STD);
    
    int64_t nSrcLiquid = 0;
    int64_t nDstLiquid = 0;
    int64_t nSrcReserve = 0;
    int64_t nDstReserve = 0;
    
    unpackbalance(frSrc, nSrcReserve, nSrcLiquid, peglevel_skip1, out_src_pegdata64, "src");
    unpackbalance(frDst, nDstReserve, nDstLiquid, peglevel_skip2, out_dst_pegdata64, "dst");
    
    Object result;
    
    result.push_back(Pair("cycle", peglevel.nCycle));
    
    printpeglevel(peglevel, result);
    printpegbalance(frSrc, nSrcReserve, nSrcLiquid, peglevel, result, "src_", true);
    printpegbalance(frDst, nDstReserve, nDstLiquid, peglevel, result, "dst_", true);
    
    return result;
}

Value removecoins(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "removecoins "
                "<arg1_pegdata_base64> "
                "<arg2_pegdata_base64>\n"
            );
    
    string inp_arg1_pegdata64 = params[0].get_str();
    string inp_arg2_pegdata64 = params[1].get_str();
    
    string out_arg1_pegdata64;
    string out_err;
    
    bool ok = pegops::removecoins(
            inp_arg1_pegdata64,
            inp_arg2_pegdata64,
            
            out_arg1_pegdata64,
            out_err);
    
    if (!ok) {
        throw JSONRPCError(RPC_MISC_ERROR, out_err);
    }
    
    CPegLevel peglevel("");
    CFractions frArg1(0, CFractions::STD);
    
    int64_t nArg1Liquid = 0;
    int64_t nArg1Reserve = 0;
    
    unpackbalance(frArg1, nArg1Reserve, nArg1Liquid, peglevel, out_arg1_pegdata64, "out");
    
    Object result;
    
    printpegbalance(frArg1, nArg1Reserve, nArg1Liquid, peglevel, result, "out_", true);
    
    return result;
}
