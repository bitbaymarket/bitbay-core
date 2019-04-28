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
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "registerdeposit <txid:nout> <balance_pegdata_base64> <exchange_pegdata_base64>\n"
            );
    
    string sTxout = params[0].get_str();
    vector<string> vTxoutArgs;
    boost::split(vTxoutArgs, sTxout, boost::is_any_of(":"));
    
    if (vTxoutArgs.size() != 2) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, format txid:nout");
    }
    
    string sTxid = vTxoutArgs[0];
    char * pEnd = nullptr;
    long nout = strtol(vTxoutArgs[1].c_str(), &pEnd, 0);
    if (pEnd == vTxoutArgs[1].c_str()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Txout is not recognized, nout is not number");
    }

    string balance_pegdata64 = params[1].get_str();
    
    CFractions frBalance(0, CFractions::VALUE);
    if (!balance_pegdata64.empty()) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack balance pegdata");
        }
    }
    frBalance = frBalance.Std();

    string exchange_pegdata64 = params[2].get_str();
    
    CFractions frExchange(0, CFractions::VALUE);
    if (!exchange_pegdata64.empty()) {
        string pegdata = DecodeBase64(exchange_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frExchange.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack exchange pegdata");
        }
    }
    frExchange = frExchange.Std();
    
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
    frExchange += frDeposit;
    
    int64_t nBalance = frBalance.Total();
    int64_t nExchange = frExchange.Total();
    
    CDataStream foutBalance(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(foutBalance);

    CDataStream foutExchange(SER_DISK, CLIENT_VERSION);
    frExchange.Pack(foutExchange);
    
    result.push_back(Pair("deposited", true));
    result.push_back(Pair("status", "Registered"));
    result.push_back(Pair("atblock", nRegisterHeight));
    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("cycle", nCycleNow));
    result.push_back(Pair("balance_value", nBalance));
    result.push_back(Pair("balance_liquid", frBalance.High(nSupply)));
    result.push_back(Pair("balance_reserve", frBalance.Low(nSupply)));
    result.push_back(Pair("balance_pegdata", EncodeBase64(foutBalance.str())));
    result.push_back(Pair("exchange_value", nExchange));
    result.push_back(Pair("exchange_liquid", frExchange.High(nSupply)));
    result.push_back(Pair("exchange_reserve", frExchange.Low(nSupply)));
    result.push_back(Pair("exchange_pegdata", EncodeBase64(foutExchange.str())));
    
    return result;
}

Value updatepegbalances(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "updatepegbalances <balance_pegdata_base64>\n"
            );
    
    string balance_pegdata64 = params[0].get_str();
    
    CFractions frBalance(0, CFractions::VALUE);
    if (!balance_pegdata64.empty()) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
             throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack pegdata");
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
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "moveliquid <liquid> <src_pegdata_base64> <dst_pegdata_base64> <pegsupply>\n"
            );
    
    int64_t move_liquid = params[0].get_int64();
    string src_pegdata64 = params[1].get_str();
    string dst_pegdata64 = params[2].get_str();
    int supply = params[3].get_int();
    
    CFractions frSrc(0, CFractions::VALUE);

    string src_pegdata = DecodeBase64(src_pegdata64);
    CDataStream src_finp(src_pegdata.data(), src_pegdata.data() + src_pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frSrc.Unpack(src_finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'src' pegdata");
    }
    int64_t src_liquid = frSrc.High(supply);
    if (src_liquid < move_liquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on 'src' to move %d",
                                     src_liquid,
                                     move_liquid));
    }

    CFractions frDst(0, CFractions::VALUE);

    if (!dst_pegdata64.empty()) {
        string dst_pegdata = DecodeBase64(dst_pegdata64);
        CDataStream dst_finp(dst_pegdata.data(), dst_pegdata.data() + dst_pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frDst.Unpack(dst_finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'dst' pegdata");
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
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "movereserve <reserve> <src_pegdata_base64> <dst_pegdata_base64> <pegsupply>\n"
            );
    
    int64_t move_reserve = params[0].get_int64();
    string src_pegdata64 = params[1].get_str();
    string dst_pegdata64 = params[2].get_str();
    int supply = params[3].get_int();
    
    CFractions frSrc(0, CFractions::VALUE);

    string src_pegdata = DecodeBase64(src_pegdata64);
    CDataStream src_finp(src_pegdata.data(), src_pegdata.data() + src_pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frSrc.Unpack(src_finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'src' pegdata");
    }
    int64_t src_reserve = frSrc.Low(supply);
    if (src_reserve < move_reserve) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough reserve %d on 'src' to move %d",
                                     src_reserve,
                                     move_reserve));
    }

    CFractions frDst(0, CFractions::VALUE);

    if (!dst_pegdata64.empty()) {
        string dst_pegdata = DecodeBase64(dst_pegdata64);
        CDataStream dst_finp(dst_pegdata.data(), dst_pegdata.data() + dst_pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frDst.Unpack(dst_finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'dst' pegdata");
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

Value faucet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "faucet <address>\n"
            );
    
    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid BitBay address");

    Object result;
    bool completed_liquid = true;
    bool completed_reserve = true;
    string status = "Unknown";
    
    int64_t amount = 100000000000;
    CFractions fr(amount, CFractions::VALUE);
    fr = fr.Std();

    int nSupply = pindexBest ? pindexBest->nPegSupplyIndex : 0;
    int64_t liquid = fr.High(nSupply);
    int64_t reserve = fr.Low(nSupply);
    
    if (liquid >0) {
        PegTxType txType = PEG_MAKETX_SEND_LIQUIDITY;
        
        std::vector<std::pair<CScript, int64_t> > vecSend;
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        vecSend.push_back(make_pair(scriptPubKey, liquid));
        
        CWalletTx wtx;
        CReserveKey keyChange(pwalletMain);
        int64_t nFeeRequired = 0;
        bool fCreated = pwalletMain->CreateTransaction(txType, vecSend, wtx, keyChange, nFeeRequired, nullptr);
        if (fCreated) {
            
            bool fCommitted = pwalletMain->CommitTransaction(wtx, keyChange);
            if (fCommitted) {
                completed_liquid = true;
            } else {
                completed_liquid = false;
                status = "Failed to commit a liquid transaction";
            }
            
        } else {
            completed_liquid = false;
            status = "Failed to create a liquid transaction";
        }
    }
    
    if (reserve >0) {
        PegTxType txType = PEG_MAKETX_SEND_RESERVE;
        
        std::vector<std::pair<CScript, int64_t> > vecSend;
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address.Get());
        vecSend.push_back(make_pair(scriptPubKey, reserve));
        
        CWalletTx wtx;
        CReserveKey keyChange(pwalletMain);
        int64_t nFeeRequired = 0;
        bool fCreated = pwalletMain->CreateTransaction(txType, vecSend, wtx, keyChange, nFeeRequired, nullptr);
        if (fCreated) {
            
            bool fCommitted = pwalletMain->CommitTransaction(wtx, keyChange);
            if (fCommitted) {
                completed_reserve = true;
            } else {
                completed_reserve = false;
                status = "Failed to commit a reserve transaction";
            }
            
        } else {
            completed_reserve = false;
            status = "Failed to create a reserve transaction";
        }
    }
    result.push_back(Pair("completed", completed_reserve && completed_liquid));
    result.push_back(Pair("completed_liquid", completed_liquid));
    result.push_back(Pair("completed_reserve", completed_reserve));
    result.push_back(Pair("status", status));
    
    return result;
}

#endif
