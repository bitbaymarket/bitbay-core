
macx {

# mac: default path to brew packages
INCLUDEPATH += /usr/local/opt/boost/include
INCLUDEPATH += /usr/local/opt/openssl/include
INCLUDEPATH += /usr/local/opt/miniupnpc/include
INCLUDEPATH += /usr/local/opt/qrencode/include

LIBS += -L/usr/local/opt/boost/lib
LIBS += -L/usr/local/opt/openssl/lib
LIBS += -L/usr/local/opt/miniupnpc/lib
LIBS += -L/usr/local/opt/qrencode/lib

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9

contains(RELEASE, 1) {
    # Mac: compile for maximum compatibility (10.5, 32-bit)
    macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk
}

}
