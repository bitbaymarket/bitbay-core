// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pegops.h"
#include "pegopsp.h"
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

// API calls

bool getpeglevel(
        int                 inp_cycle_now,
        int                 inp_cycle_prev,
        int                 inp_peg_now,
        int                 inp_peg_next,
        int                 inp_peg_next_next,
        const std::string & inp_exchange_pegdata64,
        const std::string & inp_pegshift_pegdata64,
        
        std::string & out_peglevel_hex,
        std::string & out_pegpool_pegdata64,
        std::string & out_err)
{
    out_err.clear();
    
    CPegData pdExchange(inp_exchange_pegdata64);
    if (!pdExchange.IsValid()) {
        out_err = "Can not unpack 'exchange' pegdata";
        return false;
    }

    CPegData pdPegShift(inp_pegshift_pegdata64);
    if (!pdPegShift.IsValid()) {
        out_err = "Can not unpack 'pegshift' pegdata";
        return false;
    }

    CPegData pdPegPool;
    CPegLevel peglevel;
    
    getpeglevel(inp_cycle_now,
                inp_cycle_prev,
                inp_peg_now,
                inp_peg_next,
                inp_peg_next_next,
                pdExchange,
                pdPegShift,

                peglevel,
                pdPegPool,
                out_err);

    out_peglevel_hex = peglevel.ToString();
    out_pegpool_pegdata64 = pdPegPool.ToString();

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
        int64_t     &   out_balance_liquid,
        int64_t     &   out_balance_reserve,
        std::string &   out_pegpool_pegdata64,
        std::string &   out_err)
{
    out_err.clear();
    
    CPegData pdBalance(inp_balance_pegdata64);
    if (!pdBalance.IsValid()) {
        out_err = "Can not unpack 'balance' pegdata";
        return false;
    }

    CPegData pdPegPool(inp_pegpool_pegdata64);
    if (!pdPegPool.IsValid()) {
        out_err = "Can not unpack 'pegpool' pegdata";
        return false;
    }
    
    CPegLevel peglevelNew(inp_peglevel_hex);
    if (!peglevelNew.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }

    if (pdBalance.peglevel.nCycle == peglevelNew.nCycle) { // already up-to-dated
        out_pegpool_pegdata64 = inp_pegpool_pegdata64;
        out_balance_pegdata64 = inp_balance_pegdata64;
        out_err = "Already up-to-dated";
        return true;
    }
    
    bool ok = updatepegbalances(pdBalance,
                                pdPegPool,
                                peglevelNew,
                                out_err);
    if (!ok) {
        return false;
    }
   
    out_pegpool_pegdata64   = pdPegPool.ToString();
    out_balance_pegdata64   = pdBalance.ToString();
    out_balance_liquid      = pdBalance.nLiquid;
    out_balance_reserve     = pdBalance.nReserve;

    return true;
}

bool movecoins(
        int64_t             inp_move_amount,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        bool                inp_cross_cycles,

        std::string &   out_src_pegdata64,
        int64_t     &   out_src_liquid,
        int64_t     &   out_src_reserve,
        std::string &   out_dst_pegdata64,
        int64_t     &   out_dst_liquid,
        int64_t     &   out_dst_reserve,
        std::string &   out_err)
{
    out_err.clear();
    
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }
    
    CPegData pdSrc(inp_src_pegdata64);
    if (!pdSrc.IsValid()) {
        out_err = "Can not unpack 'src' pegdata";
        return false;
    }

    CPegData pdDst(inp_dst_pegdata64);
    if (!pdDst.IsValid()) {
        out_err = "Can not unpack 'dst' pegdata";
        return false;
    }
    
    bool ok = movecoins(inp_move_amount,
                        pdSrc,
                        pdDst,
                        peglevel,
                        inp_cross_cycles,
                        out_err);
    if (!ok) {
        return false;
    }
    
    out_src_pegdata64   = pdSrc.ToString();
    out_src_liquid      = pdSrc.nLiquid;
    out_src_reserve     = pdSrc.nReserve;
    
    out_dst_pegdata64   = pdDst.ToString();
    out_dst_liquid      = pdDst.nLiquid;
    out_dst_reserve     = pdDst.nReserve;
    
    return true;
}

bool moveliquid(
        int64_t             inp_move_liquid,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,

        std::string &   out_src_pegdata64,
        int64_t     &   out_src_liquid,
        int64_t     &   out_src_reserve,
        std::string &   out_dst_pegdata64,
        int64_t     &   out_dst_liquid,
        int64_t     &   out_dst_reserve,
        std::string &   out_err)
{
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }

    CPegData pdSrc(inp_src_pegdata64);
    if (!pdSrc.IsValid()) {
        out_err = "Can not unpack 'src' pegdata";
        return false;
    }

    CPegData pdDst(inp_dst_pegdata64);
    if (!pdDst.IsValid()) {
        out_err = "Can not unpack 'dst' pegdata";
        return false;
    }

    bool ok = moveliquid(inp_move_liquid,
                         pdSrc,
                         pdDst,
                         peglevel,
                         out_err);
    if (!ok) {
        return false;
    }

    out_src_pegdata64   = pdSrc.ToString();
    out_src_liquid      = pdSrc.nLiquid;
    out_src_reserve     = pdSrc.nReserve;
    
    out_dst_pegdata64   = pdDst.ToString();
    out_dst_liquid      = pdDst.nLiquid;
    out_dst_reserve     = pdDst.nReserve;
    
    return true;
}

bool movereserve(
        int64_t             inp_move_reserve,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,

        std::string &   out_src_pegdata64,
        int64_t     &   out_src_liquid,
        int64_t     &   out_src_reserve,
        std::string &   out_dst_pegdata64,
        int64_t     &   out_dst_liquid,
        int64_t     &   out_dst_reserve,
        std::string &   out_err)
{
    CPegLevel peglevel(inp_peglevel_hex);
    if (!peglevel.IsValid()) {
        out_err = "Can not unpack peglevel";
        return false;
    }

    CPegData pdSrc(inp_src_pegdata64);
    if (!pdSrc.IsValid()) {
        out_err = "Can not unpack 'src' pegdata";
        return false;
    }

    CPegData pdDst(inp_dst_pegdata64);
    if (!pdDst.IsValid()) {
        out_err = "Can not unpack 'dst' pegdata";
        return false;
    }

    bool ok = movereserve(inp_move_reserve,
                          pdSrc,
                          pdDst,
                          peglevel,
                          out_err);
    if (!ok) {
        return false;
    }

    out_src_pegdata64   = pdSrc.ToString();
    out_src_liquid      = pdSrc.nLiquid;
    out_src_reserve     = pdSrc.nReserve;
    
    out_dst_pegdata64   = pdDst.ToString();
    out_dst_liquid      = pdDst.nLiquid;
    out_dst_reserve     = pdDst.nReserve;
    
    return true;
}

bool removecoins(
        const std::string & inp_from_pegdata64,
        const std::string & inp_remove_pegdata64,

        std::string &   out_from_pegdata64,
        int64_t     &   out_from_liquid,
        int64_t     &   out_from_reserve,
        std::string &   out_err)
{
    CPegData pdFrom(inp_from_pegdata64);
    if (!pdFrom.IsValid()) {
        out_err = "Can not unpack 'from' pegdata";
        return false;
    }
    
    CPegData pdRemove(inp_remove_pegdata64);
    if (!pdRemove.IsValid()) {
        out_err = "Can not unpack 'remove' pegdata";
        return false;
    }

    pdFrom.fractions    -= pdRemove.fractions;
    pdFrom.nLiquid      -= pdRemove.nLiquid;
    pdFrom.nReserve     -= pdRemove.nReserve;

    out_from_pegdata64  = pdFrom.ToString();
    out_from_liquid     = pdFrom.nLiquid;
    out_from_reserve    = pdFrom.nReserve;
    
    return true;
}

} // namespace
