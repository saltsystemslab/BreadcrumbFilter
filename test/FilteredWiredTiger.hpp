#pragma once
#include "wiredtiger.h"
#include <vector>
#include <iostream>
#include <filesystem>

//-----------------------------------------------------------------------------
// MurmurHash2, 64-bit versions, by Austin Appleby

// The same caveats as 32-bit MurmurHash2 apply here - beware of alignment 
// and endian-ness issues if used across multiple platforms.


// 64-bit hash for 64-bit platforms

uint64_t MurmurHash64A ( const void * key, int len, unsigned int seed )
{
	const uint64_t m = 0xc6a4a7935bd1e995;
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t * data = (const uint64_t *)key;
	const uint64_t * end = data + (len/8);

	while(data != end)
	{
		uint64_t k = *data++;

		k *= m; 
		k ^= k >> r; 
		k *= m; 

		h ^= k;
		h *= m; 
	}

	const unsigned char * data2 = (const unsigned char*)data;

	switch(len & 7)
	{
		case 7: h ^= (uint64_t)data2[6] << 48;
		case 6: h ^= (uint64_t)data2[5] << 40;
		case 5: h ^= (uint64_t)data2[4] << 32;
		case 4: h ^= (uint64_t)data2[3] << 24;
		case 3: h ^= (uint64_t)data2[2] << 16;
		case 2: h ^= (uint64_t)data2[1] << 8;
		case 1: h ^= (uint64_t)data2[0];
						h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}



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
static const size_t default_key_len = 8, default_val_len = 504;
static const std::string default_buffer_pool_size = "1024MB";
static const bool default_in_memory = false;

// class FilteredWiredTiger {
//     size_t fp = 0;
//     size_t seed = 0;
//     size_t key_len = default_key_len;
//     size_t val_len = default_val_len;

//     WT_CONNECTION *conn;
//     WT_SESSION *session;
//     WT_CURSOR *cursor;
    
//     FilteredWiredTiger(size_t cap_filter, size_t num_filters, size_t seed, size_t keylen = default_key_len, size_t vallen = default_val_len, std::string buffer_pool_size = default_buffer_pool_size, bool in_memory = default_in_memory):
//     seed{seed}, key_len{key_len}, val_len{val_len} {
//         char table_schema[max_schema_len];
//         char connection_config[max_conn_config_len];

//         if (std::filesystem::exists(wt_home))
//             std::filesystem::remove_all(wt_home);
//         std::filesystem::create_directory(wt_home);

//         sprintf(table_schema, "key_format=%lds,value_format=%lds", key_len, val_len);
//         uint64_t current_buffer_pool_size = buffer_pool_size;
//         sprintf(connection_config, "create,statistics=(all),direct_io=[data],cache_size=%ldMB,in_memory=", buffer_pool_size, in_memory ? "true" : "false");
//         error_check(wiredtiger_open(wt_home, NULL, connection_config, &conn));
//         error_check(conn->open_session(conn, NULL, NULL, &session));
//         error_check(session->create(session, "table:access", table_schema));
//         error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
//         std::cout << "[+] WiredTiger initialized" << std::endl;

//         for (size_t i=0; i < num_filters; i++) {
//             filters.emplace_back(cap_filter);
//         }
//     }


//     void insert(char *key, char *value) {
//         if (!filters[0].insert(MurmurHash64A(key, key_len, seed) % filters[0].range)) {
//             std::cerr << "Failed to insert into filter" << std::endl;
//             exit(-1);
//         }
//         insert_kv(cursor, key, value);
//     }

//     template<typename FT>
//     bool query_key(FT& f, char *key) {
//         if(!filters[0].query(MurmurHash64A(key, key_len, seed) % filters[0].range)) {
//             return false;
//         }
        
//         cursor->set_key(cursor, key);
//         int exists_err = cursor->search(cursor);
//         error_check(cursor->reset(cursor));
//         if (exists_err == 0) {
//             return true;
//         }
//         else if (exists_err == WT_NOTFOUND) {
//             return false;
//         }
//         else {
//             error_check(exists_err);
//         }
//     }

//     ~FilteredWiredTiger() {
//         error_check(conn->close(conn, NULL));
//     }
// };

// struct WiredTigerWrapper {
//     static constexpr std::string_view
//     name = "WiredTiger";

//     template<typename FTWrapper>
//     static std::vector<double> run(Settings s) {
//         size_t numThreads = s.numThreads;
//         if (numThreads == 0) {
//             std::cerr << "Cannot have 0 threads!!" << std::endl;
//             return {};
//         }
//         else if (numThreads > 1){
//             std::cerr << "no multithreaded merging yet" << std::endl;
//             return {};
//         }
//         if (!FTWrapper::canDelete) {
//             std::cerr << "Need deletions!" << std::endl;
//             return {};
//         }
//         if (!s.maxLoadFactor) {
//             std::cerr << "Does not have a max load factor!" << std::endl;
//             return std::vector < double > {};
//         }
//         double maxLoadFactor = *(s.maxLoadFactor);

//         // using FT = typename FTWrapper::type;
//         size_t filterSlots = s.N;
//         size_t N = static_cast<size_t>(s.N * maxLoadFactor);
//         // FT a(filterSlots);
//         // FT b(filterSlots);

//         std::vector <size_t> keys = generateKeys<FT>(a, 2 * N);
//         insertItems<FT>(a, keys, 0, N, std::string(FTWrapper::name));
//         if (!checkQuery(a, keys, 0, N)) {
//             std::cerr << "Failed to insert into a" << std::endl;
//             return std::vector < double > {std::numeric_limits<double>::max()};
//         }
//         insertItems<FT>(b, keys, N, 2 * N, std::string(FTWrapper::name));
//         if (!checkQuery(b, keys, N, 2 * N)) {
//             std::cerr << "Failed to insert into b" << std::endl;
//             return std::vector < double > {std::numeric_limits<double>::max()};
//         }
//         auto generator = createGenerator();

//         bool success = true;

//         FT *c;

//         double mergeTime = runTest([&]() {
//             // FT c(a, b);
//             c = new FT(a,b);
//             // if(!checkQuery(c, keys, 0, 2*N)) {
//             //     std::cerr << "Merge failed" << endl;
//             //     success = false;
//             //     exit(-1);
//             // }
//             if(!checkQuery(*c, keys, N-2, N+2)) {
//                 std::cerr << "Merge failed" << endl;
//                 success = false;
//                 exit(-1);
//             }
//         });

//         if (!checkFunctional(*c, keys, generator)) {
//             std::cerr << "Merge failed" << endl;
//             success = false;
//             exit(-1);
//         }
//         delete c;

//         if (!success) {
//             return std::vector < double > {std::numeric_limits<double>::max()};
//         }

//         return std::vector < double > {mergeTime};
//     }

//     template<typename FTWrapper>
//     static void analyze(Settings s, std::filesystem::path outputFolder, std::vector <std::vector<double>> outputs) {
//         double avgInsTime = 0;
//         for (auto v: outputs) {
//             avgInsTime += v[0] / outputs.size();
//         }

//         if (!s.maxLoadFactor) {
//             std::cerr << "Missing max load factor" << std::endl;
//             return;
//         }
//         double maxLoadFactor = *(s.maxLoadFactor);

//         double effectiveN = s.N * s.maxLoadFactor.value();
//         std::ofstream fout(outputFolder / (std::to_string(s.N) + ".txt"), std::ios_base::app);
//         fout << maxLoadFactor << " " << avgInsTime << " " << (effectiveN / avgInsTime) << std::endl;
//     }
// };