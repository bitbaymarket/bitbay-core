// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pegopsp.h"
#include "pegpack.h"
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
        int                 nCycleNow,
        int                 nCyclePrev,
        int                 nPegNow,
        int                 nPegNext,
        int                 nPegNextNext,
        const CPegData &    pdExchange,
        const CPegData &    pdPegShift,
        
        CPegLevel &     peglevel,
        CPegData &      pdPegPool,
        std::string &   sErr)
{
    sErr.clear();
    
    pdPegPool = CPegData();
    // exchange peglevel, should have +buffer values
    pdPegPool.peglevel = CPegLevel(nCycleNow,
                                   nCyclePrev,
                                   nPegNow,
                                   nPegNext,
                                   nPegNextNext,
                                   pdExchange.fractions,
                                   pdPegShift.fractions);

    int nPegEffective = pdPegPool.peglevel.nSupply + pdPegPool.peglevel.nShift;
    // pool to include both, liquidity and (may) last partial reserve fractions
    pdPegPool.fractions = pdExchange.fractions.HighPart(nPegEffective, nullptr);

    int64_t nPegPoolValue = pdPegPool.fractions.Total();
    pdPegPool.nReserve = pdPegPool.peglevel.nShiftLastPart;
    pdPegPool.nLiquid = nPegPoolValue - pdPegPool.nReserve;
    peglevel = pdPegPool.peglevel;
    return true;
}

bool updatepegbalances(
        CPegData &          pdBalance,
        CPegData &          pdPegPool,
        const CPegLevel &   peglevelNew,
        
        std::string &   sErr)
{
    sErr.clear();

    if (pdPegPool.peglevel.nCycle != peglevelNew.nCycle) {
        sErr = "PegPool has other cycle than peglevel";
        return false;
    }

    if (pdBalance.peglevel.nCycle == peglevelNew.nCycle) { // already up-to-dated
        sErr = "Already up-to-dated";
        return true;
    }

    if (pdBalance.peglevel.nCycle > peglevelNew.nCycle) {
        sErr = "Balance has greater cycle than peglevel";
        return false;
    }

    if (pdBalance.peglevel.nCycle != 0 && /*new users with empty*/
        pdBalance.peglevel.nCycle != peglevelNew.nCyclePrev) {
        std::stringstream ss;
        ss << "Mismatch for peglevel_new.nCyclePrev "
           << peglevelNew.nCyclePrev
           << " vs peglevel_old.nCycle "
           << pdBalance.peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    int64_t nValue = pdBalance.fractions.Total();

    CFractions frLiquid(0, CFractions::STD);
    CFractions frReserve(0, CFractions::STD);

    int64_t nReserve = 0;
    // current part of balance turns to reserve
    // the balance is to be updated at previous cycle
    frReserve = pdBalance.fractions.LowPart(peglevelNew, &nReserve);

    frLiquid = CFractions(0, CFractions::STD);

    if (nReserve != frReserve.Total()) {
        std::stringstream ss;
        ss << "Reserve mimatch on LowPart " << frReserve.Total() << " vs " << nValue;
        sErr = ss.str();
        return false;
    }

    // if partial last reserve fraction then take reserve from this idx
    int nSupplyEffective = peglevelNew.nSupply + peglevelNew.nShift;
    int nLastIdx = nSupplyEffective;
    if (nLastIdx >=0 &&
        nLastIdx <PEG_SIZE &&
        peglevelNew.nShiftLastPart >0 &&
        peglevelNew.nShiftLastTotal >0) {

        int64_t nLastTotal = pdPegPool.fractions.f[nLastIdx];
        int64_t nLastReserve = frReserve.f[nLastIdx];
        int64_t nTakeReserve = nLastReserve;
        nTakeReserve = std::min(nTakeReserve, nLastTotal);
        nTakeReserve = std::min(nTakeReserve, pdPegPool.nReserve);

        pdPegPool.nReserve -= nTakeReserve;
        pdPegPool.fractions.f[nLastIdx] -= nTakeReserve;

        if (nLastReserve > nTakeReserve) { // take it from liquid
            int64_t nDiff = nLastReserve - nTakeReserve;
            frReserve.f[nLastIdx] -= nDiff;
            nReserve -= nDiff;
        }

        // for liquid of partial we need to take proportionally
        // from liquid of the fraction as nLiquid/nLiquidPool
        nLastTotal = pdPegPool.fractions.f[nLastIdx];
        pdPegPool.nReserve = std::min(pdPegPool.nReserve, nLastTotal);

        int64_t nLastLiquid = nLastTotal - pdPegPool.nReserve;
        int64_t nLiquid = nValue - nReserve;
        int64_t nLiquidPool = pdPegPool.fractions.Total() - pdPegPool.nReserve;
        int64_t nTakeLiquid = RatioPart(nLastLiquid,
                                        nLiquid,
                                        nLiquidPool);
        nTakeLiquid = std::min(nTakeLiquid, nLastTotal);

        frLiquid.f[nLastIdx] += nTakeLiquid;
        pdPegPool.fractions.f[nLastIdx] -= nTakeLiquid;
    }

    // liquid is just normed to pool
    int64_t nLiquidTodo = nValue - nReserve - frLiquid.Total();
    int64_t nLiquidPool = pdPegPool.fractions.Total() - pdPegPool.nReserve;
    if (nLiquidTodo > nLiquidPool) { // exchange liquidity mismatch
        std::stringstream ss;
        ss << "Not enough liquid " << nLiquidPool
           << " on 'pool' to balance " << nLiquidTodo;
        sErr = ss.str();
        return false;
    }

    int64_t nHoldLastPart = 0;
    if (pdPegPool.nReserve >0) {
        nHoldLastPart = pdPegPool.fractions.f[nLastIdx];
        pdPegPool.fractions.f[nLastIdx] = 0;
    }

    nLiquidTodo = pdPegPool.fractions.MoveRatioPartTo(nLiquidTodo, frLiquid);

    if (nLiquidTodo >0 && nLiquidTodo <= nHoldLastPart) {
        frLiquid.f[nLastIdx] += nLiquidTodo;
        nHoldLastPart -= nLiquidTodo;
        nLiquidTodo = 0;
    }

    if (nHoldLastPart > 0) {
        pdPegPool.fractions.f[nLastIdx] = nHoldLastPart;
        nHoldLastPart = 0;
    }

    if (nLiquidTodo >0) {
        std::stringstream ss;
        ss << "Liquid not enough after MoveRatioPartTo "
           << nLiquidTodo;
        sErr = ss.str();
        return false;
    }

    pdBalance.fractions = frReserve + frLiquid;
    pdBalance.peglevel = peglevelNew;

    if (nValue != pdBalance.fractions.Total()) {
        std::stringstream ss;
        ss << "Balance mimatch after update " << pdBalance.fractions.Total()
           << " vs " << nValue;
        sErr = ss.str();
        return false;
    }

    int64_t nPegPoolValue = pdPegPool.fractions.Total();
    pdPegPool.nLiquid = nPegPoolValue - pdPegPool.nReserve;
   
    return true;
}

bool movecoins(
        int64_t             nMoveAmount,
        CPegData &          pdSrc,
        CPegData &          pdDst,
        const CPegLevel &   peglevel,
        bool                fCrossCycles,

        std::string &   sErr)
{
    if (!fCrossCycles && peglevel != pdSrc.peglevel) {
        std::stringstream ss;
        ss << "Outdated 'src' of cycle of " << pdSrc.peglevel.nCycle
           << ", current " << peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    if (nMoveAmount <0) {
        std::stringstream ss;
        ss << "Requested to move negative " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    int64_t nSrcValue = pdSrc.fractions.Total();
    if (nSrcValue < nMoveAmount) {
        std::stringstream ss;
        ss << "Not enough amount " << nSrcValue
           << " on 'src' to move " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    if (!fCrossCycles && peglevel != pdDst.peglevel) {
        std::stringstream ss;
        ss << "Outdated 'dst' of cycle of " << pdDst.peglevel.nCycle
           << ", current " << peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    int64_t nIn = pdSrc.fractions.Total() + pdDst.fractions.Total();

    CFractions frAmount = pdSrc.fractions;
    CFractions frMove = frAmount.RatioPart(nMoveAmount);

    pdSrc.fractions -= frMove;
    pdDst.fractions += frMove;

    int64_t nOut = pdSrc.fractions.Total() + pdDst.fractions.Total();

    if (nIn != nOut) {
        std::stringstream ss;
        ss << "Mismatch in and out values " << nIn
           << " vs " << nOut;
        sErr = ss.str();
        return false;
    }

    // std calc values
    pdSrc.nLiquid = pdSrc.fractions.High(peglevel);
    pdSrc.nReserve = pdSrc.fractions.Low(peglevel);
    pdDst.nLiquid = pdDst.fractions.High(peglevel);
    pdDst.nReserve = pdDst.fractions.Low(peglevel);

    if (pdSrc.fractions.Total() != (pdSrc.nLiquid+pdSrc.nReserve)) {
        std::stringstream ss;
        ss << "Mismatch total " 
           << pdSrc.fractions.Total()
           << " vs liquid/reserve 'src' values " 
           << " L:" << pdSrc.nLiquid
           << " R:" << pdSrc.nReserve;
        sErr = ss.str();
        return false;
    }
    
    if (pdDst.fractions.Total() != (pdDst.nLiquid+pdDst.nReserve)) {
        std::stringstream ss;
        ss << "Mismatch total " 
           << pdDst.fractions.Total()
           << " vs liquid/reserve 'src' values " 
           << " L:" << pdDst.nLiquid
           << " R:" << pdDst.nReserve;
        sErr = ss.str();
        return false;
    }
    
    if (pdSrc.peglevel != peglevel) {
        pdSrc.peglevel = peglevel;
    }
    if (pdDst.peglevel != peglevel) {
        pdDst.peglevel = peglevel;
    }
    
    return true;
}

bool moveliquid(
        int64_t             nMoveAmount,
        CPegData &          pdSrc,
        CPegData &          pdDst,
        const CPegLevel &   peglevel,

        std::string &   sErr)
{
    int nSupplyEffective = peglevel.nSupply + peglevel.nShift;
    if (nSupplyEffective <0 &&
        nSupplyEffective >=PEG_SIZE) {
        std::stringstream ss;
        ss << "Supply index out of bounds " << nSupplyEffective;
        sErr = ss.str();
        return false;
    }

    if (peglevel != pdSrc.peglevel) {
        std::stringstream ss;
        ss << "Outdated 'src' of cycle of " << pdSrc.peglevel.nCycle
           << ", current " << peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    if (nMoveAmount <0) {
        std::stringstream ss;
        ss << "Requested to move negative " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    if (pdSrc.nLiquid < nMoveAmount) {
        std::stringstream ss;
        ss << "Not enough liquid " << pdSrc.nLiquid
           << " on 'src' to move " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    if (peglevel != pdDst.peglevel) {
        std::stringstream ss;
        ss << "Outdated 'dst' of cycle of " << pdDst.peglevel.nCycle
           << ", current " << peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    int64_t nIn = pdSrc.fractions.Total() + pdDst.fractions.Total();

    bool fPartial = peglevel.nShiftLastPart >0 && peglevel.nShiftLastTotal >0;
    if (fPartial) {
        nSupplyEffective++;
    }

    CFractions frLiquid = pdSrc.fractions.HighPart(nSupplyEffective, nullptr);

    if (fPartial) {
        int64_t nPartialLiquid = pdSrc.nLiquid - frLiquid.Total();
        if (nPartialLiquid < 0) {
            std::stringstream ss;
            ss << "Mismatch on nPartialLiquid " << nPartialLiquid;
            sErr = ss.str();
            return false;
        }

        frLiquid.f[nSupplyEffective-1] = nPartialLiquid;
    }

    if (frLiquid.Total() < nMoveAmount) {
        std::stringstream ss;
        ss << "Not enough liquid(1) " << frLiquid.Total()
           << " on 'src' to move " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    CFractions frMove = frLiquid.RatioPart(nMoveAmount);

    pdSrc.fractions -= frMove;
    pdDst.fractions += frMove;

    pdSrc.nLiquid -= nMoveAmount;
    pdDst.nLiquid += nMoveAmount;

    int64_t nOut = pdSrc.fractions.Total() + pdDst.fractions.Total();

    if (nIn != nOut) {
        std::stringstream ss;
        ss << "Mismatch in and out values " << nIn
           << " vs " << nOut;
        sErr = ss.str();
        return false;
    }

    if (!pdSrc.fractions.IsPositive()) {
        sErr = "Negative detected in 'src";
        return false;
    }
    
    if (pdSrc.fractions.Total() != (pdSrc.nLiquid+pdSrc.nReserve)) {
        std::stringstream ss;
        ss << "Mismatch total " 
           << pdSrc.fractions.Total()
           << " vs liquid/reserve 'src' values " 
           << " L:" << pdSrc.nLiquid
           << " R:" << pdSrc.nReserve;
        sErr = ss.str();
        return false;
    }
    
    if (pdDst.fractions.Total() != (pdDst.nLiquid+pdDst.nReserve)) {
        std::stringstream ss;
        ss << "Mismatch total " 
           << pdDst.fractions.Total()
           << " vs liquid/reserve 'src' values " 
           << " L:" << pdDst.nLiquid
           << " R:" << pdDst.nReserve;
        sErr = ss.str();
        return false;
    }
    
    return true;
}

bool movereserve(
        int64_t             nMoveAmount,
        CPegData &          pdSrc,
        CPegData &          pdDst,
        const CPegLevel &   peglevel,

        std::string &   sErr)
{
    int nSupplyEffective = peglevel.nSupply + peglevel.nShift;
    if (nSupplyEffective <0 &&
        nSupplyEffective >=PEG_SIZE) {
        std::stringstream ss;
        ss << "Supply index out of bounds " << nSupplyEffective;
        sErr = ss.str();
        return false;
    }

    if (peglevel != pdSrc.peglevel) {
        std::stringstream ss;
        ss << "Outdated 'src' of cycle of " << pdSrc.peglevel.nCycle
           << ", current " << peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    if (nMoveAmount <0) {
        std::stringstream ss;
        ss << "Requested to move negative " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    if (pdSrc.nReserve < nMoveAmount) {
        std::stringstream ss;
        ss << "Not enough reserve " << pdSrc.nReserve
           << " on 'src' to move " << nMoveAmount;
        sErr = ss.str();
        return false;
    }

    if (peglevel != pdDst.peglevel) {
        std::stringstream ss;
        ss << "Outdated 'dst' of cycle of " << pdDst.peglevel.nCycle
           << ", current " << peglevel.nCycle;
        sErr = ss.str();
        return false;
    }

    int64_t nIn = pdSrc.fractions.Total() + pdDst.fractions.Total();

    CFractions frReserve = pdSrc.fractions.LowPart(nSupplyEffective, nullptr);

    bool fPartial = peglevel.nShiftLastPart >0 && peglevel.nShiftLastTotal >0;
    if (fPartial) {
        int64_t nPartialReserve = pdSrc.nReserve - frReserve.Total();
        if (nPartialReserve < 0) {
            std::stringstream ss;
            ss << "Mismatch on nPartialReserve " << nPartialReserve;
            sErr = ss.str();
            return false;
        }

        frReserve.f[nSupplyEffective] = nPartialReserve;
    }

    CFractions frMove = frReserve.RatioPart(nMoveAmount);

    pdSrc.fractions -= frMove;
    pdDst.fractions += frMove;

    pdSrc.nReserve -= nMoveAmount;
    pdDst.nReserve += nMoveAmount;

    int64_t nOut = pdSrc.fractions.Total() + pdDst.fractions.Total();

    if (nIn != nOut) {
        std::stringstream ss;
        ss << "Mismatch in and out values " << nIn
           << " vs " << nOut;
        sErr = ss.str();
        return false;
    }

    if (!pdSrc.fractions.IsPositive()) {
        sErr = "Negative detected in 'src";
        return false;
    }

    if (pdSrc.fractions.Total() != (pdSrc.nLiquid+pdSrc.nReserve)) {
        std::stringstream ss;
        ss << "Mismatch total " 
           << pdSrc.fractions.Total()
           << " vs liquid/reserve 'src' values " 
           << " L:" << pdSrc.nLiquid
           << " R:" << pdSrc.nReserve;
        sErr = ss.str();
        return false;
    }
    
    if (pdDst.fractions.Total() != (pdDst.nLiquid+pdDst.nReserve)) {
        std::stringstream ss;
        ss << "Mismatch total " 
           << pdDst.fractions.Total()
           << " vs liquid/reserve 'src' values " 
           << " L:" << pdDst.nLiquid
           << " R:" << pdDst.nReserve;
        sErr = ss.str();
        return false;
    }
    
    return true;
}

} // namespace
