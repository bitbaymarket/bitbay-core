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

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

#ifdef ENABLE_WALLET
Value registerdeposit(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "registerdeposit \n"
            );

    Object result;
    return result;
}
#endif
