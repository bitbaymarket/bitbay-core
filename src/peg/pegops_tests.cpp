// Copyright (c) 2018 yshurik
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QtTest/QtTest>

class TestPegOps: public QObject {
    Q_OBJECT
private slots:
    void test1();
};

void TestPegOps::test1()
{
    QString str = "Hello";
    QCOMPARE(str.toUpper(), QString("HELLO"));
}

QTEST_MAIN(TestPegOps)
#include "pegops_tests.moc"
