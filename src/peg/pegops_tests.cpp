// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QtTest/QtTest>

#include "pegops.h"

#include <string>

using namespace std;
using namespace pegops;

class TestPegOps: public QObject {
    Q_OBJECT
private slots:
    void test1();
    void test2();
};

void TestPegOps::test1()
{
    string inp_peglevel_hex = "202d0000000000001f2d0000000000000d0010001300000008adf56d0000000095c564c1890a0000";
    
    int out_cycle_now;
    int out_cycle_prev;
    int out_peg_now;
    int out_peg_next;
    int out_peg_next_next;
    int out_shift;
    int64_t out_shiftlastpart;
    int64_t out_shiftlasttotal;
    
    bool ok = getpeglevelinfo(
                inp_peglevel_hex,
                    
                out_cycle_now,    
                out_cycle_prev,   
                out_peg_now,      
                out_peg_next,     
                out_peg_next_next,
                out_shift,        
                out_shiftlastpart,
                out_shiftlasttotal);
    
    QVERIFY(ok                  == true);
    QVERIFY(out_cycle_now       == 11552);
    QVERIFY(out_cycle_prev      == 11551);
    QVERIFY(out_peg_now         == 13);
    QVERIFY(out_peg_next        == 16);
    QVERIFY(out_peg_next_next   == 19);
    QVERIFY(out_shift           == 0);
    QVERIFY(out_shiftlastpart   == 1844817160);
    QVERIFY(out_shiftlasttotal  == 11586771404181);
    
    qDebug() << out_cycle_now
        << out_cycle_prev
        << out_peg_now     
        << out_peg_next      
        << out_peg_next_next
        << out_shift
        << out_shiftlastpart
        << out_shiftlasttotal;
}

void TestPegOps::test2()
{
    string inp_peglevel_hex = "202d000snhgajhg0000d0010001304c1890a0000";
    
    int out_cycle_now;
    int out_cycle_prev;
    int out_peg_now;
    int out_peg_next;
    int out_peg_next_next;
    int out_shift;
    int64_t out_shiftlastpart;
    int64_t out_shiftlasttotal;
    
    bool ok = getpeglevelinfo(
                inp_peglevel_hex,
                    
                out_cycle_now,    
                out_cycle_prev,   
                out_peg_now,      
                out_peg_next,     
                out_peg_next_next,
                out_shift,        
                out_shiftlastpart,
                out_shiftlasttotal);
    
    QVERIFY(ok == false);
}

QTEST_MAIN(TestPegOps)
#include "pegops_tests.moc"
