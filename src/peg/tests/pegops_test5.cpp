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

void TestPegOps::test5()
{
    CFractions user1(0,CFractions::STD);
    for(int i=0;i<PEG_SIZE;i++) {
        user1.f[i] = 23;
    }
    CFractions pegshift(0,CFractions::STD);
    
    CPegLevel level1(1,0,0,0,0);
    
    string user1_b64 = packpegdata(user1, level1);
    string shift_b64 = packpegdata(pegshift, level1);
    
    string exchange1_b64 = user1_b64;
    string pegshift1_b64 = shift_b64;
    
    string peglevel1_hex;
    string pegpool1_b64;
    string out_err;
    
    bool ok1 = getpeglevel(
                exchange1_b64,
                pegshift1_b64,
                1,
                0,
                
                3,
                3,
                3,
                
                peglevel1_hex,
                pegpool1_b64,
                out_err
                );
    
    qDebug() << peglevel1_hex.c_str();
    qDebug() << pegpool1_b64.c_str();
    qDebug() << out_err.c_str();
    
    QVERIFY(ok1 == true);
    QVERIFY(peglevel1_hex == "01000000000000000000000000000000030003000300000000000000000000000000000000000000");
    QVERIFY(pegpool1_b64 == "AgACAAAAAAAAAAAAKwAAAAAAAAB42u3FIQEAAAgDsJOC/k0xyEfYzJJu/7Ft27Zt27Zt27Zt27Zt1w/oDgTEAQAAAAAAAAAAAAAAAAAAAAMAAwADAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAi2sAAAAAAAA=");
    
    // only 1 user, calculate balance:
    
    string pegpool1_r2_b64;
    string user1_r2_b64;
    
    bool ok2 = updatepegbalances(
                user1_b64,
                pegpool1_b64,
                peglevel1_hex,
                
                user1_r2_b64,
                pegpool1_r2_b64,
                out_err
                );
    
    qDebug() << user1_r2_b64.c_str();
    qDebug() << pegpool1_r2_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok2 == true);
    
    // move to cycle2, peg +10
    
    string peglevel2_hex;
    string pegpool2_b64;
    
    bool ok3 = getpeglevel(
                exchange1_b64,
                pegshift1_b64,
                2,
                1,
                13,
                13,
                13,
                
                peglevel2_hex,
                pegpool2_b64,
                out_err
                );
    
    qDebug() << peglevel2_hex.c_str();
    qDebug() << pegpool2_b64.c_str();
    qDebug() << out_err.c_str();
    
    QVERIFY(ok3 == true);
    
    string pegpool1_r3_b64;
    string user1_r3_b64;
    
    bool ok4 = updatepegbalances(
                user1_r2_b64,
                pegpool2_b64,
                peglevel2_hex,
                
                user1_r3_b64,
                pegpool1_r3_b64,
                out_err
                );
    
    qDebug() << user1_r3_b64.c_str();
    qDebug() << pegpool1_r3_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok4 == true);
    // pegpool is zero
    QVERIFY(pegpool1_r3_b64 == "AgACAAAAAAAAAAAAIAAAAAAAAAB42u3BMQEAAADCoPVPbQdvoAAAAAAAAAAAAHgMJYAAAQIAAAAAAAAAAQAAAAAAAAANAA0ADQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    
    // move to cycle3, peg +10
    
    string peglevel3_hex;
    string pegpool3_b64;
    
    bool ok5 = getpeglevel(
                exchange1_b64,
                pegshift1_b64,
                3,
                2,
                23,
                23,
                23,
                
                peglevel3_hex,
                pegpool3_b64,
                out_err
                );
    
    qDebug() << peglevel3_hex.c_str();
    qDebug() << pegpool3_b64.c_str();
    qDebug() << out_err.c_str();
    
    QVERIFY(ok5 == true);
    
    string pegpool3_r1_b64;
    string user1_r4_b64;
    
    bool ok6 = updatepegbalances(
                user1_r3_b64,
                pegpool3_b64,
                peglevel3_hex,
                
                user1_r4_b64,
                pegpool3_r1_b64,
                out_err
                );
    
    qDebug() << user1_r4_b64.c_str();
    qDebug() << pegpool3_r1_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok6 == true);
    
    QVERIFY(pegpool3_r1_b64 == "AgACAAAAAAAAAAAAIAAAAAAAAAB42u3BMQEAAADCoPVPbQdvoAAAAAAAAAAAAHgMJYAAAQMAAAAAAAAAAgAAAAAAAAAXABcAFwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    
    CFractions user2(0,CFractions::STD);
    for(int i=10;i<PEG_SIZE-100;i++) {
        user2.f[i] = 79;
    }
    
    CFractions exchange2 = user1 + user2;
    CPegLevel level3(3,0,0,0,0);
    
    string user2_b64 = packpegdata(user2, level3);
    string exchange2_b64 = packpegdata(exchange2, level3);

    // move to cycle4, peg +10
    
    string peglevel4_hex;
    string pegpool4_b64;
    
    bool ok7 = getpeglevel(
                exchange2_b64,
                pegshift1_b64,
                4,
                3,
                33,
                33,
                33,
                
                peglevel4_hex,
                pegpool4_b64,
                out_err
                );
    
    qDebug() << peglevel4_hex.c_str();
    qDebug() << pegpool4_b64.c_str();
    qDebug() << out_err.c_str();
    
    QVERIFY(ok7 == true);
    
    string pegpool4_r1_b64;
    string user1_r5_b64;
    
    bool ok8 = updatepegbalances(
                user1_r4_b64,
                pegpool4_b64,
                peglevel4_hex,
                
                user1_r5_b64,
                pegpool4_r1_b64,
                out_err
                );
    
    qDebug() << user1_r5_b64.c_str();
    qDebug() << pegpool4_r1_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok8 == true);
    
    string pegpool4_r2_b64;
    string user2_r1_b64;
    
    bool ok9 = updatepegbalances(
                user2_b64,
                pegpool4_r1_b64,
                peglevel4_hex,
                
                user2_r1_b64,
                pegpool4_r2_b64,
                out_err
                );
    
    qDebug() << user2_r1_b64.c_str();
    qDebug() << pegpool4_r2_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok9 == true);
    
    // move to cycle5, peg -5
    
    string peglevel5_hex;
    string pegpool5_b64;
    
    bool ok10 = getpeglevel(
                exchange2_b64,
                pegshift1_b64,
                5,
                4,
                28,
                28,
                28,
                
                peglevel5_hex,
                pegpool5_b64,
                out_err
                );
    
    qDebug() << peglevel5_hex.c_str();
    qDebug() << pegpool5_b64.c_str();
    qDebug() << out_err.c_str();
    
    QVERIFY(ok10 == true);
    
    string pegpool5_r1_b64;
    string user1_r6_b64;
    
    bool ok11 = updatepegbalances(
                user1_r5_b64,
                pegpool5_b64,
                peglevel5_hex,
                
                user1_r6_b64,
                pegpool5_r1_b64,
                out_err
                );
    
    qDebug() << user1_r6_b64.c_str();
    qDebug() << pegpool5_r1_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok11 == true);
    
    string pegpool5_r2_b64;
    string user2_r2_b64;
    
    bool ok12 = updatepegbalances(
                user2_r1_b64,
                pegpool5_r1_b64,
                peglevel5_hex,
                
                user2_r2_b64,
                pegpool5_r2_b64,
                out_err
                );
    
    qDebug() << user2_r2_b64.c_str();
    qDebug() << pegpool5_r2_b64.c_str();
    qDebug() << out_err.c_str();
    QVERIFY(ok12 == true);
    QVERIFY(pegpool5_r2_b64 == "AgACAAAAAAAAAAAAIAAAAAAAAAB42u3BMQEAAADCoPVPbQdvoAAAAAAAAAAAAHgMJYAAAQUAAAAAAAAABAAAAAAAAAAcABwAHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
}
