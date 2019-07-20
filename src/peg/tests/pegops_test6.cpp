// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QtTest/QtTest>

#include "pegops.h"
#include "pegdata.h"
#include "pegops_tests.h"

#include <string>

using namespace std;
using namespace pegops;

namespace pegops {
string packpegdata(const CFractions & fractions,
                   const CPegLevel & peglevel);
bool unpackbalance(CFractions & fractions,
                   CPegLevel & peglevel,
                   const string & pegdata64,
                   string tag,
                   string & err);
}

void TestPegOps::test6()
{
    CFractions user1(0,CFractions::STD);
    for(int i=0;i<PEG_SIZE;i++) {
        user1.f[i] = 23;
    }
    
    CFractions user2(0,CFractions::STD);
    for(int i=100;i<PEG_SIZE-100;i++) {
        user2.f[i] = 79;
    }
    
    CFractions exchange1 = user1 + user2;
    
    CFractions pegshift1(0,CFractions::STD);
    for(int i=0;i<PEG_SIZE;i++) {
        if (i<PEG_SIZE/2) {
            pegshift1.f[i] = 13;
        } else {
            pegshift1.f[i] = -13;
        }
    }
    
    QCOMPARE(pegshift1.Total(), 0);
    
    CPegLevel level1(1,0,200,200,200,
                     exchange1, pegshift1);
    
    qDebug() << level1.nSupply << level1.nShift << level1.nShiftLastPart << level1.nShiftLastTotal;

    string user1_r1_b64 = packpegdata(user1, level1);
    string user2_r1_b64 = packpegdata(user2, level1);
    string exchange1_b64 = packpegdata(exchange1, level1);
    string pegshift1_b64 = packpegdata(pegshift1, level1);
    
    CPegLevel level2(2,1,205,205,205,
                     exchange1, pegshift1);
    
    string peglevel2_hex;
    string pegpool2_b64;
    string out_err;
    
    int buffer = 3;
    
    bool ok1 = getpeglevel(
                exchange1_b64,
                pegshift1_b64,
                2,
                1,
                205+buffer,
                205+buffer,
                205+buffer,
                
                peglevel2_hex,
                pegpool2_b64,
                out_err
                );
    
    qDebug() << peglevel2_hex.c_str();
    qDebug() << pegpool2_b64.c_str();
    qDebug() << out_err.c_str();
    
    QVERIFY(ok1 == true);
    
    // update balances
    
    string pegpool2_r1_b64;
    string user1_r2_b64;
    
    bool ok8 = updatepegbalances(
                user1_r1_b64,
                pegpool2_b64,
                peglevel2_hex,
                
                user1_r2_b64,
                pegpool2_r1_b64,
                out_err
                );
    
    qDebug() << user1_r2_b64.c_str();
    qDebug() << pegpool2_r1_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok8 == true);
    
    string pegpool2_r2_b64;
    string user2_r2_b64;
    
    bool ok9 = updatepegbalances(
                user2_r1_b64,
                pegpool2_r1_b64,
                peglevel2_hex,
                
                user2_r2_b64,
                pegpool2_r2_b64,
                out_err
                );
    
    qDebug() << user2_r2_b64.c_str();
    qDebug() << pegpool2_r2_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok9 == true);
}
