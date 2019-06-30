// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITBAY_PEGOPS_H
#define BITBAY_PEGOPS_H

#include <string>

namespace pegops {

extern bool getpeglevel(
        const std::string & inp_exchange_pegdata64,
        const std::string & inp_pegshift_pegdata64,
        int                 inp_cycle_now,
        int                 inp_cycle_prev,
        int                 inp_peg_now,
        int                 inp_peg_next,
        int                 inp_peg_next_next,
        
        std::string &   out_peglevel_hex,
        std::string &   out_pegpool_pegdata64,
        std::string &   out_err);

extern bool getpeglevelinfo(
        const std::string & inp_peglevel_hex,
        
        int &       out_cycle_now,
        int &       out_cycle_prev,
        int &       out_peg_now,
        int &       out_peg_next,
        int &       out_peg_next_next,
        int &       out_shift,
        int64_t &   out_shiftlastpart,
        int64_t &   out_shiftlasttotal);

extern bool updatepegbalances(
        const std::string & inp_balance_pegdata64,
        const std::string & inp_pegpool_pegdata64,
        const std::string & inp_peglevel_hex,
        
        std::string &   out_balance_pegdata64,
        std::string &   out_pegpool_pegdata64,
        std::string &   out_err);

extern bool movecoins(
        int64_t             inp_move_amount,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        bool                inp_cross_cycles,
        
        std::string &   out_src_pegdata64,
        std::string &   out_dst_pegdata64,
        std::string &   out_err);

extern bool moveliquid(
        int64_t             inp_move_liquid,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        
        std::string &   out_src_pegdata64,
        std::string &   out_dst_pegdata64,
        std::string &   out_err);

extern bool movereserve(
        int64_t             inp_move_reserve,
        const std::string & inp_src_pegdata64,
        const std::string & inp_dst_pegdata64,
        const std::string & inp_peglevel_hex,
        
        std::string &   out_src_pegdata64,
        std::string &   out_dst_pegdata64,
        std::string &   out_err);

}

#endif // BITBAY_PEGOPS_H
