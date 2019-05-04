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
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    
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
    result.push_back(Pair("balance_nchange", frBalance.Change(nSupply, nSupplyNext)));
    result.push_back(Pair("balance_pegdata", EncodeBase64(foutBalance.str())));
    result.push_back(Pair("exchange_value", nExchange));
    result.push_back(Pair("exchange_liquid", frExchange.High(nSupply)));
    result.push_back(Pair("exchange_reserve", frExchange.Low(nSupply)));
    result.push_back(Pair("exchange_nchange", frExchange.Change(nSupply, nSupplyNext)));
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
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    
    int64_t nBalance = frBalance.Total();
    
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout);

    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("cycle", nCycleNow));
    result.push_back(Pair("value", nBalance));
    result.push_back(Pair("liquid", frBalance.High(nSupply)));
    result.push_back(Pair("reserve", frBalance.Low(nSupply)));
    result.push_back(Pair("nchange", frBalance.Change(nSupply, nSupplyNext)));
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
    int nSupply = params[3].get_int();
    
    CFractions frSrc(0, CFractions::VALUE);

    string src_pegdata = DecodeBase64(src_pegdata64);
    CDataStream src_finp(src_pegdata.data(), src_pegdata.data() + src_pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frSrc.Unpack(src_finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'src' pegdata");
    }
    int64_t src_liquid = frSrc.High(nSupply);
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
    CFractions frLiquid = frSrc.HighPart(nSupply, nullptr);
    CFractions frMove = frLiquid.RatioPart(move_liquid, src_liquid, nSupply);
    
    frSrc -= frMove;
    frDst += frMove;
    
    Object result;
        
    int64_t src_value = frSrc.Total();
    int64_t dst_value = frDst.Total();
    
    CDataStream fout_dst(SER_DISK, CLIENT_VERSION);
    frDst.Pack(fout_dst);
    CDataStream fout_src(SER_DISK, CLIENT_VERSION);
    frSrc.Pack(fout_src);

    // bypassed nSupply is supposed to be current supply, we use current nSupplyNext
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    
    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("src_value", src_value));
    result.push_back(Pair("src_liquid", frSrc.High(nSupply)));
    result.push_back(Pair("src_reserve", frSrc.Low(nSupply)));
    result.push_back(Pair("src_nchange", frSrc.Change(nSupply, nSupplyNext)));
    result.push_back(Pair("src_pegdata", EncodeBase64(fout_src.str())));
    result.push_back(Pair("dst_value", dst_value));
    result.push_back(Pair("dst_liquid", frDst.High(nSupply)));
    result.push_back(Pair("dst_reserve", frDst.Low(nSupply)));
    result.push_back(Pair("dst_nchange", frDst.Change(nSupply, nSupplyNext)));
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
    int nSupply = params[3].get_int();
    
    CFractions frSrc(0, CFractions::VALUE);

    string src_pegdata = DecodeBase64(src_pegdata64);
    CDataStream src_finp(src_pegdata.data(), src_pegdata.data() + src_pegdata.size(),
                     SER_DISK, CLIENT_VERSION);
    if (!frSrc.Unpack(src_finp)) {
         throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'src' pegdata");
    }
    int64_t src_reserve = frSrc.Low(nSupply);
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
    CFractions frReserve = frSrc.LowPart(nSupply, nullptr);
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

    // bypassed nSupply is supposed to be current supply, we use current nSupplyNext
    int nSupplyNext = pindexBest ? pindexBest->GetNextIntervalPegSupplyIndex() : 0;
    
    result.push_back(Pair("supply", nSupply));
    result.push_back(Pair("src_value", src_value));
    result.push_back(Pair("src_liquid", frSrc.High(nSupply)));
    result.push_back(Pair("src_reserve", frSrc.Low(nSupply)));
    result.push_back(Pair("src_nchange", frSrc.Change(nSupply, nSupplyNext)));
    result.push_back(Pair("src_pegdata", EncodeBase64(fout_src.str())));
    result.push_back(Pair("dst_value", dst_value));
    result.push_back(Pair("dst_liquid", frDst.High(nSupply)));
    result.push_back(Pair("dst_reserve", frDst.Low(nSupply)));
    result.push_back(Pair("dst_nchange", frDst.Change(nSupply, nSupplyNext)));
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
        
        vector<pair<CScript, int64_t> > vecSend;
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
        
        vector<pair<CScript, int64_t> > vecSend;
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

class CCoinToUse
{
public:
    uint256     txhash;
    uint64_t    i;
    int64_t     nValue;
    int64_t     nAvailableValue;
    CScript     scriptPubKey;

    CCoinToUse() : i(0),nValue(0),nAvailableValue(0) {}
    
    friend bool operator<(const CCoinToUse &a, const CCoinToUse &b) { 
        if (a.txhash < b.txhash) return true;
        if (a.txhash == b.txhash && a.i < b.i) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue < b.nValue) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue == b.nValue && a.nAvailableValue < b.nAvailableValue) return true;
        if (a.txhash == b.txhash && a.i == b.i && a.nValue == b.nValue && a.nAvailableValue == b.nAvailableValue && a.scriptPubKey < b.scriptPubKey) return true;
        return false;
    }
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(txhash);
        READWRITE(i);
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    )
};

static bool sortByAddress(const CCoinToUse &lhs, const CCoinToUse &rhs) { 
    CScript lhs_script = lhs.scriptPubKey;
    CScript rhs_script = rhs.scriptPubKey;
    
    CTxDestination lhs_dst;
    CTxDestination rhs_dst;
    bool lhs_ok1 = ExtractDestination(lhs_script, lhs_dst);
    bool rhs_ok1 = ExtractDestination(rhs_script, rhs_dst);
    
    if (!lhs_ok1 || !rhs_ok1) {
        if (lhs_ok1 == rhs_ok1) 
            return lhs_script < rhs_script;
        return lhs_ok1 < rhs_ok1;
    }
    
    string lhs_addr = CBitcoinAddress(lhs_dst).ToString();
    string rhs_addr = CBitcoinAddress(rhs_dst).ToString();
    
    return lhs_addr < rhs_addr;
}
static bool sortByDestination(const CTxDestination &lhs, const CTxDestination &rhs) { 
    string lhs_addr = CBitcoinAddress(lhs).ToString();
    string rhs_addr = CBitcoinAddress(rhs).ToString();
    return lhs_addr < rhs_addr;
}

Value prepareliquidwithdraw(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 9)
        throw runtime_error(
            "prepareliquidwithdraw "
                "<balance_pegdata_base64> "
                "<exchange_pegdata_base64> "
                "<distortion_pegdata_base64> "
                "<amount_with_fee> "
                "<address> "
                "<supplynow> "
                "<supplynext> "
                "<consumed_inputs> "
                "<provided_outputs>\n"
            );
    
    string balance_pegdata64 = params[0].get_str();
    string exchange_pegdata64 = params[1].get_str();
    string distortion_pegdata64 = params[2].get_str();
    int64_t nAmountWithFee = params[3].get_int64();

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid BitBay address");
    
    int nSupplyNow = params[5].get_int();
    int nSupplyNext = params[6].get_int();
    
    CFractions frBalance(0, CFractions::VALUE);
    if (!balance_pegdata64.empty()) {
        string pegdata = DecodeBase64(balance_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frBalance.Unpack(finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'balance' pegdata");
        }
    }

    CFractions frExchange(0, CFractions::VALUE);
    if (!exchange_pegdata64.empty()) {
        string pegdata = DecodeBase64(exchange_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frExchange.Unpack(finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'exchange' pegdata");
        }
    }

    CFractions frDistortion(0, CFractions::VALUE);
    if (!distortion_pegdata64.empty()) {
        string pegdata = DecodeBase64(distortion_pegdata64);
        CDataStream finp(pegdata.data(), pegdata.data() + pegdata.size(),
                         SER_DISK, CLIENT_VERSION);
        if (!frDistortion.Unpack(finp)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Can not unpack 'distortion' pegdata");
        }
    }
    
    int64_t nBalanceLiquid = 0;
    CFractions frBalanceLiquid = frBalance.HighPart(nSupplyNext, &nBalanceLiquid);
    if (nAmountWithFee > nBalanceLiquid) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid %d on 'balance' to withdraw %d",
                                     nBalanceLiquid,
                                     nAmountWithFee));
    }
    CFractions frAmount = frBalanceLiquid.RatioPart(nAmountWithFee, nBalanceLiquid, nSupplyNext);

    // inputs, outputs
    string sConsumedInputs = params[7].get_str();
    string sProvidedOutputs = params[8].get_str();

    set<uint320> setConsumedInputs;
    map<uint320,CCoinToUse> mapProvidedOutputs;
    vector<string> vConsumedInputsArgs;
    vector<string> vProvidedOutputsArgs;
    boost::split(vConsumedInputsArgs, sConsumedInputs, boost::is_any_of(","));
    boost::split(vProvidedOutputsArgs, sProvidedOutputs, boost::is_any_of(","));
    for(string sConsumedInput : vConsumedInputsArgs) {
        setConsumedInputs.insert(uint320(sConsumedInput));
    }
    for(string sProvidedOutput : vProvidedOutputsArgs) {
        vector<unsigned char> outData(ParseHex(sProvidedOutput));
        CDataStream ssData(outData, SER_NETWORK, PROTOCOL_VERSION);
        CCoinToUse out;
        try { ssData >> out; }
        catch (std::exception &) { continue; }
        mapProvidedOutputs[uint320(out.txhash, out.i)] = out;
    }
    
    if (!pindexBest) {
        throw JSONRPCError(RPC_MISC_ERROR, "Blockchain is not in sync");
    }
    
    assert(pwalletMain != NULL);
   
    CTxDB txdb("r");
    CPegDB pegdb("r");
    
    // make list of 'rated' outputs, multimap with key 'distortion'
    // they are rated to be less distorted towards coins to withdraw
    
    map<uint320,CCoinToUse> mapAllOutputs = mapProvidedOutputs;
    set<uint320> setWalletOutputs;
    
    map<uint320,int64_t> mapAvailableLiquid;
    
    vector<COutput> vecCoins;
    pwalletMain->AvailableCoins(vecCoins, false, true, NULL);
    for(const COutput& coin : vecCoins)
    {
        auto txhash = coin.tx->GetHash();
        auto fkey = uint320(txhash, coin.i);
        setWalletOutputs.insert(fkey);
        if (setConsumedInputs.count(fkey)) continue; // already used
        CCoinToUse & out = mapAllOutputs[fkey];
        out.i = coin.i;
        out.txhash = txhash;
        out.nValue = coin.tx->vout[coin.i].nValue;
        out.scriptPubKey = coin.tx->vout[coin.i].scriptPubKey;
    }
    
    // clean-up consumed, intersect with wallet
    set<uint320> setConsumedInputsNew;
    std::set_intersection(setConsumedInputs.begin(), setConsumedInputs.end(),
                          setWalletOutputs.begin(), setWalletOutputs.end(),
                          std::inserter(setConsumedInputsNew,setConsumedInputsNew.begin()));
    sConsumedInputs.clear();
    for(const uint320& fkey : setConsumedInputsNew) {
        if (!sConsumedInputs.empty()) sConsumedInputs += ",";
        sConsumedInputs += fkey.GetHex();
    }
    
    // clean-up provided, remove what is already in wallet
    map<uint320,CCoinToUse> mapProvidedOutputsNew;
    for(const pair<uint320,CCoinToUse> & item : mapProvidedOutputs) {
        if (setWalletOutputs.count(item.first)) continue;
        mapProvidedOutputsNew.insert(item);
    }
    sProvidedOutputs.clear();
    for(const pair<uint320,CCoinToUse> & item : mapProvidedOutputsNew) {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << item.second;
        if (!sProvidedOutputs.empty()) sProvidedOutputs += ",";
        sProvidedOutputs += HexStr(ss.begin(), ss.end());
    }
    
    // read avaialable coin fractions to rate
    multimap<double,CCoinToUse> ratedOutputs;
    for(const pair<uint320,CCoinToUse>& item : mapAllOutputs) {
        uint320 fkey = item.first;
        CFractions frOut(0, CFractions::VALUE);
        if (!pegdb.ReadFractions(fkey, frOut, true)) {
            if (!mempool.lookup(fkey.b1(), fkey.b2(), frOut)) {
                continue;
            }
        }
        
        int64_t nAvailableLiquid = 0;
        frOut = frOut.HighPart(nSupplyNext, &nAvailableLiquid);
        
        double distortion = frOut.Distortion(frAmount);
        ratedOutputs.insert(pair<double,CCoinToUse>(distortion, item.second));
        mapAvailableLiquid[fkey] = nAvailableLiquid;
    }

    // get available value for selected coins
    set<CCoinToUse> setCoins;
    int64_t nLeftAmount = nAmountWithFee;
    auto it = ratedOutputs.begin();
    for (; it != ratedOutputs.end(); ++it) {
        CCoinToUse out = (*it).second;
        auto txhash = out.txhash;
        auto fkey = uint320(txhash, out.i);
        
        nLeftAmount -= mapAvailableLiquid[fkey];
        out.nAvailableValue = mapAvailableLiquid[fkey];
        setCoins.insert(out);
        
        if (nLeftAmount <= 0) {
            break;
        }
    }
    
    if (nLeftAmount > 0) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Not enough liquid on 'exchange' to withdraw %d",
                                     nAmountWithFee));
    }
    
    int64_t nFeeRet = 1000000 /*temp fee*/;
    int64_t nAmount = nAmountWithFee - nFeeRet;
    
    vector<pair<CScript, int64_t> > vecSend;
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address.Get());
    vecSend.push_back(make_pair(scriptPubKey, nAmount));
    
    int64_t nValue = 0;
    for(const pair<CScript, int64_t>& s : vecSend) {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    
    size_t nNumInputs = 1;

    CTransaction rawTx;
    
    nNumInputs = setCoins.size();
    if (!nNumInputs) return false;
    
    // Inputs to be sorted by address
    vector<CCoinToUse> vCoins;
    for(const CCoinToUse& coin : setCoins) {
        vCoins.push_back(coin);
    }
    sort(vCoins.begin(), vCoins.end(), sortByAddress);
    
    // Collect input addresses
    // Prepare maps for input,available,take
    set<CTxDestination> setInputAddresses;
    vector<CTxDestination> vInputAddresses;
    map<CTxDestination, int64_t> mapAvailableValuesAt;
    map<CTxDestination, int64_t> mapInputValuesAt;
    map<CTxDestination, int64_t> mapTakeValuesAt;
    int64_t nValueToTakeFromChange = 0;
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        setInputAddresses.insert(address); // sorted due to vCoins
        mapAvailableValuesAt[address] = 0;
        mapInputValuesAt[address] = 0;
        mapTakeValuesAt[address] = 0;
    }
    // Get sorted list of input addresses
    for(const CTxDestination& address : setInputAddresses) {
        vInputAddresses.push_back(address);
    }
    sort(vInputAddresses.begin(), vInputAddresses.end(), sortByDestination);
    // Input and available values can be filled in
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        int64_t& nValueAvailableAt = mapAvailableValuesAt[address];
        nValueAvailableAt += coin.nAvailableValue;
        int64_t& nValueInputAt = mapInputValuesAt[address];
        nValueInputAt += coin.nValue;
    }
            
    // vouts to the payees
    BOOST_FOREACH (const PAIRTYPE(CScript, int64_t)& s, vecSend)
        rawTx.vout.push_back(CTxOut(s.second, s.first));
    
    CReserveKey reservekey(pwalletMain);
    reservekey.ReturnKey();

    // Available values - liquidity
    // Compute values to take from each address (liquidity is common)
    int64_t nValueLeft = nValue;
    for(const CCoinToUse& coin : vCoins) {
        CTxDestination address;
        if(!ExtractDestination(coin.scriptPubKey, address))
            continue;
        int64_t nValueAvailable = coin.nAvailableValue;
        int64_t nValueTake = nValueAvailable;
        if (nValueTake > nValueLeft) {
            nValueTake = nValueLeft;
        }
        int64_t& nValueTakeAt = mapTakeValuesAt[address];
        nValueTakeAt += nValueTake;
        nValueLeft -= nValueTake;
    }
    
    // Calculate change (minus fee and part taken from change)
    int64_t nTakeFromChangeLeft = nValueToTakeFromChange + nFeeRet;
    for (const CTxDestination& address : vInputAddresses) {
        CScript scriptPubKey;
        scriptPubKey.SetDestination(address);
        int64_t nValueTake = mapTakeValuesAt[address];
        int64_t nValueInput = mapInputValuesAt[address];
        int64_t nValueChange = nValueInput - nValueTake;
        if (nValueChange > nTakeFromChangeLeft) {
            nValueChange -= nTakeFromChangeLeft;
            nTakeFromChangeLeft = 0;
        }
        if (nValueChange < nTakeFromChangeLeft) {
            nTakeFromChangeLeft -= nValueChange;
            nValueChange = 0;
        }
        if (nValueChange == 0) continue;
        rawTx.vout.push_back(CTxOut(nValueChange, scriptPubKey));
    }
    
    // Fill vin
    for(const CCoinToUse& coin : vCoins) {
        rawTx.vin.push_back(CTxIn(coin.txhash,coin.i));
    }
    
    // Calculate peg
    MapPrevOut mapInputs;
    MapPrevTx mapTxInputs;
    MapFractions mapInputsFractions;
    MapFractions mapOutputFractions;
    CFractions feesFractions(0, CFractions::STD);
    string sPegFailCause;
    
    size_t n_vin = rawTx.vin.size();
        
    for (unsigned int i = 0; i < n_vin; i++)
    {
        const COutPoint & prevout = rawTx.vin[i].prevout;
        auto fkey = uint320(prevout.hash, prevout.n);
        
        // Read txindex
        CTxIndex& txindex = mapTxInputs[prevout.hash].first;
        if (!txdb.ReadTxIndex(prevout.hash, txindex)) {
            // todo prevout via input args
            continue;
        }
        // Read txPrev
        CTransaction& txPrev = mapTxInputs[prevout.hash].second;
        if (!txPrev.ReadFromDisk(txindex.pos)) {
            // todo prevout via input args
            continue;
        }  
        
        if (prevout.n >= txPrev.vout.size()) {
            // error?
            continue;
        }

        mapInputs[fkey] = txPrev.vout[prevout.n];
        
        CFractions& fractions = mapInputsFractions[fkey];
        fractions = CFractions(mapInputs[fkey].nValue, CFractions::VALUE);
        pegdb.ReadFractions(fkey, fractions);
    }
    
    bool peg_ok = CalculateStandardFractions(rawTx, 
                                             nSupplyNext,
                                             pindexBest->nTime,
                                             mapInputs, 
                                             mapInputsFractions,
                                             mapOutputFractions,
                                             feesFractions,
                                             sPegFailCause);
    if (!peg_ok) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Fail on calculations of tx fractions (cause=%s)",
                                     sPegFailCause.c_str()));
    }
    
    string txhash = rawTx.GetHash().GetHex();
    
    // without payment is the first out
    auto fkey = uint320(rawTx.GetHash(), 0);
    if (!mapOutputFractions.count(fkey)) {
        throw JSONRPCError(RPC_MISC_ERROR, "No withdraw fractions");
    }
    CFractions frProcessed = mapOutputFractions[fkey] + feesFractions;
    CFractions frRequested = frAmount;

    if (frRequested.Total() != nAmountWithFee) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch requested and amount_with_fee (%d - %d)",
                                     frRequested.Total(), nAmountWithFee));
    }
    if (frProcessed.Total() != nAmountWithFee) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch processed and amount_with_fee (%d - %d)",
                                     frProcessed.Total(), nAmountWithFee));
    }
    
    // get list of consumed inputs
    for (size_t i=0; i< rawTx.vin.size(); i++) {
        const COutPoint & prevout = rawTx.vin[i].prevout;
        auto fkey = uint320(prevout.hash, prevout.n);
        if (!sConsumedInputs.empty()) sConsumedInputs += ",";
        sConsumedInputs += fkey.GetHex();
    }
    // get list of provided outputs and save fractions
    {
        CPegDB pegdbrw;
        for (size_t i=1; i< rawTx.vout.size(); i++) { // skip 0 (withdraw)
            auto fkey = uint320(rawTx.GetHash(), i);
            // save these outputs in pegdb, so they can be used in next withdraws
            pegdbrw.WriteFractions(fkey, mapOutputFractions[fkey]);
            // serialize to sProvidedOutputs
            CCoinToUse out;
            out.i = i;
            out.txhash = rawTx.GetHash();
            out.nValue = rawTx.vout[i].nValue;
            out.scriptPubKey = rawTx.vout[i].scriptPubKey;
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << out;
            if (!sProvidedOutputs.empty()) sProvidedOutputs += ",";
            sProvidedOutputs += HexStr(ss.begin(), ss.end());
        }
    }
    
    frBalance -= frRequested;
    frExchange -= frRequested;
    frDistortion += (frRequested - frProcessed);
    
    // first to consume distortion by balance
    // as computation were completed by nSupplyNext it may use fractions
    // of current reserves - at current supply not to consume these fractions
    int64_t nDistortionPositive = 0;
    int64_t nDistortionNegative = 0;
    CFractions frDistortionNow = frDistortion.HighPart(nSupplyNow, nullptr);
    CFractions frDistortionPositive = frDistortionNow.Positive(&nDistortionPositive);
    CFractions frDistortionNegative = frDistortionNow.Negative(&nDistortionNegative);
    CFractions frDistortionNegativeConsume = frDistortionNegative & (-frBalance);
    int64_t nDistortionNegativeConsume = frDistortionNegativeConsume.Total();
    int64_t nDistortionPositiveConsume = frDistortionPositive.Total();
    if ((-nDistortionNegativeConsume) > nDistortionPositiveConsume) {
        CFractions frToPositive = -frDistortionNegativeConsume; 
        frToPositive = frToPositive.RatioPart(nDistortionPositiveConsume,
                                              (-nDistortionNegativeConsume),
                                              nSupplyNow);
        frDistortionNegativeConsume = -frToPositive;
        nDistortionNegativeConsume = frDistortionNegativeConsume.Total();
    }
    nDistortionPositiveConsume = -nDistortionNegativeConsume;
    CFractions frDistortionPositiveConsume = frDistortionPositive.RatioPart(nDistortionPositiveConsume, nDistortionPositive, nSupplyNow);
    CFractions frDistortionConsume = frDistortionNegativeConsume + frDistortionPositiveConsume;
    
    frBalance += frDistortionConsume;
    frExchange += frDistortionConsume;
    frDistortion -= frDistortionConsume;
    
    if (frDistortion.Positive(nullptr).Total() != -frDistortion.Negative(nullptr).Total()) {
        throw JSONRPCError(RPC_MISC_ERROR, 
                           strprintf("Mismatch distortion parts (%d - %d)",
                                     frDistortion.Positive(nullptr).Total(), 
                                     frDistortion.Negative(nullptr).Total()));
    }
    int64_t nDistortion = frDistortion.Positive(nullptr).Total();
    int64_t nDistortionLiquid = nDistortion;
    int64_t nDistortionReserve = nDistortion;
    CFractions frDistortionLiquid = frDistortion.HighPart(nSupplyNow, nullptr);
    CFractions frDistortionReserve = frDistortion.LowPart(nSupplyNow, nullptr);
    int64_t nDistortionLiquidNegative = 0;
    int64_t nDistortionLiquidPositive = 0;
    frDistortionLiquid.Negative(&nDistortionLiquidNegative);
    frDistortionLiquid.Positive(&nDistortionLiquidPositive);
    int64_t nDistortionReserveNegative = 0;
    int64_t nDistortionReservePositive = 0;
    frDistortionReserve.Negative(&nDistortionReserveNegative);
    frDistortionReserve.Positive(&nDistortionReservePositive);
    nDistortionLiquid = std::min(nDistortionLiquidPositive, -nDistortionLiquidNegative);
    nDistortionReserve = std::min(nDistortionReservePositive, -nDistortionReserveNegative);
    
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << rawTx;
    
    string txstr = HexStr(ss.begin(), ss.end());
    
    Object result;
    result.push_back(Pair("completed", true));
    result.push_back(Pair("txhash", txhash));
    result.push_back(Pair("rawtx", txstr));

    result.push_back(Pair("consumed_inputs", sConsumedInputs));
    result.push_back(Pair("provided_outputs", sProvidedOutputs));

    result.push_back(Pair("created_on_supply", nSupplyNow));
    result.push_back(Pair("broadcast_on_supply", nSupplyNext));
    
    CDataStream fout_balance(SER_DISK, CLIENT_VERSION);
    frBalance.Pack(fout_balance);
    result.push_back(Pair("balance_value", frBalance.Total()));
    result.push_back(Pair("balance_liquid", frBalance.High(nSupplyNow)));
    result.push_back(Pair("balance_reserve", frBalance.Low(nSupplyNow)));
    result.push_back(Pair("balance_nchange", frBalance.Change(nSupplyNow, nSupplyNext)));
    result.push_back(Pair("balance_pegdata", EncodeBase64(fout_balance.str())));

    CDataStream fout_exchange(SER_DISK, CLIENT_VERSION);
    frExchange.Pack(fout_exchange);
    result.push_back(Pair("exchange_value", frExchange.Total()));
    result.push_back(Pair("exchange_liquid", frExchange.High(nSupplyNow)));
    result.push_back(Pair("exchange_reserve", frExchange.Low(nSupplyNow)));
    result.push_back(Pair("exchange_nchange", frExchange.Change(nSupplyNow, nSupplyNext)));
    result.push_back(Pair("exchange_pegdata", EncodeBase64(fout_exchange.str())));
    
    CDataStream fout_distortion(SER_DISK, CLIENT_VERSION);
    frDistortion.Pack(fout_distortion);
    result.push_back(Pair("distortion_value", nDistortion));
    result.push_back(Pair("distortion_liquid", nDistortionLiquid));
    result.push_back(Pair("distortion_reserve", nDistortionReserve));
    result.push_back(Pair("distortion_pegdata", EncodeBase64(fout_distortion.str())));
    
    return result;
}

#endif
