// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcclient.h"
#include <set>

#include "chainparams.h"  // for Params().RPCPort()
#include "rpcprotocol.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"

#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include "json/json_spirit_writer_template.h"

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;

Object CallRPC(const string& strMethod, const Array& params) {
	if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
		throw runtime_error(strprintf(
		    _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
		      "If the file does not exist, create it with owner-readable-only file permissions."),
		    GetConfigFile().string()));

	// Connect to localhost
	bool         fUseSSL = GetBoolArg("-rpcssl", false);
	ioContext    io_context;
	ssl::context context(ssl::context::sslv23);
	context.set_options(ssl::context::no_sslv2);
	asio::ssl::stream<asio::ip::tcp::socket>             sslStream(io_context, context);
	SSLIOStreamDevice<asio::ip::tcp>                     d(sslStream, fUseSSL);
	iostreams::stream<SSLIOStreamDevice<asio::ip::tcp> > stream(d);

	bool fWait = GetBoolArg("-rpcwait", false);  // -rpcwait means try until server has started
	do {
		bool fConnected = d.connect(GetArg("-rpcconnect", "127.0.0.1"),
		                            GetArg("-rpcport", itostr(Params().RPCPort())));
		if (fConnected)
			break;
		if (fWait)
			MilliSleep(1000);
		else
			throw runtime_error("couldn't connect to server");
	} while (fWait);

	// HTTP basic authentication
	string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
	map<string, string> mapRequestHeaders;
	mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

	// Send request
	string strRequest = JSONRPCRequest(strMethod, params, 1);
	string strPost    = HTTPPost(strRequest, mapRequestHeaders);
	stream << strPost << std::flush;

	// Receive HTTP reply status
	int nProto  = 0;
	int nStatus = ReadHTTPStatus(stream, nProto);

	// Receive HTTP reply message headers and body
	map<string, string> mapHeaders;
	string              strReply;
	ReadHTTPMessage(stream, mapHeaders, strReply, nProto);

	if (nStatus == HTTP_UNAUTHORIZED)
		throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
	else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND &&
	         nStatus != HTTP_INTERNAL_SERVER_ERROR)
		throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
	else if (strReply.empty())
		throw runtime_error("no response from server");

	// Parse reply
	Value valReply;
	if (!read_string(strReply, valReply))
		throw runtime_error("couldn't parse reply from server");
	const Object& reply = valReply.get_obj();
	if (reply.empty())
		throw runtime_error("expected reply to have result, error and id properties");

	return reply;
}

class CRPCConvertParam {
public:
	std::string methodName;  // method whose params want conversion
	int         paramIdx;    // 0-based idx of param to convert
};

static const CRPCConvertParam vRPCConvertParams[] = {
    {"stop", 0},
    {"getaddednodeinfo", 0},
    {"sendtoaddress", 1},
    {"sendliquid", 1},
    {"sendreserve", 1},
    {"settxfee", 0},
    {"getreceivedbyaddress", 1},
    {"getreceivedbyaccount", 1},
    {"listreceivedbyaddress", 0},
    {"listreceivedbyaddress", 1},
    {"listreceivedbyaddress", 2},
    {"listreceivedbyaccount", 0},
    {"listreceivedbyaccount", 1},
    {"getbalance", 1},
    {"getblock", 1},
    {"getblockbynumber", 0},
    {"getblockbynumber", 1},
    {"getblockhash", 0},
    {"bridgeautomate", 1},
    {"bridgeautomate", 2},
    {"bridgeautomate", 3},
    {"move", 2},
    {"move", 3},
    {"sendfrom", 2},
    {"sendfrom", 3},
    {"listtransactions", 1},
    {"listtransactions", 2},
    {"listbridgetransactions", 1},
    {"listbridgetransactions", 2},
    {"listaccounts", 0},
    {"walletpassphrase", 1},
    {"walletpassphrase", 2},
    {"getblocktemplate", 0},
    {"listsinceblock", 1},
    {"sendalert", 2},
    {"sendalert", 3},
    {"sendalert", 4},
    {"sendalert", 5},
    {"sendalert", 6},
    {"sendmany", 1},
    {"sendmany", 2},
    {"reservebalance", 0},
    {"reservebalance", 1},
    {"addmultisigaddress", 0},
    {"addmultisigaddress", 1},
    {"listunspent", 0},
    {"listunspent", 1},
    {"listunspent", 2},
    {"listunspent", 3},
    {"listfrozen", 0},
    {"listfrozen", 1},
    {"listfrozen", 2},
    {"listfrozen", 3},
    {"liststaked", 0},
    {"liststaked", 1},
    {"liststaked", 2},
    {"liststaked", 3},
    {"listdeposits", 0},
    {"listdeposits", 1},
    {"listdeposits", 2},
    {"balance", 1},
    {"getrawtransaction", 1},
    {"createrawtransaction", 0},
    {"createrawtransaction", 1},
    {"signrawtransaction", 1},
    {"signrawtransaction", 2},
    {"keypoolrefill", 0},
    {"importprivkey", 2},
    {"importaddress", 2},
    {"checkkernel", 0},
    {"checkkernel", 1},
    {"submitblock", 1},
    {"gettxout", 1},
    {"gettxout", 2},
    {"getliquidityrate", 1},
    {"getpeglevel", 2},
    {"getfractions", 1},
    {"makepeglevel", 0},
    {"makepeglevel", 1},
    {"makepeglevel", 2},
    {"makepeglevel", 3},
    {"makepeglevel", 4},
    {"makepeglevel", 5},
    {"movecoins", 0},
    {"moveliquid", 0},
    {"movereserve", 0},
    {"prepareliquidwithdraw", 3},
    {"preparereservewithdraw", 3},
    {"validaterawtransaction", 1},
    {"validaterawtransaction", 2},
    {"merklesin", 0},
    {"merklesout", 0},
};

static const CRPCConvertParam vRPCMixedParams[] = {
    {"listunspent", 0},
    {"listfrozen", 0},
    {"liststaked", 0},
};

class CRPCConvertTable {
private:
	std::set<std::pair<std::string, int> > mixed;
	std::set<std::pair<std::string, int> > members;

public:
	CRPCConvertTable();

	bool ismixed(const std::string& method, int idx) {
		return (mixed.count(std::make_pair(method, idx)) > 0);
	}
	bool convert(const std::string& method, int idx) {
		return (members.count(std::make_pair(method, idx)) > 0);
	}
};

CRPCConvertTable::CRPCConvertTable() {
	const uint32_t n_elem1 = (sizeof(vRPCMixedParams) / sizeof(vRPCMixedParams[0]));

	for (uint32_t i = 0; i < n_elem1; i++) {
		mixed.insert(std::make_pair(vRPCMixedParams[i].methodName, vRPCMixedParams[i].paramIdx));
	}

	const uint32_t n_elem2 = (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

	for (uint32_t i = 0; i < n_elem2; i++) {
		members.insert(
		    std::make_pair(vRPCConvertParams[i].methodName, vRPCConvertParams[i].paramIdx));
	}
}

static CRPCConvertTable rpcCvtTable;

// Convert strings to command-specific RPC representation
Array RPCConvertValues(const std::string& strMethod, const std::vector<std::string>& strParams) {
	Array params;

	for (uint32_t idx = 0; idx < strParams.size(); idx++) {
		const std::string& strVal = strParams[idx];

		// if type can be mixed
		if (rpcCvtTable.ismixed(strMethod, idx)) {
			Value jVal;
			if (!read_string(strVal, jVal)) {
				params.push_back(strVal);
			} else {
				params.push_back(jVal);
			}
			continue;
		}

		// insert string value directly
		if (!rpcCvtTable.convert(strMethod, idx)) {
			params.push_back(strVal);
		}

		// parse string as JSON, insert bool/number/object/etc. value
		else {
			Value jVal;
			if (!read_string(strVal, jVal))
				throw runtime_error(string("Error parsing JSON:") + strVal);
			params.push_back(jVal);
		}
	}

	return params;
}

int CommandLineRPC(int argc, char* argv[]) {
	string strPrint;
	int    nRet = 0;
	try {
		// Skip switches
		while (argc > 1 && IsSwitchChar(argv[1][0])) {
			argc--;
			argv++;
		}

		// Method
		if (argc < 2)
			throw runtime_error("too few parameters");
		string strMethod = argv[1];

		// Parameters default to strings
		std::vector<std::string> strParams(&argv[2], &argv[argc]);
		Array                    params = RPCConvertValues(strMethod, strParams);

		// Execute
		Object reply = CallRPC(strMethod, params);

		// Parse reply
		const Value& result = find_value(reply, "result");
		const Value& error  = find_value(reply, "error");

		if (error.type() != null_type) {
			// Error
			strPrint = "error: " + write_string(error, false);
			int code = find_value(error.get_obj(), "code").get_int();
			nRet     = abs(code);
		} else {
			// Result
			if (result.type() == null_type)
				strPrint = "";
			else if (result.type() == str_type)
				strPrint = result.get_str();
			else
				strPrint = write_string(result, true);
		}
	} catch (boost::thread_interrupted) {
		throw;
	} catch (std::exception& e) {
		strPrint = string("error: ") + e.what();
		nRet     = 87;
	} catch (...) {
		PrintException(NULL, "CommandLineRPC()");
	}

	if (strPrint != "") {
		fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
	}
	return nRet;
}
