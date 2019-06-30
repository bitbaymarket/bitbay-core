
TEMPLATE = app
QT += testlib

CONFIG += console
CONFIG -= app_bundle
CONFIG += no_include_pwd
CONFIG += thread
CONFIG += c++11

INCLUDEPATH += $$PWD/..

include($$PWD/pegops.pri)

HEADERS += $$PWD/pegops_tests.h
SOURCES += $$PWD/pegops_tests.cpp
SOURCES += $$PWD/pegops_test5.cpp
SOURCES += $$PWD/pegops_test6.cpp
SOURCES += $$PWD/pegops_test7.cpp
SOURCES += $$PWD/pegops_test1k.cpp

LIBS += -lz
LIBS += -lboost_system
LIBS += -lssl -lcrypto 
