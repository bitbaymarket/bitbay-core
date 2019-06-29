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

void updatepegbalances()
{
    
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
