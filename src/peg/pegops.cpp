// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pegops.h"
#include "pegdata.h"
#include "pegutil.h"

#include <map>
#include <set>
#include <cstdint>
#include <utility>
#include <algorithm> 
#include <type_traits>

#include <boost/multiprecision/cpp_int.hpp>

#include <zconf.h>
#include <zlib.h>

using namespace std;
using namespace boost;
using namespace pegutil;

namespace pegops {

string packpegdata(const CFractions &   fractions,
                   const CPegLevel &    peglevel)
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

string packpegbalance(const CFractions &     fractions,
                      const CPegLevel &      peglevel,
                      int64_t                nReserve,
                      int64_t                nLiquid)
{
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    fractions.Pack(fout);
    peglevel.Pack(fout);
    fout << nReserve;
    fout << nLiquid;
    return EncodeBase64(fout.str());
}

static bool unpackpegdata(CFractions &      fractions, 
                          const string &    pegdata64,
                          const string &    tag,
                          string &          err)
{
    if (pegdata64.empty()) 
        return true;
    
    string pegdata = DecodeBase64(pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    
    if (!fractions.Unpack(finp)) {
         err = "Can not unpack '"+tag+"' pegdata";
         return false;
    }
    
    return true;
}

bool unpackbalance(CFractions &     fractions,
                   CPegLevel &      peglevel,
                   const string &   pegdata64,
                   string           tag,
                   string &         err)
{
    if (pegdata64.empty()) 
        return true;
    
    string pegdata = DecodeBase64(pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    
    if (!fractions.Unpack(finp)) {
         err = "Can not unpack '"+tag+"' pegdata";
         return false;
    }
    
    try { 
    
        CPegLevel peglevel_copy = peglevel;
        if (!peglevel_copy.Unpack(finp)) {
            err = "Can not unpack '"+tag+"' peglevel";
            return false;
        }
        else peglevel = peglevel_copy;
    }
    catch (std::exception &) { ; }
    
    return true;
}

bool unpackbalance(
        const string &   inp_pegdata64,
        string           inp_tag,
        
        CFractions &     out_fractions,
        CPegLevel &      out_peglevel,
        int64_t &        out_reserve,
        int64_t &        out_liquid,
        string &         out_err)
{
    if (inp_pegdata64.empty()) 
        return true;
    
    string pegdata = DecodeBase64(inp_pegdata64);
    CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    
    if (!out_fractions.Unpack(finp)) {
         out_err = "Can not unpack '"+inp_tag+"' pegdata";
         return false;
    }
    
    try { 
    
        CPegLevel peglevel_copy = out_peglevel;
        if (!peglevel_copy.Unpack(finp)) {
            out_err = "Can not unpack '"+inp_tag+"' peglevel";
            return false;
        }
        else out_peglevel = peglevel_copy;
    
        try {
            finp >> out_reserve;
            finp >> out_liquid;
        }
        catch (std::exception &) { 
            out_liquid = out_fractions.High(out_peglevel);
            out_reserve = out_fractions.Low(out_peglevel);
        }
    }
    catch (std::exception &) { ; }
    
    return true;
}

// API calls

bool getpeglevel(
        const std::string & inp_exchange_pegdata64,
        const std::string & inp_pegshift_pegdata64,
        int                 inp_cycle_now,
        int                 inp_cycle_prev,
        int                 inp_peg_now,
        int                 inp_peg_next,
        int                 inp_peg_next_next,
        
        std::string & out_peglevel_hex,
        std::string & out_pegpool_pegdata64,
        std::string & out_err)
{
    out_err.clear();
    CFractions frExchange(0, CFractions::VALUE);
    CFractions frPegShift(0, CFractions::VALUE);

    if (!unpackpegdata(frExchange, inp_exchange_pegdata64, "exchange", out_err)) {
        return false;
    }
    if (!unpackpegdata(frPegShift, inp_pegshift_pegdata64, "pegshift", out_err)) {
        return false;
    }
    
    frExchange = frExchange.Std();
    frPegShift = frPegShift.Std();

    // exchange peglevel
    CPegLevel peglevel(inp_cycle_now,
                       inp_cycle_prev,
                       inp_peg_now,
                       inp_peg_next,
                       inp_peg_next_next,
                       frExchange,
                       frPegShift);

    int peg_effective = peglevel.nSupply + peglevel.nShift;
    CFractions frPegPool = frExchange.HighPart(peg_effective, nullptr);

    int64_t nPegPoolValue = frPegPool.Total();
    int64_t nPegPoolReserve = peglevel.nShiftLastPart;
    int64_t nPegPoolLiquid = nPegPoolValue - nPegPoolReserve;
    
    out_peglevel_hex = peglevel.ToString();
    out_pegpool_pegdata64 = packpegbalance(frPegPool,
                                           peglevel,
                                           nPegPoolReserve,
                                           nPegPoolLiquid);
    
    return true;
}

bool getpeglevelinfo(
        const std::string & inp_peglevel_hex,
        
        int &       out_cycle_now,
        int &       out_cycle_prev,
        int &       out_peg_now,
        int &       out_peg_next,
        int &       out_peg_next_next,
        int &       out_shift,
        int64_t &   out_shiftlastpart,
        int64_t &   out_shiftlasttotal)
{
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        return false;
    }

    out_cycle_now       = peglevel.nCycle;
    out_cycle_prev      = peglevel.nCyclePrev;
    out_peg_now         = peglevel.nSupply;
    out_peg_next        = peglevel.nSupplyNext;
    out_peg_next_next   = peglevel.nSupplyNextNext;
    out_shift           = peglevel.nShift;
    out_shiftlastpart   = peglevel.nShiftLastPart;
    out_shiftlasttotal  = peglevel.nShiftLastTotal;
    
    return true;
}

bool updatepegbalances(
        const std::string & inp_balance_pegdata64,
        const std::string & inp_pegpool_pegdata64,
        const std::string & inp_peglevel_hex,
        
        std::string &   out_balance_pegdata64,
        std::string &   out_pegpool_pegdata64,
        std::string &   out_err)
{
    out_err.clear();
    CPegLevel peglevel_old("");
    CPegLevel peglevel_pool("");
    CPegLevel peglevel_new(inp_peglevel_hex);
    if (!peglevel_new.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }
    
    CFractions frBalance(0, CFractions::VALUE);
    CFractions frPegPool(0, CFractions::VALUE);
    
    int64_t nPegPoolLiquid = 0;
    int64_t nPegPoolReserve = 0;
    
    if (!unpackbalance(inp_pegpool_pegdata64, "pegpool", 
                       frPegPool, peglevel_pool,
                       nPegPoolReserve, nPegPoolLiquid,
                       out_err)) {
        return false;
    }
    if (!unpackbalance(frBalance, peglevel_old, inp_balance_pegdata64, "balance", out_err)) {
        return false;
    }

    frBalance = frBalance.Std();
    frPegPool = frPegPool.Std();
    
    if (peglevel_pool.nCycle != peglevel_new.nCycle) {
        out_err = "PegPool has other cycle than peglevel";
        return false;
    }
    
    if (peglevel_old.nCycle == peglevel_new.nCycle) { // already up-to-dated
        out_pegpool_pegdata64 = inp_pegpool_pegdata64;
        out_balance_pegdata64 = inp_balance_pegdata64;
        out_err = "Already up-to-dated";
        return true;
    }

    if (peglevel_old.nCycle > peglevel_new.nCycle) {
        out_err = "Balance has greater cycle than peglevel";
        return false;
    }
    
    if (peglevel_old.nCycle != 0 && 
        peglevel_old.nCycle != peglevel_new.nCyclePrev) {
        std::stringstream ss;
        ss << "Mismatch for peglevel_new.nCyclePrev "
           << peglevel_new.nCyclePrev
           << " vs peglevel_old.nCycle "
           << peglevel_old.nCycle;
        out_err = ss.str();
        return false;
    }
    
    int64_t nValue = frBalance.Total();
    
    CFractions frLiquid(0, CFractions::STD);
    CFractions frReserve(0, CFractions::STD);
    
    int64_t nReserve = 0;
    // current part of balance turns to reserve
    // the balance is to be updated at previous cycle
    frReserve = frBalance.LowPart(peglevel_new, &nReserve);
    
    frLiquid = CFractions(0, CFractions::STD);
    
    if (nReserve != frReserve.Total()) {
        std::stringstream ss;
        ss << "Reserve mimatch on LowPart " << frReserve.Total() << " vs " << nValue;
        out_err = ss.str();
        return false;
    }
        
    // if partial last reserve fraction then took reserve from this idx
    int nSupplyEffective = peglevel_new.nSupply + peglevel_new.nShift;
    int nLastIdx = nSupplyEffective;
    if (nLastIdx >=0 && 
        nLastIdx <PEG_SIZE && 
        peglevel_new.nShiftLastPart >0 && 
        peglevel_new.nShiftLastTotal >0) {
        
        int64_t nLastTotal = frPegPool.f[nLastIdx];
        int64_t nLastReserve = frReserve.f[nLastIdx];
        int64_t nTakeReserve = nLastReserve;
        nTakeReserve = std::min(nTakeReserve, nLastTotal);
        nTakeReserve = std::min(nTakeReserve, nPegPoolReserve);
        
        nPegPoolReserve -= nTakeReserve;
        frPegPool.f[nLastIdx] -= nTakeReserve;
        
        if (nLastReserve > nTakeReserve) { // take it from liquid
            int64_t nDiff = nLastReserve - nTakeReserve;
            frReserve.f[nLastIdx] -= nDiff;
            nReserve -= nDiff;
        }
        
        // for liquid of partial we need to take proportionally
        // from liquid of the fraction as nLiquid/nLiquidPool
        nLastTotal = frPegPool.f[nLastIdx];
        nPegPoolReserve = std::min(nPegPoolReserve, nLastTotal);
        
        int64_t nLastLiquid = nLastTotal - nPegPoolReserve;
        int64_t nLiquid = nValue - nReserve;
        int64_t nLiquidPool = frPegPool.Total() - nPegPoolReserve;
        int64_t nTakeLiquid = RatioPart(nLastLiquid,
                                        nLiquid,
                                        nLiquidPool);
        nTakeLiquid = std::min(nTakeLiquid, nLastTotal);
        
        frLiquid.f[nLastIdx] += nTakeLiquid;
        frPegPool.f[nLastIdx] -= nTakeLiquid;
    }
    
    // liquid is just normed to pool
    int64_t nLiquid = nValue - nReserve;
    int64_t nLiquidTodo = nValue - nReserve - frLiquid.Total();
    int64_t nLiquidPool = frPegPool.Total() - nPegPoolReserve;
    if (nLiquidTodo > nLiquidPool) { // exchange liquidity mismatch
        std::stringstream ss;
        ss << "Not enough liquid " << nLiquidPool 
           << " on 'pool' to balance " << nLiquidTodo;
        out_err = ss.str();
        return false;
    }
    
    int64_t nHoldLastPart = 0;
    if (nPegPoolReserve >0) {
        nHoldLastPart = frPegPool.f[nLastIdx];
        frPegPool.f[nLastIdx] = 0;
    }
    
    nLiquidTodo = frPegPool.MoveRatioPartTo(nLiquidTodo, frLiquid);

    if (nLiquidTodo >0 && nLiquidTodo <= nHoldLastPart) {
        frLiquid.f[nLastIdx] += nLiquidTodo;
        nHoldLastPart -= nLiquidTodo;
        nLiquidTodo = 0;
    }
    
    if (nHoldLastPart > 0) {
        frPegPool.f[nLastIdx] = nHoldLastPart;
        nHoldLastPart = 0; 
    }
    
    if (nLiquidTodo >0) {
        std::stringstream ss;
        ss << "Liquid not enough after MoveRatioPartTo "
           << nLiquidTodo;
        out_err = ss.str();
        return false;
    }
    
    frBalance = frReserve + frLiquid;
    
    if (nValue != frBalance.Total()) {
        std::stringstream ss;
        ss << "Balance mimatch after update " << frBalance.Total() 
           << " vs " << nValue;
        out_err = ss.str();
        return false;
    }
    
    int64_t nPegPoolValue = frPegPool.Total();
    nPegPoolLiquid = nPegPoolValue - nPegPoolReserve;
    
    out_pegpool_pegdata64 = packpegbalance(
                frPegPool, 
                peglevel_new,
                nPegPoolReserve,
                nPegPoolLiquid
                );
    out_balance_pegdata64 = packpegbalance(
                frBalance, 
                peglevel_new,
                nReserve,
                nLiquid
                );
    
    return true;
}

bool movecoins(
        int64_t             inp_move_amount,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        bool                inp_cross_cycles,
        
        std::string &   out_src_pegdata64,
        std::string &   out_dst_pegdata64,
        std::string &   out_err)
{
    int64_t move_amount = inp_move_amount;
    
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }
    
    int64_t nSrcLiquid = 0;
    int64_t nSrcReserve = 0;
    CPegLevel peglevel_src = peglevel;
    CFractions frSrc(0, CFractions::VALUE);
    if (!unpackbalance(inp_src_pegdata64, "src", 
                       frSrc, peglevel_src, 
                       nSrcReserve, nSrcLiquid, 
                       out_err)) {
        //for move coins ok to skip uncognized peglevel
        //return false;
    }
    
    if (!inp_cross_cycles && peglevel != peglevel_src) {
        std::stringstream ss;
        ss << "Outdated 'src' of cycle of " << peglevel_src.nCycle
           << ", current " << peglevel.nCycle;
        out_err = ss.str();
        return false;
    }
    
    if (move_amount <0) {
        std::stringstream ss;
        ss << "Requested to move negative " << move_amount;
        out_err = ss.str();
        return false;
    }
    
    int64_t src_value = frSrc.Total();
    if (src_value < move_amount) {
        std::stringstream ss;
        ss << "Not enough amount " << src_value
           << " on 'src' to move " << move_amount;
        out_err = ss.str();
        return false;
    }

    int64_t nDstLiquid = 0;
    int64_t nDstReserve = 0;
    CPegLevel peglevel_dst = peglevel;
    CFractions frDst(0, CFractions::VALUE);
    if (!unpackbalance(inp_dst_pegdata64, "dst", 
                       frDst, peglevel_dst, 
                       nDstReserve, nDstLiquid,
                       out_err)) {
        return false;
    }
    
    if (peglevel != peglevel_dst) {
        std::stringstream ss;
        ss << "Outdated 'dst' of cycle of " << peglevel_dst.nCycle
           << ", current " << peglevel.nCycle;
        out_err = ss.str();
        return false;
    }
    
    int64_t nIn = frSrc.Total() + frDst.Total();
    
    frSrc = frSrc.Std();
    frDst = frDst.Std();
    
    CFractions frAmount = frSrc;
    CFractions frMove = frAmount.RatioPart(move_amount);
    
    frSrc -= frMove;
    frDst += frMove;
    
    int64_t nOut = frSrc.Total() + frDst.Total();
    
    if (nIn != nOut) {
        std::stringstream ss;
        ss << "Mismatch in and out values " << nIn
           << " vs " << nOut;
        out_err = ss.str();
        return false;
    }
    
    // std calc
    nSrcLiquid = frSrc.High(peglevel);
    nSrcReserve = frSrc.Low(peglevel);
    nDstLiquid = frDst.High(peglevel);
    nDstReserve = frDst.Low(peglevel);
    
    out_src_pegdata64 = packpegbalance(frSrc, peglevel, nSrcReserve, nSrcLiquid);
    out_dst_pegdata64 = packpegbalance(frDst, peglevel, nDstReserve, nDstLiquid);
    return true;
}

bool moveliquid(
        int64_t             inp_move_liquid,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        
        std::string &   out_src_pegdata64,
        std::string &   out_dst_pegdata64,
        std::string &   out_err)
{
    int64_t move_liquid = inp_move_liquid;
    
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }
    
    int nSupplyEffective = peglevel.nSupply + peglevel.nShift;
    if (nSupplyEffective <0 && 
        nSupplyEffective >=PEG_SIZE) {
        std::stringstream ss;
        ss << "Supply index out of bounds " << nSupplyEffective;
        out_err = ss.str();
        return false;
    }
    
    int64_t nSrcLiquid = 0;
    int64_t nSrcReserve = 0;
    CPegLevel peglevel_src = peglevel;
    CFractions frSrc(0, CFractions::VALUE);
    if (!unpackbalance(inp_src_pegdata64, "src", 
                       frSrc, peglevel_src, 
                       nSrcReserve, nSrcLiquid, 
                       out_err)) {
        return false;
    }
    
    if (peglevel != peglevel_src) {
        std::stringstream ss;
        ss << "Outdated 'src' of cycle of " << peglevel_src.nCycle
           << ", current " << peglevel.nCycle;
        out_err = ss.str();
        return false;
    }
    
    if (move_liquid <0) {
        std::stringstream ss;
        ss << "Requested to move negative " << move_liquid;
        out_err = ss.str();
        return false;
    }
    
    if (nSrcLiquid < move_liquid) {
        std::stringstream ss;
        ss << "Not enough liquid " << nSrcLiquid
           << " on 'src' to move " << move_liquid;
        out_err = ss.str();
        return false;
    }

    int64_t nDstLiquid = 0;
    int64_t nDstReserve = 0;
    CPegLevel peglevel_dst = peglevel;
    CFractions frDst(0, CFractions::VALUE);
    if (!unpackbalance(inp_dst_pegdata64, "dst", 
                       frDst, peglevel_dst, 
                       nDstReserve, nDstLiquid,
                       out_err)) {
        return false;
    }
    
    if (peglevel != peglevel_dst) {
        std::stringstream ss;
        ss << "Outdated 'dst' of cycle of " << peglevel_dst.nCycle
           << ", current " << peglevel.nCycle;
        out_err = ss.str();
        return false;
    }
    
    int64_t nIn = frSrc.Total() + frDst.Total();
    
    frSrc = frSrc.Std();
    frDst = frDst.Std();

    bool fPartial = peglevel.nShiftLastPart >0 && peglevel.nShiftLastTotal >0;
    if (fPartial) {
        nSupplyEffective++;
    }
    
    CFractions frLiquid = frSrc.HighPart(nSupplyEffective, nullptr);
    
    if (fPartial) {
        int64_t nPartialLiquid = nSrcLiquid - frLiquid.Total();
        if (nPartialLiquid < 0) {
            std::stringstream ss;
            ss << "Mismatch on nPartialLiquid " << nPartialLiquid;
            out_err = ss.str();
            return false;
        }
        
        frLiquid.f[nSupplyEffective-1] = nPartialLiquid;
    }
    
    if (frLiquid.Total() < move_liquid) {
        std::stringstream ss;
        ss << "Not enough liquid(1) " << frLiquid.Total()
           << " on 'src' to move " << move_liquid;
        out_err = ss.str();
        return false;
    }
    
    CFractions frMove = frLiquid.RatioPart(move_liquid);
    
    frSrc -= frMove;
    frDst += frMove;
    
    nSrcLiquid -= move_liquid;
    nDstLiquid += move_liquid;
    
    int64_t nOut = frSrc.Total() + frDst.Total();
    
    if (nIn != nOut) {
        std::stringstream ss;
        ss << "Mismatch in and out values " << nIn
           << " vs " << nOut;
        out_err = ss.str();
        return false;
    }
    
    if (!frSrc.IsPositive()) {
        out_err = "Negative detected in 'src";
        return false;
    }
    
    out_src_pegdata64 = packpegbalance(frSrc, peglevel, nSrcReserve, nSrcLiquid);
    out_dst_pegdata64 = packpegbalance(frDst, peglevel, nDstReserve, nDstLiquid);
    return true;
}

bool movereserve(
        int64_t             inp_move_reserve,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        
        std::string &   out_src_pegdata64,
        std::string &   out_dst_pegdata64,
        std::string &   out_err)
{
    int64_t move_reserve = inp_move_reserve;
    
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }
    
    int nSupplyEffective = peglevel.nSupply + peglevel.nShift;
    if (nSupplyEffective <0 && 
        nSupplyEffective >=PEG_SIZE) {
        std::stringstream ss;
        ss << "Supply index out of bounds " << nSupplyEffective;
        out_err = ss.str();
        return false;
    }
    
    int64_t nSrcLiquid = 0;
    int64_t nSrcReserve = 0;
    CPegLevel peglevel_src = peglevel;
    CFractions frSrc(0, CFractions::VALUE);
    if (!unpackbalance(inp_src_pegdata64, "src", 
                       frSrc, peglevel_src, 
                       nSrcReserve, nSrcLiquid, 
                       out_err)) {
        return false;
    }
    
    if (peglevel != peglevel_src) {
        std::stringstream ss;
        ss << "Outdated 'src' of cycle of " << peglevel_src.nCycle
           << ", current " << peglevel.nCycle;
        out_err = ss.str();
        return false;
    }
    
    if (move_reserve <0) {
        std::stringstream ss;
        ss << "Requested to move negative " << move_reserve;
        out_err = ss.str();
        return false;
    }
    
    if (nSrcReserve < move_reserve) {
        std::stringstream ss;
        ss << "Not enough reserve " << nSrcReserve
           << " on 'src' to move " << move_reserve;
        out_err = ss.str();
        return false;
    }

    int64_t nDstLiquid = 0;
    int64_t nDstReserve = 0;
    CPegLevel peglevel_dst = peglevel;
    CFractions frDst(0, CFractions::VALUE);
    if (!unpackbalance(inp_dst_pegdata64, "dst", 
                       frDst, peglevel_dst, 
                       nDstReserve, nDstLiquid,
                       out_err)) {
        return false;
    }
    
    if (peglevel != peglevel_dst) {
        std::stringstream ss;
        ss << "Outdated 'dst' of cycle of " << peglevel_dst.nCycle
           << ", current " << peglevel.nCycle;
        out_err = ss.str();
        return false;
    }
    
    int64_t nIn = frSrc.Total() + frDst.Total();
    
    frSrc = frSrc.Std();
    frDst = frDst.Std();

    CFractions frReserve = frSrc.LowPart(nSupplyEffective, nullptr);
    
    bool fPartial = peglevel.nShiftLastPart >0 && peglevel.nShiftLastTotal >0;
    if (fPartial) {
        int64_t nPartialReserve = nSrcReserve - frReserve.Total();
        if (nPartialReserve < 0) {
            std::stringstream ss;
            ss << "Mismatch on nPartialReserve " << nPartialReserve;
            out_err = ss.str();
            return false;
        }
        
        frReserve.f[nSupplyEffective] = nPartialReserve;
    }
    
    CFractions frMove = frReserve.RatioPart(move_reserve);
    
    frSrc -= frMove;
    frDst += frMove;
    
    nSrcReserve -= move_reserve;
    nDstReserve += move_reserve;
    
    int64_t nOut = frSrc.Total() + frDst.Total();
    
    if (nIn != nOut) {
        std::stringstream ss;
        ss << "Mismatch in and out values " << nIn
           << " vs " << nOut;
        out_err = ss.str();
        return false;
    }
    
    if (!frSrc.IsPositive()) {
        out_err = "Negative detected in 'src";
        return false;
    }
    
    out_src_pegdata64 = packpegbalance(frSrc, peglevel, nSrcReserve, nSrcLiquid);
    out_dst_pegdata64 = packpegbalance(frDst, peglevel, nDstReserve, nDstLiquid);
    return true;
}

bool removecoins(
        const std::string & inp_arg1_pegdata64,
        const std::string & inp_arg2_pegdata64,
        
        std::string &   out_arg1_pegdata64,
        std::string &   out_err)
{
    int64_t nArg1Liquid = 0;
    int64_t nArg1Reserve = 0;
    CPegLevel peglevel_arg1("");
    CFractions frArg1(0, CFractions::VALUE);
    if (!unpackbalance(inp_arg1_pegdata64, "arg1", 
                       frArg1, peglevel_arg1, 
                       nArg1Reserve, nArg1Liquid, 
                       out_err)) {
        return false;
    }
    
    int64_t nArg2Liquid = 0;
    int64_t nArg2Reserve = 0;
    CPegLevel peglevel_arg2("");
    CFractions frArg2(0, CFractions::VALUE);
    if (!unpackbalance(inp_arg2_pegdata64, "arg2", 
                       frArg2, peglevel_arg2, 
                       nArg2Reserve, nArg2Liquid,
                       out_err)) {
        //skip false if no peglevel
        //return false;
    }
    
    frArg1 = frArg1.Std();
    frArg2 = frArg2.Std();
    
    frArg1 -= frArg2;
    
    nArg1Liquid -= nArg2Liquid;
    nArg1Reserve -= nArg2Reserve;
    
    out_arg1_pegdata64 = packpegbalance(frArg1, peglevel_arg1, nArg1Reserve, nArg1Liquid);
    return true;
}

} // namespace
