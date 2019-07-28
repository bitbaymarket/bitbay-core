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

} // namespace
