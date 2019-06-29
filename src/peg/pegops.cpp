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

static bool unpackpegdata(CFractions & fractions, 
                          const string & pegdata64,
                          const string & tag,
                          string & err)
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

static bool unpackbalance(CFractions & fractions,
                          CPegLevel & peglevel,
                          const string & pegdata64,
                          string tag,
                          string & err)
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
//             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, 
//                                "Can not unpack '"+tag+"' peglevel");
        }
        else peglevel = peglevel_copy;
    
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
                       inp_peg_now+3,
                       inp_peg_next+3,
                       inp_peg_next_next+3,
                       frExchange,
                       frPegShift);

    int peg_effective = peglevel.nSupply + peglevel.nShift;
    CFractions frPegPool = frExchange.HighPart(peg_effective, nullptr);
    
    out_peglevel_hex = peglevel.ToString();
    out_pegpool_pegdata64 = packpegdata(frPegPool, peglevel);
    
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
    CPegLevel peglevel_old("");
    CPegLevel peglevel_new(inp_peglevel_hex);
    if (!peglevel_new.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }
    
    CFractions frBalance(0, CFractions::VALUE);
    CFractions frPegPool(0, CFractions::VALUE);
    
    if (!unpackpegdata(frPegPool, inp_pegpool_pegdata64, "pegpool", out_err)) {
        return false;
    }
    if (!unpackbalance(frBalance, peglevel_old, inp_balance_pegdata64, "balance", out_err)) {
        return false;
    }

    frBalance = frBalance.Std();
    frPegPool = frPegPool.Std();
    
    if (peglevel_old.nCycle == peglevel_new.nCycle) { // already up-to-dated
        out_pegpool_pegdata64 = packpegdata(frPegPool, peglevel_new);
        out_balance_pegdata64 = packpegdata(frBalance, peglevel_new);
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
    
    if (nReserve != frReserve.Total()) {
        std::stringstream ss;
        ss << "Reserve mimatch on LowPart " << frReserve.Total() << " vs " << nValue;
        out_err = ss.str();
        return false;
    }
        
    // if partial last reserve fraction then took reserve from this idx
    int nLastIdx = peglevel_new.nSupply + peglevel_new.nShift;
    if (nLastIdx >=0 && 
        nLastIdx <PEG_SIZE && 
        peglevel_new.nShiftLastPart >0 && 
        peglevel_new.nShiftLastTotal >0) {
        
        int64_t nLastTotal = frPegPool.f[nLastIdx];
        int64_t nLastReserve = frReserve.f[nLastIdx];
        int64_t nTakeReserve = std::min(nLastReserve, nLastTotal);
        
        frPegPool.f[nLastIdx] -= nTakeReserve;
        
        if (nLastReserve > nTakeReserve) { // from liquid
            int64_t nDiff = nLastReserve - nTakeReserve;
            frReserve.f[nLastIdx] -= nDiff;
            nReserve -= nDiff;
        }
    }
    
    // liquid is just normed to pool
    int64_t nLiquid = nValue - nReserve;
    if (nLiquid > frPegPool.Total()) { // exchange liquidity mismatch
        std::stringstream ss;
        ss << "Not enough liquid " << frPegPool.Total() 
           << " on 'pool' to balance " << nLiquid;
        out_err = ss.str();
        return false;
    }
    
    frLiquid = CFractions(0, CFractions::STD);
    frPegPool.MoveRatioPartTo(nLiquid, frLiquid);
    
    if (nLiquid != frLiquid.Total()) {
        std::stringstream ss;
        ss << "Liquid mimatch on MoveRatioPartTo " << frLiquid.Total() 
           << " vs " << nLiquid;
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
    
    out_pegpool_pegdata64 = packpegdata(frPegPool, peglevel_new);
    out_balance_pegdata64 = packpegdata(frBalance, peglevel_new);
    
    return true;
}

void movecoins()
{
    
}

void moveliquid()
{
    
}

void movereserve()
{
    
}

} // namespace
