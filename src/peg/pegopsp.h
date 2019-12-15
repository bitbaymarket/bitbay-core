// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITBAY_PEGOPSP_H
#define BITBAY_PEGOPSP_H

/**
  * Internal API
  * The use of the API requires includes for classes
  * CFractions, CPegLevel, CPegData for declarations.
  */

#include <string>

class CPegData;
class CPegLevel;

namespace pegops {

extern bool getpeglevel(
        int                 nCycleNow,
        int                 nCyclePrev,
        int                 nBuffer,
        int                 nPegNow,
        int                 nPegNext,
        int                 nPegNextNext,
        const CPegData &    pdExchange,
        const CPegData &    pdPegShift,
        
        CPegLevel &     peglevel,
        CPegData &      pdPegPool,
        std::string &   sErr);

extern bool updatepegbalances(
        CPegData &          pdBalance,
        CPegData &          pdPegPool,
        const CPegLevel &   peglevelNew,
        
        std::string &   sErr);

extern bool movecoins(
        int64_t             nMoveAmount,
        CPegData &          pdSrc,
        CPegData &          pdDst,
        const CPegLevel &   peglevel,
        bool                fCrossCycles,

        std::string &   sErr);

extern bool moveliquid(
        int64_t             nMoveAmount,
        CPegData &          pdSrc,
        CPegData &          pdDst,
        const CPegLevel &   peglevel,

        std::string &   sErr);

extern bool movereserve(
        int64_t             nMoveAmount,
        CPegData &          pdSrc,
        CPegData &          pdDst,
        const CPegLevel &   peglevel,

        std::string &   sErr);

}

#endif // BITBAY_PEGOPSP_H
