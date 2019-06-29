
TEMPLATE = app
QT += testlib

CONFIG += console
CONFIG -= app_bundle
CONFIG += no_include_pwd
CONFIG += thread
CONFIG += c++11

INCLUDEPATH += $$PWD/..

include($$PWD/pegops.pri)

SOURCES += $$PWD/pegops_tests.cpp

LIBS += -lz
LIBS += -lboost_system
LIBS += -lssl -lcrypto 
