// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

#include <leveldb/env.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#include <memenv/memenv.h>

#include "kernel.h"
#include "pegdb-leveldb.h"
#include "util.h"
#include "main.h"
#include "chainparams.h"
#include "base58.h"

using namespace std;
using namespace boost;

leveldb::DB *pegdb; // global pointer for LevelDB object instance

static leveldb::Options GetOptions() {
    leveldb::Options options;
    int nCacheSizeMB = GetArg("-dbcache", 25);
    options.block_cache = leveldb::NewLRUCache(nCacheSizeMB * 1048576);
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    return options;
}

static void init_blockindex(leveldb::Options& options, bool fRemoveOld = false, bool fCreateBootstrap = false) {
    // First time init.
    filesystem::path directory = GetDataDir() / "pegleveldb";
    filesystem::create_directory(directory);
    LogPrintf("Opening LevelDB in %s\n", directory.string());
    leveldb::Status status = leveldb::DB::Open(options, directory.string(), &pegdb);
    if (!status.ok()) {
        throw runtime_error(strprintf("init_blockindex(): error opening database environment %s", status.ToString()));
    }
}

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CPegDB::CPegDB(const char* pszMode)
{
    assert(pszMode);
    activeBatch = NULL;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (pegdb) {
        pdb = pegdb;
        return;
    }

    bool fCreate = strchr(pszMode, 'c');

    options = GetOptions();
    options.create_if_missing = fCreate;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);

    init_blockindex(options); // Init directory
    pdb = pegdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        LogPrintf("Peg index version is %d\n", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            LogPrintf("Required index version is %d, removing old database\n", DATABASE_VERSION);

            // Leveldb instance destruction
            delete pegdb;
            pegdb = pdb = NULL;
            delete activeBatch;
            activeBatch = NULL;

            init_blockindex(options, true, true); // Remove directory and create new database
            pdb = pegdb;

            bool fTmp = fReadOnly;
            fReadOnly = false;
            WriteVersion(DATABASE_VERSION); // Save transaction index version
            fReadOnly = fTmp;
        }
    }
    else if (fCreate)
    {
        bool fTmp = fReadOnly;
        fReadOnly = false;
        WriteVersion(DATABASE_VERSION);
        fReadOnly = fTmp;
    }

    LogPrintf("Opened Peg LevelDB successfully\n");
}

void CPegDB::Close()
{
    delete pegdb;
    pegdb = pdb = NULL;
    delete options.filter_policy;
    options.filter_policy = NULL;
    delete options.block_cache;
    options.block_cache = NULL;
    delete activeBatch;
    activeBatch = NULL;
}

bool CPegDB::TxnBegin()
{
    assert(!activeBatch);
    activeBatch = new leveldb::WriteBatch();
    return true;
}

bool CPegDB::TxnCommit()
{
    assert(activeBatch);
    leveldb::Status status = pdb->Write(leveldb::WriteOptions(), activeBatch);
    delete activeBatch;
    activeBatch = NULL;
    if (!status.ok()) {
        LogPrintf("LevelDB batch commit failure: %s\n", status.ToString());
        return false;
    }
    return true;
}

class CPegBatchScanner : public leveldb::WriteBatch::Handler {
public:
    std::string needle;
    bool *deleted;
    std::string *foundValue;
    bool foundEntry;

    CPegBatchScanner() : foundEntry(false) {}

    virtual void Put(const leveldb::Slice& key, const leveldb::Slice& value) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = false;
            *foundValue = value.ToString();
        }
    }

    virtual void Delete(const leveldb::Slice& key) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = true;
        }
    }
};

// When performing a read, if we have an active batch we need to check it first
// before reading from the database, as the rest of the code assumes that once
// a database transaction begins reads are consistent with it. It would be good
// to change that assumption in future and avoid the performance hit, though in
// practice it does not appear to be large.
bool CPegDB::ScanBatch(const CDataStream &key, string *value, bool *deleted) const {
    assert(activeBatch);
    *deleted = false;
    CPegBatchScanner scanner;
    scanner.needle = key.str();
    scanner.deleted = deleted;
    scanner.foundValue = value;
    leveldb::Status status = activeBatch->Iterate(&scanner);
    if (!status.ok()) {
        throw runtime_error(status.ToString());
    }
    return scanner.foundEntry;
}

bool CPegDB::Read(uint320 txout, CFractions & f) {
    std::string strValue;
    if (!ReadStr(txout, strValue)) {
        // For now returns true indicating this output is not in pegdb
        // and supposed to be before the peg started, otherwise may
        // need to know height of transction to compare with peg start
        // to indicate pegdb fault.
        return true;
        //return false;
    }
    CDataStream finp(strValue.data(), strValue.data() + strValue.size(),
                     SER_DISK, CLIENT_VERSION);
    return f.Unpack(finp);
}
bool CPegDB::Write(uint320 txout, const CFractions & f) {
    CDataStream fout(SER_DISK, CLIENT_VERSION);
    f.Pack(fout);
    return Write(txout, fout);
}

bool CPegDB::ReadPegStartHeight(int& nHeight)
{
    return Read(string("pegStartHeight"), nHeight);
}

bool CPegDB::WritePegStartHeight(int nHeight)
{
    return Write(string("pegStartHeight"), nHeight);
}
