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

void unpackpegdata(CFractions & fractions,
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

void unpackbalance(CFractions & fractions,
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
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, 
                                "Can not unpack '"+tag+"' peglevel");
        }
        else peglevel = peglevel_copy;
    
    }
    catch (std::exception &) { ; }
}

bool unpackbalance(CFractions &     fractions,
                   int64_t &        nReserve,
                   int64_t &        nLiquid,
                   CPegLevel &      peglevel,
                   const string &   pegdata64,
                   string           tag)
{
    if (pegdata64.empty()) 
        return true;
    
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
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, 
                               "Can not unpack '"+tag+"' peglevel");
        }
        else peglevel = peglevel_copy;
    
        try {
            finp >> nReserve;
            finp >> nLiquid;
        }
        catch (std::exception &) { 
            nLiquid = fractions.High(peglevel);
            nReserve = fractions.Low(peglevel);
        }
    }
    catch (std::exception &) { ; }
    
    return true;
}

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
        result.push_back(Pair("pegshift_pegdata", packpegdata(frPegShift, peglevel)));
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
        result.push_back(Pair(prefix+"pegdata", packpegdata(frBalance, peglevel)));
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
        result.push_back(Pair(prefix+"pegdata", packpegdata(frBalance, peglevel)));
    }
}

string scripttoaddress(const CScript& scriptPubKey,
                       bool* ptrIsNotary = nullptr,
                       string* ptrNotary = nullptr) {
    int nRequired;
    txnouttype type;
    vector<CTxDestination> addresses;
    if (ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
        std::string str_addr_all;
        bool fNone = true;
        for(const CTxDestination& addr : addresses) {
            std::string str_addr = CBitcoinAddress(addr).ToString();
            if (!str_addr_all.empty())
                str_addr_all += "\n";
            str_addr_all += str_addr;
            fNone = false;
        }
        if (!fNone)
            return str_addr_all;
    }

    if (ptrNotary || ptrIsNotary) {
        if (ptrIsNotary) *ptrIsNotary = false;
        if (ptrNotary) *ptrNotary = "";

        opcodetype opcode1;
        vector<unsigned char> vch1;
        CScript::const_iterator pc1 = scriptPubKey.begin();
        if (scriptPubKey.GetOp(pc1, opcode1, vch1)) {
            if (opcode1 == OP_RETURN && scriptPubKey.size()>1) {
                if (ptrIsNotary) *ptrIsNotary = true;
                if (ptrNotary) {
                    unsigned long len_bytes = scriptPubKey[1];
                    if (len_bytes > scriptPubKey.size()-2)
                        len_bytes = scriptPubKey.size()-2;
                    for (uint32_t i=0; i< len_bytes; i++) {
                        ptrNotary->push_back(char(scriptPubKey[i+2]));
                    }
                }
            }
        }
    }

    string as_bytes;
    unsigned long len_bytes = scriptPubKey.size();
    for(unsigned int i=0; i< len_bytes; i++) {
        as_bytes += char(scriptPubKey[i]);
    }
    return as_bytes;
}
