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

#include "rpcprotocol.h"

using namespace std;
using namespace boost;
using namespace pegutil;

namespace pegops {

string packpegdata(const CFractions & fractions,
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

bool unpackbalance(

        const string &   pegdata64,
        string           tag,
        
        CFractions &     fractions,
        int64_t &        nReserve,
        int64_t &        nLiquid,
        CPegLevel &      peglevel
        )
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

} // namespace
