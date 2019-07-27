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
#include "pegpack.h"
#include "pegdata.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;





void printpegshift(const CFractions & frPegShift,
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
        int64_t nPegShiftLiquid = frPegShift.High(peglevel);
        int64_t nPegShiftReserve = frPegShift.Low(peglevel);
        result.push_back(Pair("pegshift_pegdata", 
                              pegops::packpegdata(frPegShift, peglevel,
                                                  nPegShiftReserve, nPegShiftLiquid)));
    }
}

void printpeglevel(const CPegLevel & peglevel,
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

void printpegbalance(const CFractions & frBalance,
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
        result.push_back(Pair(prefix+"pegdata", 
                              pegops::packpegdata(frBalance, peglevel,
                                                  nReserve, nLiquid)));
    }
}

void printpegbalance(const CFractions & frBalance,
                     int64_t nReserve,
                     int64_t nLiquid,
                     const CPegLevel & peglevel,
                     Object & result,
                     string prefix,
                     bool print_pegdata)
{
    int64_t nValue      = frBalance.Total();
    int64_t nNChange    = frBalance.NChange(peglevel);

    result.push_back(Pair(prefix+"value", nValue));
    result.push_back(Pair(prefix+"liquid", nLiquid));
    result.push_back(Pair(prefix+"reserve", nReserve));
    result.push_back(Pair(prefix+"nchange", nNChange));
    if (print_pegdata) {
        result.push_back(Pair(prefix+"pegdata", pegops::packpegdata(frBalance, peglevel, nReserve, nLiquid)));
    }
}
