[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4555f42045b34caf8c22261d3fe3293f)](https://app.codacy.com/gh/bitbaymarket/bitbay-core/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Open Source Love](https://badges.frapsoft.com/os/mit/mit.svg?v=102)](https://github.com/bitbaymarket/bitbay-core/blob/master/COPYING)


BitBay
======

The world's first decentralised currency designed for mass adoption
With its unique system of adaptive supply control, BitBay is creating a reliable currency that is truly independent.
The revolutionary 'Dynamic Peg' creates both a store of value and a medium of exchange.

License
-------

BitBay is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Compilation
-----------

**Local**

Compiling *bitbayd* on unix host:

```
autoreconf --install --force
./configure
make
```

**Using docker images of builders**

1. Building Linux64 static binary of BitBay Qt Wallet:
Ref builder image repo: [builder-linux64](https://https://github.com/bitbaymarket/builder-linux64)
```sh
#!/bin/sh
set -x
set -e
rm -rf .qmake.stash
rm -rf build bitbay-wallet-qt
cd src
git clean -f -d -x .
cd ..
docker pull bitbayofficial/builder-linux64:alpine
rm -rf bitbay-qt-local.pri
echo "CONFIG += silent" >> bitbay-qt-local.pri
echo "LIBS += -static" > bitbay-qt-local.pri
echo "DEFINES += CURL_STATICLIB" >> bitbay-qt-local.pri
echo "DEFINES += SECP256K1_STATIC" >> bitbay-qt-local.pri
echo "LIBS_CURL_DEPS += -lssh2 -lnghttp2" >> bitbay-qt-local.pri
mkdir -p build
/bin/sh share/genbuild.sh build/build.h
docker run --rm \
	-v $(pwd):/mnt \
	-u $(stat -c %u:%g .) \
	bitbayofficial/builder-linux64:alpine \
	/bin/sh -c "qmake-qt5 -v && \
		cd /mnt && \
		ls -al && \
		qmake-qt5 \
		CICD=travis_x64 \
		\"USE_TESTNET=0\" \
		\"USE_DBUS=0\" \
		\"USE_QRCODE=0\" \
		\"BOOST_LIB_SUFFIX=-mt\" \
		bitbay-qt.pro && \
		sed -i 's/\/usr\/lib\/libssl.so/-lssl/' Makefile &&
		sed -i 's/\/usr\/lib\/libcrypto.so/-lcrypto/' Makefile &&
		sed -i s:sys/fcntl.h:fcntl.h: src/compat.h &&
		make -j32";
tar -zcf mainnet-qt-wallet-lin64.tgz  bitbay-wallet-qt
```

2. Building Linux64 static binary of bitbayd (without a wallet):
Ref builder image repo: [builder-linux64](https://https://github.com/bitbaymarket/builder-linux64)
```sh
#!/bin/sh
set -e
set -x
rm -rf .qmake.stash
rm -rf build bitbayd
cd src
git clean -f -d -x .
cd ..
docker pull bitbayofficial/builder-linux64:alpine
rm -rf bitbayd-local.pri
echo "CONFIG += silent" >> bitbayd-local.pri
echo "LIBS += -static" > bitbayd-local.pri
echo "DEFINES += CURL_STATICLIB" >> bitbayd-local.pri
echo "DEFINES += SECP256K1_STATIC" >> bitbayd-local.pri
echo "LIBS_CURL_DEPS += -lssh2 -lnghttp2" >> bitbayd-local.pri
mkdir -p build
/bin/sh share/genbuild.sh build/build.h
docker run --rm \
	-v $(pwd):/mnt \
	-u $(stat -c %u:%g .) \
	bitbayofficial/builder-linux64:alpine \
	/bin/sh -c "qmake-qt5 -v && \
		cd /mnt && \
		ls -al && \
		qmake-qt5 \
		CICD=travis_x64 \
		\"USE_DBUS=0\" \
		\"USE_QRCODE=0\" \
		\"USE_WALLET=0\" \
		\"USE_TESTNET=0\" \
		\"BOOST_LIB_SUFFIX=-mt\" \
		bitbayd.pro && \
		sed -i 's/\/usr\/lib\/libssl.so/-lssl/' Makefile &&
		sed -i 's/\/usr\/lib\/libcrypto.so/-lcrypto/' Makefile &&
		sed -i s:sys/fcntl.h:fcntl.h: src/compat.h &&
		make -j32"
```

3. Building Windows64 static binary of BitBay Qt Wallet:
Ref builder image repo: [builder-windows64](https://https://github.com/bitbaymarket/builder-windows64)
```sh
#!/bin/sh
set -x
set -e
rm -rf .qmake.stash
rm -rf build bitbay-wallet-qt.exe
cd src
git clean -f -d -x .
cd ..
docker pull bitbayofficial/builder-windows64:qt
rm -rf bitbay-qt-local.pri
echo "CONFIG += silent" >> bitbay-qt-local.pri
echo "DEFINES += CURL_STATICLIB" >> bitbay-qt-local.pri
echo "DEFINES += SECP256K1_STATIC" >> bitbay-qt-local.pri
echo "LIBS_CURL_DEPS += -lidn2 -lunistring -liconv -lcharset -lssh2 -lssh2 -lz -lgcrypt -lgpg-error -lintl -liconv -lws2_32 -lnettle -lgnutls -lhogweed -lnettle -lidn2 -lz -lws2_32 -lcrypt32 -lgmp -lunistring -liconv -lcharset -lwldap32 -lz -lws2_32 -lpthread" >> bitbay-qt-local.pri
mkdir -p build
/bin/sh share/genbuild.sh build/build.h
docker run --rm \
	-v $(pwd):/mnt \
	-u $(stat -c %u:%g .) \
	bitbayofficial/builder-windows64:qt \
	/bin/bash -c "cd /mnt && qmake -v && \
		qmake \
		CICD=travis_x64 \
		QMAKE_LRELEASE=lrelease \
		\"USE_TESTNET=0\" \
		\"USE_QRCODE=1\" \
		bitbay-qt.pro && \
		mv Makefile.Release Makefile.tmp && ( \
		cat Makefile.tmp | \
		sed -e 's/bin.lrelease\.exe/bin\/lrelease/m' | \
		sed -e 's/boost_thread-mt/boost_thread_win32-mt/m' > Makefile.Release \
		) && \
		make -j32";
mv release/bitbay-wallet-qt.exe .           &&
file bitbay-wallet-qt.exe                   &&
zip mainnet-qt-wallet-win64.zip bitbay-wallet-qt.exe;
```

