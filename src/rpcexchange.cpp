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
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

#ifdef ENABLE_WALLET
Value registerdeposit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 5)
        throw runtime_error(
            "registerdeposit <txid:nout> <balance_liquid> <balance_reserve> <balance_pegdata> <balance_pegsupply>\n"
            );
    
    string sTxout = params[0].get_str();
    vector<string> vTxoutArgs;
    boost::split(vTxoutArgs, sTxout, boost::is_any_of(":"));
    
    if (vTxoutArgs.size() != 2) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, no txid:nout");
    }
    
    string sTxid = vTxoutArgs[0];
    char * pEnd = nullptr;
    long nout = strtol(vTxoutArgs[1].c_str(), &pEnd, 0);
    if (pEnd == vTxoutArgs[1].c_str()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, nout is not number");
    }

    int64_t balance_liquid = params[1].get_int64();
    int64_t balance_reserve = params[2].get_int64();
    string balance_pegdata64 = params[3].get_str();
    int balance_supply = params[4].get_int();
    
    CFractions frBalance(balance_liquid+balance_reserve, CFractions::VALUE);
    if ((balance_liquid+balance_reserve) >0) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
        }
        if (frBalance.High(balance_supply) != balance_liquid) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided liquid balance %d does not match pegdata %d of balance supply %d",
//                                         balance_liquid,
//                                         frBalance.High(balance_supply),
//                                         balance_supply));
            balance_liquid = frBalance.High(balance_supply);
        }
        if (frBalance.Low(balance_supply) != balance_reserve) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided reserve balance %d does not match pegdata %d of balance supply %d",
//                                         balance_reserve,
//                                         frBalance.Low(balance_supply),
//                                         balance_supply));
            balance_reserve = frBalance.Low(balance_supply);
        }
    }
    frBalance = frBalance.Std();
    
    uint256 txhash;
    txhash.SetHex(sTxid);
    
    Object result;
    
    CTxDB txdb("r");
    CTxIndex txindex;
    if (!txdb.ReadTxIndex(txhash, txindex)) {
        throw JSONRPCError(RPC_MISC_ERROR, "ReadTxIndex tx failed");
    }
    CTransaction tx;
    if (!tx.ReadFromDisk(txindex.pos)) {
        throw JSONRPCError(RPC_MISC_ERROR, "ReadFromDisk tx failed");
    }
    if (nout <0 || size_t(nout) >= tx.vout.size()) {
        throw JSONRPCError(RPC_MISC_ERROR, "nout is out of index");
    }
    
    int nDepth = txindex.GetDepthInMainChain();
    int nHeight = nBestHeight - nDepth +1;
    int nPegInterval = Params().PegInterval(nHeight);
    int nNextHeight = (nHeight / nPegInterval +1) * nPegInterval;
    int nRegisterHeight = std::max(nNextHeight, nHeight + nRecommendedConfirmations);
    int nCycleNow = nBestHeight / nPegInterval;

    if (nDepth < nRecommendedConfirmations) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", strprintf("Need at least %d confirmations", nRecommendedConfirmations)));
        return result;
    }

    if (nRegisterHeight > nBestHeight) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", strprintf("Need to wait for registration at %d block", nRegisterHeight)));
        return result;
    }

    int nSupply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    
    CPegDB pegdb("r");
    auto fkey = uint320(txhash, nout);
    CFractions frDeposit(tx.vout[nout].nValue, CFractions::VALUE);
    if (!pegdb.ReadFractions(fkey, frDeposit, false)) {
        result.push_back(Pair("deposited", false));
        result.push_back(Pair("atblock", nRegisterHeight));
        result.push_back(Pair("status", "No peg data read failed"));
        return result;
    }
    
    frDeposit = frDeposit.Std();
    frBalance += frDeposit;
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout);

    result.push_back(Pair("deposited", true));
    result.push_back(Pair("atblock", nRegisterHeight));
    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("cycle", nCycleNow));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(nSupply)));
    result.push_back(Pair("reserve", frBalance.Low(nSupply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout.str())));
    result.push_back(Pair("status", "Registered"));
    
    return result;
}

Value updatepegbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "updatepegbalances <balance_liquid> <balance_reserve> <balance_pegdata> <balance_pegsupply>\n"
            );
    
    int64_t balance_liquid = params[0].get_int64();
    int64_t balance_reserve = params[1].get_int64();
    string balance_pegdata64 = params[2].get_str();
    int balance_supply = params[3].get_int();
    
    CFractions frBalance(balance_liquid+balance_reserve, CFractions::VALUE);
    if ((balance_liquid+balance_reserve) >0) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
        }
        if (frBalance.High(balance_supply) != balance_liquid) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided liquid balance %d does not match pegdata %d of balance supply %d",
//                                         balance_liquid,
//                                         frBalance.High(balance_supply),
//                                         balance_supply));
            balance_liquid = frBalance.High(balance_supply);
        }
        if (frBalance.Low(balance_supply) != balance_reserve) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided reserve balance %d does not match pegdata %d of balance supply %d",
//                                         balance_reserve,
//                                         frBalance.Low(balance_supply),
//                                         balance_supply));
            balance_reserve = frBalance.Low(balance_supply);
        }
    }
    frBalance = frBalance.Std();
    
    Object result;
    
    int nPegInterval = Params().PegInterval(nBestHeight);
    int nCycleNow = nBestHeight / nPegInterval;

    int nSupply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout);

    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("cycle", nCycleNow));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(nSupply)));
    result.push_back(Pair("reserve", frBalance.Low(nSupply)));
    result.push_back(Pair("pegdata", EncodeBase64(fout.str())));
    
    return result;
}

Value moveliquid(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 8)
        throw runtime_error(
            "moveliquid <liquid> <src_liquid> <src_reserve> <src_pegdata> <dst_liquid> <dst_reserve> <dst_pegdata> <pegsupply>\n"
            );
    
    int64_t move_liquid = params[0].get_int64();
    int64_t src_liquid = params[1].get_int64();
    int64_t src_reserve = params[2].get_int64();
    string src_pegdata64 = params[3].get_str();
    int64_t dst_liquid = params[4].get_int64();
    int64_t dst_reserve = params[5].get_int64();
    string dst_pegdata64 = params[6].get_str();
    int supply = params[7].get_int();
    
    CFractions frSrc(src_liquid+src_reserve, CFractions::VALUE);

    string src_pegdata = DecodeBase64(src_pegdata64);
    CDataStream src_finp(src_pegdata.data(), src_pegdata.data() + src_pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frSrc.Unpack(src_finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'from' pegdata");
    }
    if (frSrc.High(supply) != src_liquid) {
//        throw JSONRPCError(RPC_MISC_ERROR, 
//                           strprintf("Provided liquid balance %d does not match pegdata %d of 'from' supply %d",
//                                     src_liquid,
//                                     frSrc.High(supply),
//                                     supply));
        src_liquid = frSrc.High(supply);
    }
    if (frSrc.Low(supply) != src_reserve) {
//        throw JSONRPCError(RPC_MISC_ERROR, 
//                           strprintf("Provided reserve balance %d does not match pegdata %d of 'from' supply %d",
//                                     src_reserve,
//                                     frSrc.Low(supply),
//                                     supply));
        src_reserve = frSrc.Low(supply);
    }
    if (src_liquid < move_liquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on 'from' to move %d",
                                     src_liquid,
                                     move_liquid));
    }

    CFractions frDst(dst_liquid+dst_reserve, CFractions::VALUE);

    if (dst_liquid+dst_reserve != 0) {
        string dst_pegdata = DecodeBase64(dst_pegdata64);
        CDataStream dst_finp(dst_pegdata.data(), dst_pegdata.data() + dst_pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frDst.Unpack(dst_finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'to' pegdata");
        }
        if (frDst.High(supply) != dst_liquid) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided liquid balance %d does not match pegdata %d of 'to' supply %d",
//                                         dst_liquid,
//                                         frDst.High(supply),
//                                         supply));
            dst_liquid = frDst.High(supply);
        }
        if (frDst.Low(supply) != dst_reserve) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided reserve balance %d does not match pegdata %d of 'to' supply %d",
//                                         dst_reserve,
//                                         frDst.Low(supply),
//                                         supply));
            dst_reserve = frDst.Low(supply);
        }
    }
    
    frSrc = frSrc.Std();
    CFractions frLiquid = frSrc.HighPart(supply, nullptr);
    CFractions frMove = frLiquid.RatioPart(move_liquid, src_liquid, supply);
    
    frSrc -= frMove;
    frDst += frMove;
    
    Object result;
        
    int64_t src_value = frSrc.Total();
    int64_t dst_value = frDst.Total();
    
    CDataStream fout_dst(SER_DISK, CLIENT_VERSION);
    frDst.Pack(fout_dst);
    CDataStream fout_src(SER_DISK, CLIENT_VERSION);
    frSrc.Pack(fout_src);

    result.push_back(Pair("supply", supply));
    result.push_back(Pair("src_value", src_value));
    result.push_back(Pair("src_liquid", frSrc.High(supply)));
    result.push_back(Pair("src_reserve", frSrc.Low(supply)));
    result.push_back(Pair("src_pegdata", EncodeBase64(fout_src.str())));
    result.push_back(Pair("dst_value", dst_value));
    result.push_back(Pair("dst_liquid", frDst.High(supply)));
    result.push_back(Pair("dst_reserve", frDst.Low(supply)));
    result.push_back(Pair("dst_pegdata", EncodeBase64(fout_dst.str())));
    
    return result;
}

Value movereserve(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 8)
        throw runtime_error(
            "movereserve <reserve> <src_liquid> <src_reserve> <src_pegdata> <dst_liquid> <dst_reserve> <dst_pegdata> <pegsupply>\n"
            );
    
    int64_t move_reserve = params[0].get_int64();
    int64_t src_liquid = params[1].get_int64();
    int64_t src_reserve = params[2].get_int64();
    string src_pegdata64 = params[3].get_str();
    int64_t dst_liquid = params[4].get_int64();
    int64_t dst_reserve = params[5].get_int64();
    string dst_pegdata64 = params[6].get_str();
    int supply = params[7].get_int();
    
    CFractions frSrc(src_liquid+src_reserve, CFractions::VALUE);

    string src_pegdata = DecodeBase64(src_pegdata64);
    CDataStream src_finp(src_pegdata.data(), src_pegdata.data() + src_pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frSrc.Unpack(src_finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'from' pegdata");
    }
    if (frSrc.High(supply) != src_liquid) {
//        throw JSONRPCError(RPC_MISC_ERROR, 
//                           strprintf("Provided liquid balance %d does not match pegdata %d of 'from' supply %d",
//                                     src_liquid,
//                                     frSrc.High(supply),
//                                     supply));
        src_liquid = frSrc.High(supply);
    }
    if (frSrc.Low(supply) != src_reserve) {
//        throw JSONRPCError(RPC_MISC_ERROR, 
//                           strprintf("Provided reserve balance %d does not match pegdata %d of 'from' supply %d",
//                                     src_reserve,
//                                     frSrc.Low(supply),
//                                     supply));
        src_reserve = frSrc.Low(supply);
    }
    if (src_reserve < move_reserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough reserve %d on 'from' to move %d",
                                     src_reserve,
                                     move_reserve));
    }

    CFractions frDst(dst_liquid+dst_reserve, CFractions::VALUE);

    if (dst_liquid+dst_reserve != 0) {
        string dst_pegdata = DecodeBase64(dst_pegdata64);
        CDataStream dst_finp(dst_pegdata.data(), dst_pegdata.data() + dst_pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frDst.Unpack(dst_finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'to' pegdata");
        }
        if (frDst.High(supply) != dst_liquid) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided liquid balance %d does not match pegdata %d of 'to' supply %d",
//                                         dst_liquid,
//                                         frDst.High(supply),
//                                         supply));
            dst_liquid = frDst.High(supply);
        }
        if (frDst.Low(supply) != dst_reserve) {
//            throw JSONRPCError(RPC_MISC_ERROR, 
//                               strprintf("Provided reserve balance %d does not match pegdata %d of 'to' supply %d",
//                                         dst_reserve,
//                                         frDst.Low(supply),
//                                         supply));
            dst_reserve = frDst.Low(supply);
        }
    }
    
    frSrc = frSrc.Std();
    CFractions frReserve = frSrc.LowPart(supply, nullptr);
    CFractions frMove = frReserve.RatioPart(move_reserve, src_reserve, 0);
    
    frSrc -= frMove;
    frDst += frMove;
    
    Object result;
        
    int64_t src_value = frSrc.Total();
    int64_t dst_value = frDst.Total();
    
    CDataStream fout_dst(SER_DISK, CLIENT_VERSION);
    frDst.Pack(fout_dst);
    CDataStream fout_src(SER_DISK, CLIENT_VERSION);
    frSrc.Pack(fout_src);

    result.push_back(Pair("supply", supply));
    result.push_back(Pair("src_value", src_value));
    result.push_back(Pair("src_liquid", frSrc.High(supply)));
    result.push_back(Pair("src_reserve", frSrc.Low(supply)));
    result.push_back(Pair("src_pegdata", EncodeBase64(fout_src.str())));
    result.push_back(Pair("dst_value", dst_value));
    result.push_back(Pair("dst_liquid", frDst.High(supply)));
    result.push_back(Pair("dst_reserve", frDst.Low(supply)));
    result.push_back(Pair("dst_pegdata", EncodeBase64(fout_dst.str())));
    
    return result;
}

#endif
