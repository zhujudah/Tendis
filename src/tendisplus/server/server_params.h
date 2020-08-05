#ifndef SRC_TENDISPLUS_SERVER_SERVER_PARAMS_H_
#define SRC_TENDISPLUS_SERVER_SERVER_PARAMS_H_

#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <assert.h>
#include <stdlib.h>
#include <functional>
#include <atomic>
#include <list>
#include "glog/logging.h"
#include "tendisplus/server/session.h"
#include "tendisplus/utils/status.h"
#include "tendisplus/utils/string.h"
#include "tendisplus/utils/redis_port.h"

namespace tendisplus {
using namespace std;

using funptr = std::function<void ()>;
using checkfunptr = std::function<bool (const string &)>;
using preProcess = std::function<string (const string &)>;

string removeQuotes(const string& v);
string removeQuotesAndToLower(const string& v);

class BaseVar {
public:
    BaseVar(const string& s, void* v, checkfunptr ptr, preProcess preFun, bool allowDS)
        : name(s), value(v), Onupdate(nullptr), checkFun(ptr), preProcessFun(preFun), allowDynamicSet(allowDS) {
        if (v == NULL) {
            assert(false);
            return;
        }
    };
    virtual ~BaseVar(){};
    bool setVar(const string& value, string* errinfo = NULL, bool force = true) {
        if (!allowDynamicSet && !force) {
            if (errinfo != NULL) {
                *errinfo = "not allow dynamic set";
            }
            return false;
        }
        return set(value);
    }
    virtual string show() const = 0;
    virtual string default_show() const = 0;
    void setUpdate(funptr f) {
        Onupdate = f;
    }

    string getName() const{
        return name;
    }

    bool isallowDynamicSet() const{
        return allowDynamicSet;
    }

protected:
    virtual bool set(const string& value) = 0;
    virtual bool check(const string& value) {
        if (checkFun != NULL) {
            return checkFun(value);
        }
        return true;
    };

    string name = "";
    void* value = NULL;
    funptr Onupdate = NULL;
    checkfunptr checkFun = NULL;        // check the value whether it is valid
    preProcess preProcessFun = NULL;    // pre process for the value, such as remove the quotes
    bool allowDynamicSet = false;
};

class StringVar : public BaseVar {
public:
    StringVar(const string& name, void* v, checkfunptr ptr, preProcess preFun, bool allowDynamicSet)
        : BaseVar(name, v, ptr, preFun, allowDynamicSet), _defaultValue(*(string*)value) {
        if (!preProcessFun) {
            preProcessFun = removeQuotes;
        }
    };
    virtual string show() const {
        return "\"" + *(string*)value + "\"";
    };

    virtual string default_show() const {
        return "\"" + _defaultValue + "\"";
    };

private:
    bool set(const string& val) {
        auto v = preProcessFun ? preProcessFun(val) : val;
        if(!check(v)) return false;

        *(string*)value = v;

        if (Onupdate != NULL) Onupdate();
        return true;
    }
    std::string _defaultValue;
};

// support:int, uint32_t
class IntVar : public BaseVar {
public:
    IntVar(const string& name, void* v, checkfunptr ptr, preProcess preFun,
            int64_t minVal, int64_t maxVal, bool allowDynamicSet)
        : BaseVar(name, v, ptr, preFun, allowDynamicSet), _defaultValue(*(int *)value),
          minVal_(minVal), maxVal_(maxVal){};
    virtual string show() const {
        return std::to_string(*(int*)value);
    };
    virtual string default_show() const {
        return std::to_string(_defaultValue);
    };
private:
    bool set(const string& val) {
        auto v = preProcessFun ? preProcessFun(val) : val;
        if(!check(v)) return false;
        int64_t valTemp;

        try {
            valTemp = std::stoi(v);
        } catch (...) {
            LOG(ERROR) << "IntVar stoi err:" << v;
            return false;
        }
        if (valTemp < minVal_ || valTemp > maxVal_) {
            return false;
        }
        *reinterpret_cast<int *>(value) = valTemp;

        if (Onupdate != NULL) Onupdate();
        return true;
    }
    int _defaultValue;
    int64_t minVal_;
    int64_t maxVal_;
};

// support:int64_t, uint64_t
class Int64Var : public BaseVar {
public:
    Int64Var(const string& name, void* v, checkfunptr ptr, preProcess preFun,
            int64_t minVal, int64_t maxVal, bool allowDynamicSet)
            : BaseVar(name, v, ptr, preFun, allowDynamicSet), _defaultValue(*(int64_t *)value),
              minVal_(minVal), maxVal_(maxVal){};
    virtual string show() const {
        return std::to_string(*(int64_t *)value);
    };
    virtual string default_show() const {
        return std::to_string(_defaultValue);
    };

private:
    bool set(const string& val) {
        auto v = preProcessFun ? preProcessFun(val) : val;
        if(!check(v)) return false;
        int64_t valTemp;

        try {
            valTemp = std::stoll(v);
        } catch (...) {
            LOG(ERROR) << "Int64Var stoll err:" << v;
            return false;
        }
        if (valTemp < minVal_ || valTemp > maxVal_) {
            return false;
        }
        *reinterpret_cast<int64_t *>(value) = valTemp;

        if (Onupdate != NULL) Onupdate();
        return true;
    }
    int64_t _defaultValue;
    int64_t minVal_;
    int64_t maxVal_;
};

class FloatVar : public BaseVar {
public:
    FloatVar(const string& name, void* v, checkfunptr ptr, preProcess preFun, bool allowDynamicSet)
        : BaseVar(name, v, ptr, preFun, allowDynamicSet), _defaultValue(*(float*)value){};
    virtual string show() const {
        return std::to_string(*(float*)value);
    };
    virtual string default_show() const {
        return std::to_string(_defaultValue);
    };
private:
    bool set(const string& val) {
        auto v = preProcessFun ? preProcessFun(val) : val;
        if(!check(v)) return false;

        try {
            *(float*)value = std::stof(v);
        } catch (...) {
            LOG(ERROR) << "FloatVar stof err:" << v;
            return false;
        }
        if (Onupdate != NULL) Onupdate();
        return true;
    }
    float _defaultValue;
};

class BoolVar : public BaseVar {
public:
    BoolVar(const string& name, void* v, checkfunptr ptr, preProcess preFun, bool allowDynamicSet)
        : BaseVar(name, v, ptr, preFun, allowDynamicSet), _defaultValue(*(bool*)value){};
    virtual string show() const {
        return value ? "yes" : "no";
    };
    virtual string default_show() const {
        return _defaultValue ? "yes" : "no";
    };

private:
    bool set(const string& val) {
        auto v = preProcessFun ? preProcessFun(val) : val;
        if(!check(v)) return false;

        *(bool*)value = isOptionOn(v);

        if (Onupdate != NULL) Onupdate();
        return true;
    }
    bool _defaultValue;
};

class rewriteConfigState {
public:
    rewriteConfigState()
        : _hasTail(false){};
    ~rewriteConfigState(){};
    Status rewriteConfigReadOldFile(const std::string& confFile);
    void rewriteConfigOption(const std::string& option, const std::string& value, const std::string& defvalue);
    void rewriteConfigRewriteLine(const std::string& option, const std::string& line, bool force);
    std::string rewriteConfigFormatMemory(uint64_t bytes);
    void rewriteConfigRemoveOrphaned();
    std::string rewriteConfigGetContentFromState();
    Status rewriteConfigOverwriteFile(const std::string& confFile, const std::string& content);

private:
    std::unordered_map<std::string, std::list<uint64_t>> _optionToLine;
    std::unordered_map<std::string, std::list<uint64_t>> _rewritten;
    std::vector<std::string> _lines;
    bool _hasTail;
    const std::string _fixInfo = "# Generated by CONFIG REWRITE";
};

class ServerParams{
public:
    ServerParams();
    ~ServerParams();

    Status parseFile(const std::string& filename);
    bool registerOnupdate(const string& name, funptr ptr);
    string showAll() const;
    bool showVar(const string& key, string& info) const;
    bool showVar(const string& key, vector<string>& info) const;
    bool setVar(const string& name, const string& value, string* errinfo, bool force = true);
    Status rewriteConfig() const;
    uint32_t paramsNum() const {
        return _mapServerParams.size();
    }
    string getConfFile() const {
        return _confFile;
    }
    const std::unordered_map<string, int64_t>& getRocksdbOptions() const {
        return _rocksdbOptions;
    }
    BaseVar *serverParamsVar(const std::string &key) {
        return _mapServerParams[tendisplus::toLower(key)];
    }

private:
    map<string, BaseVar*> _mapServerParams;
    std::unordered_map<string, int64_t> _rocksdbOptions;
    std::string _confFile = "";
    std::set<std::string> _setConfFile;

public:
    std::string bindIp = "127.0.0.1";
    uint32_t port = 8903;
    std::string logLevel = "";
    std::string logDir = "./";

    std::string storageEngine = "rocks";
    std::string dbPath = "./db";
    std::string dumpPath = "./dump";
    std::string requirepass = "";
    std::string masterauth = "";
    std::string pidFile = "./tendisplus.pid";
    bool versionIncrease = true;
    bool generalLog = false;
    // false: For command "set a b", it don't check the type of 
    // "a" and update it directly. It can make set() faster. 
    // Default false. Redis layer can guarantee that it's safe
    bool checkKeyTypeForSet = false;

    uint32_t chunkSize = 0x4000;  // same as rediscluster
    uint32_t kvStoreCount = 10;

    uint32_t scanCntIndexMgr = 1000;
    uint32_t scanJobCntIndexMgr = 1;
    uint32_t delCntIndexMgr = 10000;
    uint32_t delJobCntIndexMgr = 1;
    uint32_t pauseTimeIndexMgr = 10;

    uint32_t protoMaxBulkLen = CONFIG_DEFAULT_PROTO_MAX_BULK_LEN;
    uint32_t dbNum = CONFIG_DEFAULT_DBNUM;

    bool noexpire = false;
    uint64_t maxBinlogKeepNum = 1000000;
    uint32_t minBinlogKeepSec = 0;
    uint64_t slaveBinlogKeepNum = 1;

    uint32_t maxClients = CONFIG_DEFAULT_MAX_CLIENTS;
    std::string slowlogPath = "./slowlog";
    uint32_t slowlogLogSlowerThan = CONFIG_DEFAULT_SLOWLOG_LOG_SLOWER_THAN;
    //uint32_t slowlogMaxLen = CONFIG_DEFAULT_SLOWLOG_LOG_MAX_LEN;
    uint32_t slowlogFlushInterval = CONFIG_DEFAULT_SLOWLOG_FLUSH_INTERVAL;
    uint64_t slowlogMaxLen = CONFIG_DEFAULT_SLOWLOG_LOG_MAX_LEN;
    bool slowlogFileEnabled = true;
    uint32_t netIoThreadNum = 0;
    uint32_t executorThreadNum = 0;
    uint32_t executorWorkPoolSize = 8;

    uint32_t binlogRateLimitMB = 64;
    uint32_t netBatchSize = 1024*1024;
    uint32_t netBatchTimeoutSec = 10;
    uint32_t timeoutSecBinlogWaitRsp = 30;
    uint32_t incrPushThreadnum = 4;
    uint32_t fullPushThreadnum = 4;
    uint32_t fullReceiveThreadnum = 4;
    uint32_t logRecycleThreadnum = 4;
    uint32_t truncateBinlogIntervalMs = 1000;
    uint32_t truncateBinlogNum = 50000;
    uint32_t binlogFileSizeMB = 64;
    uint32_t binlogFileSecs = 20*60;

    uint32_t keysDefaultLimit = 100;
    uint32_t lockWaitTimeOut = 3600;

    // parameter for rocksdb
    uint32_t rocksBlockcacheMB = 4096;
    bool rocksStrictCapacityLimit = false;
    std::string rocksWALDir = "";
    string rocksCompressType = "snappy";
    // WriteOptions
    bool rocksDisableWAL = false;
    bool rocksFlushLogAtTrxCommit = false;
    bool level0Compress = false;
    bool level1Compress = false;

    uint32_t bingLogSendBatch =  256;
    uint32_t bingLogSendBytes =  16 * 1024 * 1024;

    uint32_t migrateSenderThreadnum = 4;
    uint32_t migrateClearThreadnum = 4;
    uint32_t migrateReceiveThreadnum = 4;
    bool clusterEnabled = false;
    uint32_t migrateTaskSlotsLimit = 10;
    uint32_t migrateDistance = 10000;
    uint16_t migrateBinlogIter = 10;
    uint32_t migrateRateLimitMB = 32;
    uint32_t clusterNodeTimeout = 15000;
    bool clusterRequireFullCoverage = true;
    bool clusterSlaveNoFailover = false;
    uint32_t clusterMigrationBarrier = 1;
    uint32_t clusterSlaveValidityFactor = 50;

    // TODO(takenliu) delete this two parm after performance test.
    bool netIoMultiIoContext = false;
    bool executorMultiIoContext = false;
};
}  // namespace tendisplus

#endif  // SRC_TENDISPLUS_SERVER_SERVER_PARAMS_H_
