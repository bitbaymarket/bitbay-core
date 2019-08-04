// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pegdata.h"
#include "pegutil.h"

//#include <map>
//#include <set>
//#include <cstdint>
//#include <utility>
//#include <algorithm> 
//#include <type_traits>

//#include <boost/multiprecision/cpp_int.hpp>

//#include <zconf.h>
//#include <zlib.h>

//using namespace std;
//using namespace boost;
//using namespace pegutil;

bool CPegData::Unpack2(CDataStream & finp) {
    try {
        //finp >> nVersion;
        if (!fractions.Unpack(finp)) return false;
        if (!peglevel.Unpack(finp)) return false;
        finp >> nReserve;
        finp >> nLiquid;
        
        // match total
        if ((nReserve+nLiquid) != fractions.Total()) return false;
        
        // validate liquid/reserve match peglevel
        int nSupplyEffective = peglevel.nSupply+peglevel.nShift;
        bool fPartial = peglevel.nShiftLastPart >0 && peglevel.nShiftLastTotal >0;
        if (fPartial) {
            nSupplyEffective++;
            int64_t nLiquidWithoutPartial = fractions.High(nSupplyEffective);
            int64_t nReserveWithoutPartial = fractions.Low(nSupplyEffective-1);
            if (nLiquid < nLiquidWithoutPartial) return false;
            if (nReserve < nReserveWithoutPartial) return false;
        }
        else {
            int64_t nLiquidCalc = fractions.High(nSupplyEffective);
            int64_t nReserveCalc = fractions.Low(nSupplyEffective);
            if (nLiquid != nLiquidCalc) return false;
            if (nReserve != nReserveCalc) return false;
        }
    }
    catch (std::exception &) {
        return false;
    }
    
    fractions = fractions.Std();
    return true;
}

bool CPegData::Unpack1(CDataStream & finp) {

    // try prev version
    try {
        if (!fractions.Unpack1(finp)) return false;
        if (!peglevel.Unpack(finp)) return false;
        finp >> nReserve;
        finp >> nLiquid;
        
        // match total
        if ((nReserve+nLiquid) != fractions.Total()) return false;
        
        // validate liquid/reserve match peglevel
        int nSupplyEffective = peglevel.nSupply+peglevel.nShift;
        bool fPartial = peglevel.nShiftLastPart >0 && peglevel.nShiftLastTotal >0;
        if (fPartial) {
            nSupplyEffective++;
            int64_t nLiquidWithoutPartial = fractions.High(nSupplyEffective);
            int64_t nReserveWithoutPartial = fractions.Low(nSupplyEffective-1);
            if (nLiquid < nLiquidWithoutPartial) return false;
            if (nReserve < nReserveWithoutPartial) return false;
        }
        else {
            int64_t nLiquidCalc = fractions.High(nSupplyEffective);
            int64_t nReserveCalc = fractions.Low(nSupplyEffective);
            if (nLiquid != nLiquidCalc) return false;
            if (nReserve != nReserveCalc) return false;
        }
    }
    catch (std::exception &) {
        return false;
    }
    
    fractions = fractions.Std();
    return true;
}
