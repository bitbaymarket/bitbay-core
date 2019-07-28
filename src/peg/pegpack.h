// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITBAY_PEGPACK_H
#define BITBAY_PEGPACK_H

#include <string>

class CPegLevel;
class CFractions;

namespace pegops {

extern std::string packpegdata(const CFractions &     fractions,
                               const CPegLevel &      peglevel,
                               int64_t                nReserve,
                               int64_t                nLiquid);

}

#endif // BITBAY_PEGPACK_H
