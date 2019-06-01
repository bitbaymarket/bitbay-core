
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
    $$PWD/serialize.h \
    $$PWD/core.h \
    $$PWD/main.h \
    $$PWD/net.h \
    $$PWD/key.h \
    $$PWD/txdb.h \
    $$PWD/txmempool.h \
    $$PWD/script.h \
    $$PWD/init.h \
    $$PWD/mruset.h \
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
    $$PWD/init.cpp \
    $$PWD/net.cpp \
    $$PWD/checkpoints.cpp \
    $$PWD/addrman.cpp \
    $$PWD/keystore.cpp \
    $$PWD/rpcclient.cpp \
    $$PWD/rpcprotocol.cpp \
    $$PWD/rpcserver.cpp \
    $$PWD/rpcmisc.cpp \
    $$PWD/rpcnet.cpp \
    $$PWD/rpcblockchain.cpp \
    $$PWD/rpcrawtransaction.cpp \
    $$PWD/timedata.cpp \
    $$PWD/crypter.cpp \
    $$PWD/protocol.cpp \
    $$PWD/noui.cpp \
    $$PWD/kernel.cpp \

HEADERS += \
    $$PWD/crypto/pbkdf2.h \
    $$PWD/crypto/scrypt.h \

SOURCES += \
    $$PWD/crypto/pbkdf2.cpp \
    $$PWD/crypto/scrypt.cpp \

wallet {

	DEFINES += ENABLE_WALLET

	HEADERS += \
		$$PWD/db.h \
		$$PWD/miner.h \
		$$PWD/wallet.h \
		$$PWD/walletdb.h \

	SOURCES += \
		$$PWD/db.cpp \
		$$PWD/miner.cpp \
		$$PWD/rpcdump.cpp \
		$$PWD/rpcmining.cpp \
		$$PWD/rpcwallet.cpp \
		$$PWD/rpcexchange.cpp \
		$$PWD/wallet.cpp \
		$$PWD/walletdb.cpp \

}

# peg system
HEADERS += src/peg.h
SOURCES += src/peg.cpp
SOURCES += src/peg_compat.cpp
HEADERS += src/pegdb-leveldb.h
SOURCES += src/pegdb-leveldb.cpp
LIBS += -lz
