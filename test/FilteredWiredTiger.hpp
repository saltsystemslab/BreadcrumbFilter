#pragma once
#include "wiredtiger.h"
#include "TesterTools.hpp"
#include "Config.hpp"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <iomanip>

//-----------------------------------------------------------------------------
// MurmurHash2, 64-bit versions, by Austin Appleby

// The same caveats as 32-bit MurmurHash2 apply here - beware of alignment 
// and endian-ness issues if used across multiple platforms.


// 64-bit hash for 64-bit platforms

uint64_t MurmurHash64A ( const void * key, int len, unsigned int seed );



// Plan:
// Wrapper around Wiredtiger that adds a filter(s)
// Can configure negative query rate outside of that
// Maybe also add the option to add multiple filters
// This would simulate a distributed cache use case, maybe.

static inline void error_check(int ret)
{
    if (ret != 0) {
        std::cerr << "WiredTiger Error: " << wiredtiger_strerror(ret) << std::endl;
        exit(ret);
    }
}

static inline void insert_kv(WT_CURSOR *cursor, char *key, char *value)
{
    cursor->set_key(cursor, key);
    cursor->set_value(cursor, value);
    error_check(cursor->insert(cursor));
}

static const char *wt_home = "./wt_database_home";
static const uint32_t max_schema_len = 128;
static const uint32_t max_conn_config_len = 128;
static const size_t default_key_len = 16, default_val_len = 496;
static const size_t default_insert_buffer_pool_size_mb = 4096;
static const size_t default_query_buffer_pool_size_mb = 128;
static const bool default_in_memory = false;

struct FilteredWiredTiger {
    size_t fp = 0;
    size_t seed = 0;
    size_t key_len = default_key_len;
    size_t val_len = default_val_len;
    size_t insert_buffer_pool_size_mb;
    size_t query_buffer_pool_size_mb;
    char table_schema[max_schema_len];
    char connection_config[max_conn_config_len];
    bool in_memory = default_in_memory;
    bool initialized = false;

    std::vector<uint8_t> kvs;
    std::vector<uint8_t> random_kvs;

    WT_CONNECTION *conn;
    WT_SESSION *session;
    WT_CURSOR *cursor;
    
    FilteredWiredTiger(size_t key_len = default_key_len, size_t val_len = default_val_len, size_t insert_buffer_pool_size_mb = default_insert_buffer_pool_size_mb, size_t query_buffer_pool_size_mb = default_query_buffer_pool_size_mb, bool in_memory = default_in_memory, size_t seed = 42):
    seed{seed}, key_len{key_len}, val_len{val_len},
    insert_buffer_pool_size_mb{insert_buffer_pool_size_mb},
    query_buffer_pool_size_mb{query_buffer_pool_size_mb},
    in_memory{in_memory} {
    }

    void insert(char *key, char *value) {
        // if (!filters[0].insert(MurmurHash64A(key, key_len, seed) % filters[0].range)) {
        //     std::cerr << "Failed to insert into filter" << std::endl;
        //     exit(-1);
        // }
        insert_kv(cursor, key, value);
    }

    void insert_kvs(char* kvs, size_t num) {
        for(size_t i = 0; i < num; i++, kvs+=(key_len+val_len)) {
            if ((i % 1000000) == 0) {
                std::cout << i << std::endl;
            }
            insert(kvs, kvs + key_len);
        }
    }

    size_t num_inserted() {
        return kvs.size() / (key_len + val_len);
    }

    void reset_and_insert(size_t num, size_t random_num) {
        sprintf(table_schema, "key_format=%lds,value_format=%lds", key_len, val_len);
        if (std::filesystem::exists(wt_home))
            std::filesystem::remove_all(wt_home);
        std::filesystem::create_directory(wt_home);

        sprintf(connection_config, "create,statistics=(all),direct_io=[data],cache_size=%ldMB,in_memory=%s", insert_buffer_pool_size_mb, in_memory ? "true" : "false");
        error_check(wiredtiger_open(wt_home, NULL, connection_config, &conn));
        error_check(conn->open_session(conn, NULL, NULL, &session));
        error_check(session->create(session, "table:access", table_schema));
        error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
        std::cout << "[+] WiredTiger initialized" << std::endl;

        error_check(cursor->reset(cursor));

        size_t kvsize = key_len + val_len;
        kvs = genBytes(num * kvsize);
        // just gonna have random_kvs be same size as kvs.
        random_kvs = genBytes(random_num * kvsize);
        insert_kvs((char*)kvs.data(), num);
        std::cout << "[+] Keys inserted into wiredtiger" << std::endl;

        // error_check(conn->close(conn, NULL));
        error_check(cursor->reset(cursor));
        initialized = true;
    }

    size_t hash_key(char* key, size_t range, bool useHashFunc) {
        if(useHashFunc) {
            return MurmurHash64A(key, key_len, seed) % range;
        }
        else {
            assert(key_len == 8);
            return (*((size_t*)key)) % range;
        }
    }

    template<typename FT>
    void insert_into_filter(FT& f, bool useHashFunc) {
        for(size_t i = 0, j=0; i < kvs.size(); i+=(key_len+val_len), j++) {
            size_t hash = hash_key((char*)&kvs[i], f.range, useHashFunc);
            // if (i % 1001 == 0)
            //     std::cout << hash << std::endl;
            if(!f.insert(hash)) {
                std::cerr << i << " " << j << " " << f.range << " filter full!" << std::endl;
                exit(-1);
            }
        }
    }

    template<typename FT>
    void initialize_to_filter(FT& f) {
        size_t filter_size_mb = f.sizeFilter() / 1'000'000;
        size_t db_size_mb = query_buffer_pool_size_mb - filter_size_mb;
        // sprintf(connection_config, "statistics=(all),direct_io=[data],cache_size=%ldMB,in_memory=%s", db_size_mb, in_memory ? "true" : "false");
        sprintf(connection_config, "cache_size=%ldMB", db_size_mb);
        std::cout << connection_config << std::endl;
        error_check(conn->reconfigure(conn, connection_config));
        // error_check(wiredtiger_open(wt_home, NULL, connection_config, &conn));
        // error_check(conn->open_session(conn, NULL, NULL, &session));
        // error_check(session->create(session, "table:access", table_schema));
        // error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
    }

    void reset() {
        // error_check(conn->close(conn, NULL));
    }

    template<typename FT>
    bool query_filter(FT& f, char* key, bool useHashFunc) {
        size_t hash = hash_key(key, f.range, useHashFunc);
        return f.query(hash);
    }

    bool query_db(char* key) {
        cursor->set_key(cursor, key);
        int exists_err = cursor->search(cursor);
        error_check(cursor->reset(cursor));
        if (exists_err == 0) {
            return true;
        }
        else if (exists_err == WT_NOTFOUND) {
            return false;
        }
        else {
            error_check(exists_err);
            return false;
        }
    }

    template<typename FT>
    bool query_key(FT& f, char *key, bool useHashFunc) {
        if(query_filter(f, key, useHashFunc)) {
            return query_db(key);
        }
        return false;
    }

    template<typename FT>
    void check_no_false_negatives(FT& f, bool useHashFunc) {
        for(size_t i = 0; i < kvs.size(); i+=(key_len+val_len)) {
            // size_t hash = MurmurHash64A((char*)&kvs[i], key_len, seed) % f.range;
            size_t hash = hash_key((char*)&kvs[i], f.range, useHashFunc);
            if(!f.query(hash)) {
                std::cerr << "false negatives!" << std::endl;
                exit(-1);
            }
        }
    }

    template<typename FT>
    size_t query_bench(FT& f, size_t num, size_t invfrac_nonrandom, bool useHashFunc) {
        size_t kvsize = key_len + val_len;
        size_t fpr = 0;
        size_t super_fpr = 0;
        for(size_t i=0, j=0, k=0; i < num; i++) {
            if ((i % 1000000) == 0) {
                std::cout << i << std::endl;
            }
            // std::cout << "HEHEHEHEHEH" << std::endl;
            if (((invfrac_nonrandom == 0) || ((i % invfrac_nonrandom) == 0)) && (invfrac_nonrandom < 1'000'000)) {
                // std::cout << "DAFAFAFAFFAF " << " " << invfrac_nonrandom << " " <<  std::endl;
                // exit(-1);
                if(!query_key(f, (char*)&kvs[j], useHashFunc)) {
                    std::cerr << "false negatives!" << std::endl;
                    exit(-1);
                }
                j+=kvsize;
                j%=kvs.size();
            }
            else {
                // std::cout << "GOOLLLLAAAAAA" << std::endl;
                if(query_filter(f, (char*)&random_kvs[k], useHashFunc)) {
                    // std::cout << "HOOOOOLLLLLLA" << std::endl;
                    fpr += 1;
                    if (query_db((char*)&random_kvs[k])) {
                        super_fpr += 1;
                    }
                }
                k+=kvsize;
                k%=random_kvs.size();
            }
        }
        if (super_fpr) {
            std::cout << "# Got really unlucky with db, wow." << std::endl;
        }
        return fpr;
    }

    ~FilteredWiredTiger() {
        if(conn != NULL)
            error_check(conn->close(conn, NULL));
    }
};

static FilteredWiredTiger fwt{};

struct WiredTigerBenchmark {
    static constexpr std::string_view name = "WiredTiger";

    template<typename FTWrapper>
    static std::vector<double> run(Settings s) {
        // std::cout << "GOT HERE" << std::endl;
        bool diff = !fwt.initialized;
        size_t invfrac_nonrandom = 1;
        size_t queryN = s.N;
        bool useHashFunc = true;
        if(s.other_settings.count("WiredTigerQueryN") > 0) {
            queryN = s.other_settings["WiredTigerQueryN"];
        }
        if(s.other_settings.count("WiredTigerInsertCacheSize") > 0) {
            diff |= fwt.insert_buffer_pool_size_mb != ((size_t)s.other_settings["WiredTigerInsertCacheSize"]);
            std::cout << diff << std::endl;
            fwt.insert_buffer_pool_size_mb = s.other_settings["WiredTigerInsertCacheSize"];
        }
        if(s.other_settings.count("WiredTigerQueryCacheSize") > 0) {
            fwt.query_buffer_pool_size_mb = s.other_settings["WiredTigerQueryCacheSize"];
        }
        if(s.other_settings.count("WiredTigerInvFracNonrandom") > 0) {
            invfrac_nonrandom = s.other_settings["WiredTigerInvFracNonrandom"];
        }
        if(s.other_settings.count("WiredTigerUseHashFunc") > 0) {
            // std::cout << "GOOBUNGUS" << std::endl;
            useHashFunc = s.other_settings["WiredTigerUseHashFunc"];
        }
        if(s.other_settings.count("WiredTigerKeySize") > 0) {
            diff |= fwt.key_len != ((size_t)s.other_settings["WiredTigerKeySize"]);
            std::cout << diff << std::endl;
            fwt.key_len = s.other_settings["WiredTigerKeySize"];
        }
        if(s.other_settings.count("WiredTigerValSize") > 0) {
            diff |= fwt.val_len != ((size_t)s.other_settings["WiredTigerValSize"]);
            std::cout << diff << std::endl;
            fwt.val_len = s.other_settings["WiredTigerValSize"];
        }
        if(s.other_settings.count("WiredTigerInMem") > 0) {
            // diff |= fwt.in_memory != ((bool)s.other_settings["WiredTigerInMem"]);
            // std::cout << diff << std::endl;
            fwt.in_memory = s.other_settings["WiredTigerInMem"];
        }
        size_t numThreads = s.numThreads;
        if (numThreads == 0) {
            std::cerr << "Cannot have 0 threads!!" << std::endl;
            return {};
        }
        else if (numThreads > 1){
            std::cerr << "no multithreaded wiredtiger yet" << std::endl;
            return {};
        }
        if (!FTWrapper::canDelete) {
            std::cerr << "Need deletions!" << std::endl;
            return {};
        }
        if (!s.maxLoadFactor) {
            std::cerr << "Does not have a max load factor!" << std::endl;
            return std::vector < double > {};
        }
        using FT = typename FTWrapper::type;
        // size_t filterSlots = static_cast<size_t>(s.N);
        // FT f(filterSlots);
        // size_t N = static_cast<size_t>(s.N * (*(s.maxLoadFactor)));
        if (diff || (s.N != fwt.num_inserted())) {
            std::cout << "resetting" << std::endl;
            fwt.reset_and_insert(s.N, queryN);
        }
        double maxLoadFactor = *(s.maxLoadFactor);
        // std::cout << "nn " << s.N << " " << filterSlots << std::endl;
        size_t filterSlots = static_cast<size_t>(s.N / maxLoadFactor);
        FT f(filterSlots);
        std::cout << "nn " << s.N << " " << filterSlots << " " << f.range << std::endl;
        
        double filterInsertTime = runTest([&]() {
            fwt.insert_into_filter(f, useHashFunc);
        });
        fwt.check_no_false_negatives(f, useHashFunc);

        fwt.initialize_to_filter(f);

        size_t fpr;
        double queryTime = runTest([&]() {
            fpr = fwt.query_bench(f, queryN, invfrac_nonrandom, useHashFunc);
        });
        fwt.reset();

        return std::vector < double > {filterInsertTime, queryTime, (double)fpr, (double)invfrac_nonrandom};
    }

    template<typename FTWrapper>
    static void analyze(Settings s, std::filesystem::path outputFolder, std::vector <std::vector<double>> outputs) {
        double avgFilterInsertTime = 0;
        double avgQueryTime = 0;
        double fpr = 0;
        size_t invfrac_nonrandom = (size_t) outputs[0][3];
        for (auto v: outputs) {
            avgFilterInsertTime += v[0] / outputs.size();
            avgQueryTime += v[1] / outputs.size();
            fpr += v[2] / outputs.size();
            if (((size_t) v[3]) != invfrac_nonrandom) {
                std::cerr << "not all with same invfrac" << std::endl;
                exit(-1);
            }
        }

        std::ofstream fout(outputFolder / (std::to_string(s.N) + ".txt"), std::ios_base::app);
        // fout << s << std::endl;
        size_t queryN = s.N;
        if(s.other_settings.count("WiredTigerQueryN") > 0) {
            queryN = s.other_settings["WiredTigerQueryN"];
        }
        // fout << s.other_settings["WiredTigerInsertCacheSize"] << " " << s.other_settings["WiredTigerQueryCacheSize"] << " " << s.other_settings["WiredTigerKeySize"] << " " << s.other_settings["WiredTigerValSize"] << " " << s.other_settings["WiredTigerInMem"] << " " << s.other_settings["WiredTigerInvFracNonrandom"] << std::endl;
        fout << s;
        fout << "Average Filter Insert Throughput" << std::setw(40) << "Average Query Throughput (Filter + DB)" << std::setw(40) << "False Positive Rate" << std::endl;
        fout << (s.N / avgFilterInsertTime) << std::setw(40) << (queryN / avgQueryTime) << std::setw(40) << (fpr / queryN) << std::endl;
    }
};