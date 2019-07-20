// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QtTest/QtTest>

#include "pegops.h"
#include "pegdata.h"
#include "pegops_tests.h"

#include <string>
#include <vector>

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
bool unpackbalance(const string &   pegdata64,
                   string           tag,
                   CFractions &     fractions,
                   CPegLevel &      peglevel,
                   int64_t &        nReserve,
                   int64_t &        nLiquid,
                   string &         err);

}

void TestPegOps::test1k()
{
    //return;
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0,1000);
    
    CPegLevel level1(1,0,500,500,500);
    qDebug() << level1.nSupply << level1.nShift << level1.nShiftLastPart << level1.nShiftLastTotal;
    
    vector<CFractions> users;
    vector<string> user_balances;
    // init users
    for(int i=0; i< 1000; i++) {
        CFractions user(0,CFractions::STD);
        int start = distribution(generator);
        for(int i=start;i<PEG_SIZE;i++) {
            user.f[i] = distribution(generator) / (i*5/6+1);
        }
        users.push_back(user);
        string user_b64 = packpegdata(user, level1);
        user_balances.push_back(user_b64);
    }
    
    // calc exchange
    CFractions exchange(0,CFractions::STD);
    for(int i=0; i< 1000; i++) {
        CFractions & user = users[i];
        exchange += user;
    }
    
    CFractions pegshift(0,CFractions::STD);
    for(int i=0;i<PEG_SIZE;i++) {
        if (i<PEG_SIZE/2) {
            pegshift.f[i] = distribution(generator) /10 / (i+1);
        } else {
            pegshift.f[i] = -pegshift.f[PEG_SIZE-i-1];
        }
    }
    
    QCOMPARE(pegshift.Total(), 0);
    
    for(int i=0; i<1000; i++) {
        string exchange1_b64 = packpegdata(exchange, level1);
        string pegshift1_b64 = packpegdata(pegshift, level1);
        
        string peglevel_hex;
        string pegpool_b64;
        string out_err;
        
        int peg = 0;
        int buffer = 3;
        if (i<500) {
            peg = 500+i;
        } else {
            peg = 500+500 - (i-500)*2;
        }
        
        bool ok1 = getpeglevel(
                    exchange1_b64,
                    pegshift1_b64,
                    i+2,
                    i+1,
                    peg+buffer,
                    peg+buffer,
                    peg+buffer,
                    
                    peglevel_hex,
                    pegpool_b64,
                    out_err
                    );
        
        //qDebug() << peglevel_hex.c_str();
        //qDebug() << pegpool_b64.c_str();
        //qDebug() << out_err.c_str();
        qDebug() << i;
        
        QVERIFY(ok1 == true);
    
        // update balances
        for(int j=0; j< 1000; j++) {
            string & user_balance = user_balances[j];
            
            string pegpool_out_b64;
            string user_balance_out_b64;
            
            bool ok8 = updatepegbalances(
                        user_balance,
                        pegpool_b64,
                        peglevel_hex,
                        
                        user_balance_out_b64,
                        pegpool_out_b64,
                        out_err
                        );
            
            if (!ok8) {
                qDebug() << user_balance_out_b64.c_str();
                qDebug() << pegpool_out_b64.c_str();
                qDebug() << out_err.c_str();
            }
            QVERIFY(ok8 == true);
            
            pegpool_b64 = pegpool_out_b64;
            user_balance = user_balance_out_b64;
        }
        
        // pool should be empty
        CPegLevel peglevel_skip("");
        CFractions pegpool(0, CFractions::STD);
        bool ok99 = unpackbalance(pegpool,
                                  peglevel_skip, 
                                  pegpool_b64, 
                                  "pegppol", out_err);
        
        QVERIFY(ok99 == true);
        if (pegpool.Total() != 0) {
            qDebug() << pegpool_b64.c_str();
        }
        QVERIFY(pegpool.Total() == 0);
        
        // some trades
        for(int j=0; j<1000; j++) {
            
            int src_idx = std::min(distribution(generator), 999);
            int dst_idx = std::min(distribution(generator), 999);
            
            if (src_idx == dst_idx) continue;
            
            string & user_src = user_balances[src_idx];
            string & user_dst = user_balances[dst_idx];
            
            string src_out_b64;
            string dst_out_b64;
            
            int64_t src_liquid = 0;
            int64_t src_reserve = 0;
            CFractions src(0, CFractions::STD);
            bool ok10 = unpackbalance(user_src, 
                                      "src", 
                                      src,
                                      peglevel_skip, 
                                      src_reserve,
                                      src_liquid,
                                      out_err);
            QVERIFY(ok10 == true);
            
            string user_src_copy = user_src;
            string user_dst_copy = user_dst;
            
            if (src_idx % 2 == 0) {
                
                int64_t amount = (src_liquid * (distribution(generator)/100+1)) /10;
                amount = std::min(amount, src_liquid);
                bool ok11 = moveliquid(
                            amount,
                            user_src,
                            user_dst,
                            peglevel_hex,
                            
                            src_out_b64,
                            dst_out_b64,
                            out_err);
                
                if (!ok11) {
                    qDebug() << "problem trade:";
                    qDebug() << out_err.c_str();
                    qDebug() << amount;
                    qDebug() << user_src_copy.c_str();
                    qDebug() << user_dst_copy.c_str();
                    qDebug() << src_out_b64.c_str();
                    qDebug() << dst_out_b64.c_str();
                    qDebug() << peglevel_hex.c_str(); 
                }
                QVERIFY(ok11 == true);
                
                user_src = src_out_b64;
                user_dst = dst_out_b64;
                
                // extra checks compare all
                // calc exchange
                /*
                CFractions exchange1(0,CFractions::STD);
                for(int k=0; k< 1000; k++) {
                    //CFractions & user = users[i];
                    CPegLevel user_skip("");
                    CFractions user(0, CFractions::STD);
                    bool ok = unpackbalance(user,
                                              user_skip, 
                                              user_balances[k], 
                                              "user", out_err);
                    if (!ok) {
                        qDebug() << out_err.c_str();
                    }
                    QVERIFY(ok == true);
                    
                    exchange1 += user;
                }
                
                for(int k=0;k<PEG_SIZE; k++) {
                    if (exchange.f[k] != exchange1.f[k]) {
                        qDebug() << "problem trade:";
                        qDebug() << amount;
                        qDebug() << user_src_copy.c_str();
                        qDebug() << user_dst_copy.c_str();
                        qDebug() << src_out_b64.c_str();
                        qDebug() << dst_out_b64.c_str();
                        qDebug() << peglevel_hex.c_str();
                        qDebug() << "exchange orig";
                        qDebug() << packpegdata(exchange, level1).c_str();
                        qDebug() << "exchange new";
                        qDebug() << packpegdata(exchange1, level1).c_str();
                        qDebug() << "exchange diff";
                        CFractions diff = exchange1 - exchange;
                        qDebug() << packpegdata(diff, level1).c_str();
                    }
                    QVERIFY(exchange.f[k] == exchange1.f[k]);
                }
                */
            }
            else {
                int64_t amount = (src_reserve * (distribution(generator)/100+1)) /10;
                amount = std::min(amount, src_reserve);
                bool ok12 = movereserve(
                            amount,
                            user_src,
                            user_dst,
                            peglevel_hex,
                            
                            src_out_b64,
                            dst_out_b64,
                            out_err);
                
                if (!ok12) {
                    qDebug() << "problem trade:";
                    qDebug() << out_err.c_str();
                    qDebug() << amount;
                    qDebug() << user_src_copy.c_str();
                    qDebug() << user_dst_copy.c_str();
                    qDebug() << src_out_b64.c_str();
                    qDebug() << dst_out_b64.c_str();
                    qDebug() << peglevel_hex.c_str(); 
                }
                QVERIFY(ok12 == true);
                
                user_src = src_out_b64;
                user_dst = dst_out_b64;
            }
        }
    
        // extra checks compare all
        // calc exchange
        
        CFractions exchange1(0,CFractions::STD);
        for(int k=0; k< 1000; k++) {
            CPegLevel user_skip("");
            CFractions user(0, CFractions::STD);
            bool ok = unpackbalance(user,
                                      user_skip, 
                                      user_balances[k], 
                                      "user", out_err);
            if (!ok) {
                qDebug() << out_err.c_str();
            }
            QVERIFY(ok == true);
            
            exchange1 += user;
        }
        
        for(int k=0;k<PEG_SIZE; k++) {
            if (exchange.f[k] != exchange1.f[k]) {
                qDebug() << "exchange orig";
                qDebug() << packpegdata(exchange, level1).c_str();
                qDebug() << "exchange new";
                qDebug() << packpegdata(exchange1, level1).c_str();
                qDebug() << "exchange diff";
                CFractions diff = exchange1 - exchange;
                qDebug() << packpegdata(diff, level1).c_str();
            }
            QVERIFY(exchange.f[k] == exchange1.f[k]);
        }

    }

}
