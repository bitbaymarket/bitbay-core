
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/alert.h \
    $$PWD/addrman.h \
    $$PWD/base58.h \
    $$PWD/bignum.h \
    $$PWD/chainparams.h \
    $$PWD/chainparamsseeds.h \
    $$PWD/checkpoints.h \
    $$PWD/compat.h \
    $$PWD/coincontrol.h \
    $$PWD/sync.h \
    $$PWD/util.h \
    $$PWD/hash.h \
    $$PWD/uint256.h \
    $$PWD/kernel.h \
    $$PWD/scrypt.h \
    $$PWD/pbkdf2.h \
    $$PWD/serialize.h \
    $$PWD/core.h \
    $$PWD/main.h \
    $$PWD/miner.h \
    $$PWD/net.h \
    $$PWD/key.h \
    $$PWD/db.h \
    $$PWD/txdb.h \
    $$PWD/txmempool.h \
    $$PWD/walletdb.h \
    $$PWD/script.h \
    $$PWD/init.h \
    $$PWD/mruset.h \
    $$PWD/wallet.h \
    $$PWD/keystore.h \
    $$PWD/rpcclient.h \
    $$PWD/rpcprotocol.h \
    $$PWD/rpcserver.h \
    $$PWD/timedata.h \
    $$PWD/crypter.h \
    $$PWD/protocol.h \
    $$PWD/allocators.h \
    $$PWD/ui_interface.h \
    $$PWD/version.h \
    $$PWD/netbase.h \
    $$PWD/clientversion.h \
    $$PWD/threadsafety.h \
    $$PWD/tinyformat.h

SOURCES += \
    $$PWD/alert.cpp \
    $$PWD/chainparams.cpp \
    $$PWD/version.cpp \
    $$PWD/sync.cpp \
    $$PWD/txmempool.cpp \
    $$PWD/util.cpp \
    $$PWD/hash.cpp \
    $$PWD/netbase.cpp \
    $$PWD/key.cpp \
    $$PWD/script.cpp \
    $$PWD/core.cpp \
    $$PWD/main.cpp \
    $$PWD/miner.cpp \
    $$PWD/init.cpp \
    $$PWD/net.cpp \
    $$PWD/checkpoints.cpp \
    $$PWD/addrman.cpp \
    $$PWD/db.cpp \
    $$PWD/walletdb.cpp \
    $$PWD/wallet.cpp \
    $$PWD/keystore.cpp \
    $$PWD/rpcclient.cpp \
    $$PWD/rpcprotocol.cpp \
    $$PWD/rpcserver.cpp \
    $$PWD/rpcdump.cpp \
    $$PWD/rpcmisc.cpp \
    $$PWD/rpcnet.cpp \
    $$PWD/rpcmining.cpp \
    $$PWD/rpcwallet.cpp \
    $$PWD/rpcblockchain.cpp \
    $$PWD/rpcrawtransaction.cpp \
    $$PWD/rpcexchange.cpp \
    $$PWD/timedata.cpp \
    $$PWD/crypter.cpp \
    $$PWD/protocol.cpp \
    $$PWD/noui.cpp \
    $$PWD/kernel.cpp \
    $$PWD/scrypt-arm.S \
    $$PWD/scrypt-x86.S \
    $$PWD/scrypt-x86_64.S \
    $$PWD/scrypt.cpp \
    $$PWD/pbkdf2.cpp

# peg system
HEADERS += src/peg.h
SOURCES += src/peg.cpp
HEADERS += src/pegdb-leveldb.h
SOURCES += src/pegdb-leveldb.cpp
LIBS += -lz
