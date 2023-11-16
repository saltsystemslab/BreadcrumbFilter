#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <iostream>
#include <functional>
#include <fstream>
#include <limits>
#include <filesystem>
#include <stdlib.h>
#include <algorithm>
#include <cctype>

#include "DynamicPrefixFilter.hpp"
#include "TestWrappers.hpp"

using namespace std;

struct TestResult { //Times are all in microseconds for running not one test but all (really just chosen arbitrarily like that)
    size_t insertTime;
    size_t successfulQueryTime;
    size_t randomQueryTime;
    double falsePositiveRate;
    size_t removeTime;
    size_t mixedTime;
    double lf;
    size_t sizeFilter;
    size_t N;
    size_t NFilter;
    double sBLR; //successful backyard lookup rate
    double rBLR; //random backyard lookup rate
};

template<typename FT>
void insertItems(FT& filter, vector<size_t>& keys, size_t start, size_t end) {
    // std::cout << "start: " << start << ", end: " << end << std::endl;
    for(size_t i{start}; i < end; i++) {
        filter.insert(keys[i]);
    }
}

template<typename FT>
void mixedItems(FT& filter, vector<size_t>& keys, vector<size_t>& extraKeys, vector<size_t>& randomKeys, size_t start, size_t end) {
    // std::cout << "start: " << start << ", end: " << end << std::endl;
    size_t f = 0l; //Just to ensure compiler does not optimize away the code
    for(size_t i{start}; i < end; i++) {
        filter.insert(extraKeys[i]);
        //using a different index to be reasonably sure its not in cache when its removed
        f += filter.query(keys[(end-i-1)+start]); //Might be a successful query, might be not (starts of successful since haven't inserted at the end, moves to random later)
        f += filter.query(randomKeys[i]);
        filter.remove(keys[i]);
    }
    if(f < 10){
        exit(-1);
    }
}

template<typename FT>
void checkQuery(FT& filter, vector<size_t>& keys, size_t start, size_t end, bool& status) {
    for(size_t i{start}; i < end; i++) {
        if(!filter.query(keys[i])) {
            cerr << "Query on " << keys[i] << " " << i << " failed." << endl;
            // exit(EXIT_FAILURE);
            status = false;
            return;
        }
    }
}

template<typename FT>
void getNumFalsePositives(FT& filter, vector<size_t>& FPRkeys, size_t start, size_t end, uint64_t& fpr_res) {
    uint64_t fpr = 0;
    for(size_t i{start}; i < end; i++) {
        fpr += filter.query(FPRkeys[i]);
    }
    // return fpr;
    fpr_res = fpr;
}

template<typename FT>
void removeItems(FT& filter, vector<size_t>& keys, size_t start, size_t end) {
    for(size_t i{start}; i < end; i++) {
        // assert(filter.remove(keys[i]));
        filter.remove(keys[i]);
    }
}

//max ratio is the ratio of how much space you make for filter items to how many items you actually insert
template<typename FT, bool CanDelete = true, bool getBLR = false, bool testBatch=false>
TestResult benchFilter(size_t N, double ratio, size_t numThreads = 1) {
    random_device rd;
    mt19937 generator (rd());
    TestResult res;
    res.N = static_cast<size_t>(N*ratio);
    res.NFilter = N;
    res.lf = ratio;

    FT filter(res.NFilter);
    res.sizeFilter = filter.sizeFilter();

    N = res.N;

    vector<size_t> keys(N);
    vector<size_t> extraKeys(N);
    vector<size_t> FPRkeys(N);
    constexpr size_t batchSize = 128;
    vector<size_t> batch(batchSize);
    // vector<bool> status(batchSize);
    uniform_int_distribution<size_t> keyDist(0, -1ull);
    for(size_t i{0}; i < N; i++) {
        keys[i] = keyDist(generator) % filter.range;
        FPRkeys[i] = keyDist(generator) % filter.range;
        extraKeys[i] = keyDist(generator) % filter.range;
    }
    // this_thread::sleep_for(chrono::seconds(delayBetweenTests)); //Just sleep to try to keep turbo boost to full


    auto start = chrono::high_resolution_clock::now();
    if (numThreads > 1) {
        std::vector<std::thread> threads;
        for(size_t i = 0; i < numThreads; i++) {
            // std::cout << i << std::endl;
            threads.push_back(std::thread([&, i] () -> void {insertItems(filter, keys, (i*N) / numThreads, ((i+1)*N) / numThreads);}));
        }
        for(auto& th: threads) {
            th.join();
        }
    }
    else {
        insertItems(filter, keys, 0, N);
    }
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
    // cout << "inserted" << endl;
    res.insertTime = (size_t)duration.count();
    // this_thread::sleep_for(chrono::seconds(delayBetweenTests));

    start = chrono::high_resolution_clock::now(); 
    // uint64_t x = 0;
    bool status = true;
    if (numThreads > 1) {
        std::vector<std::thread> threads;
        for(size_t i = 0; i < numThreads; i++) {
            // std::cout << i << std::endl;
            threads.push_back(std::thread([&, i] () -> void {checkQuery(filter, keys, (i*N) / numThreads, ((i+1)*N) / numThreads, status);}));
        }
        for(auto& th: threads) {
            th.join();
        }
    }
    else {
        checkQuery(filter, keys, 0, N, status);
    }
    if(!status) {
        res.successfulQueryTime = -1ull;
        return res;
    }
    // cout << x << endl;
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    // cout << "queried" << endl;
    res.successfulQueryTime = (size_t)duration.count();
    if constexpr (getBLR) {
        res.sBLR = ((double)filter.backyardLookupCount) / N;
        filter.backyardLookupCount = 0;
    }
    // this_thread::sleep_for(chrono::seconds(delayBetweenTests));

    start = chrono::high_resolution_clock::now();
    uint64_t fpr = 0;
    if (numThreads > 1) {
        std::vector<std::thread> threads;
        std::vector<uint64_t> fpr_res(numThreads);
        for(size_t i = 0; i < numThreads; i++) {
            // std::cout << i << std::endl;
            threads.push_back(std::thread([&, i] () -> void {getNumFalsePositives(filter, FPRkeys, (i*N) / numThreads, ((i+1)*N) / numThreads, fpr_res[i]);}));
        }
        for(size_t i = 0; i < numThreads; i++) {
            threads[i].join();
            fpr+=fpr_res[i];
        }
    }
    else {
        getNumFalsePositives(filter, FPRkeys, 0, N, fpr);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    // cout << "fpred" << endl;
    res.randomQueryTime = (size_t)duration.count();
    res.falsePositiveRate = ((double)fpr) / N;
    if constexpr (getBLR) {
        res.rBLR = ((double)filter.backyardLookupCount) / N;
        filter.backyardLookupCount = 0;
    }
    // this_thread::sleep_for(chrono::seconds(delayBetweenTests));

    if constexpr (CanDelete) {
        start = chrono::high_resolution_clock::now();
        if (numThreads > 1) {
            std::vector<std::thread> threads;
            for(size_t i = 0; i < numThreads; i++) {
                // std::cout << i << std::endl;
                threads.push_back(std::thread([&, i] () -> void {removeItems(filter, keys, (i*N) / numThreads, ((i+1)*N) / numThreads);}));
            }
            for(auto& th: threads) {
                th.join();
            }
        }
        else {
            removeItems(filter, keys, 0, N);
        }
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::microseconds>(end-start);
        res.removeTime = (size_t)duration.count();



        //MIXED workload testing. Innefficient but first reinsert all the keys
        if (numThreads > 1) {
            std::vector<std::thread> threads;
            for(size_t i = 0; i < numThreads; i++) {
                // std::cout << i << std::endl;
                threads.push_back(std::thread([&, i] () -> void {insertItems(filter, keys, (i*N) / numThreads, ((i+1)*N) / numThreads);}));
            }
            for(auto& th: threads) {
                th.join();
            }
        }
        else {
            insertItems(filter, keys, 0, N);
        }
        start = chrono::high_resolution_clock::now();
        if (numThreads > 1) { //Somewhat complicated to add this
            std::vector<std::thread> threads;
            for(size_t i = 0; i < numThreads; i++) {
                threads.push_back(std::thread([&, i] () -> void {mixedItems(filter, keys, extraKeys, FPRkeys, (i*N) / numThreads, ((i+1)*N) / numThreads);}));
            }
            for(auto& th: threads){
                th.join();
            }
        }
        else {
            mixedItems(filter, keys, extraKeys, FPRkeys, 0, N);
        }
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::microseconds>(end-start);
        res.mixedTime = (size_t)duration.count();
    }
    else {
        // res.removeTime = numeric_limits<double>::infinity();
        res.removeTime = -1ull;
        res.mixedTime = -1ull;
    }

    return res;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FQR = false, bool testBatch = false, bool Threaded = false>
TestResult benchPQF(size_t N, double ratio = 1.0, size_t numThreads = 1) {
    using FilterType = DynamicPrefixFilter::PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FQR, Threaded>;
    return benchFilter<FilterType, true, true, testBatch>(N, ratio, numThreads);
}

class FilterTester {
    vector<string> filterNames;
    vector<function<TestResult()>> benchFunctions;
    vector<size_t> Ns;

    vector<vector<TestResult>> testResults;
    string individualFilterHeader = "N, \"Size of Filter\", \"Insert Time\", \"Positive Query Time\", \"Random Query Time\", \"False Positive Rate\", \"Remove Time\""; //Idk why doing it this way but here we report how long total to insert, and in summary we say how much per item

    string summaryHeader = "\"Filter Name\", N, \"Insert(ns)\", \"Positive Query(ns)\", \"Random Query(ns)\", \"Inverse False Positive Rate\", \"Remove Time\", \"Bits Per Key\"";

    public: 
        FilterTester(){}

        void addTest(string filterName, function<TestResult()> benchFunction, size_t N) {
            filterNames.push_back(filterName);
            benchFunctions.push_back(benchFunction);
            testResults.push_back(vector<TestResult>());
            Ns.push_back(N);
        }

        void addLoadFactors(string filterName, function<TestResult(size_t, double)> f, size_t N, double minLF, double maxLF, double step) {
            // cout << filterName << " ";
            for(double lf = minLF; lf <= maxLF+step - 1e-5; lf+=step) {
                if(lf > maxLF) {
                    lf = maxLF;
                }
                // cout << lf << " ";
                addTest(filterName, [=]() -> TestResult {return f(N, lf);}, N);
            }
            // cout << endl;
        }

        // void addThreadedTest(string filterName, function<TestResult(size_t, double, size_t)> f, size_t N, double minLF, double maxLF, double step, vector<size_t> threadCounts) {
        //     for(double lf = minLF; lf <= maxLF+step - 1e-5; lf+=step) {
        //         if(lf > maxLF) {
        //             lf = maxLF;
        //         }
        //         for(size_t threadCount : threadCounts) {
        //             addTest(filterName, [=]() -> TestResult {return f(lf, N, threadCount);}, N);
        //         }
        //     }
        // }

        void runAll(size_t numTests, string ofolder, bool append = false) {
            random_device rd;
            mt19937 generator (rd());
            // if(!filesystem::exists("results")) {
            //     filesystem::create_directory("results");
            // }

            string folder = ofolder;

            if(!filesystem::exists(folder)) {
                filesystem::create_directory(folder);
            }

            // vector<ofstream> fileOutputs;
            // for(string filterName: filterNames) {
            for(size_t i=0; i < filterNames.size(); i++){
                string filterName = filterNames[i];
                string bfolder = folder + "/" + filterName+"/";
                if(!filesystem::exists(bfolder)) {
                    filesystem::create_directory(bfolder);
                }
                if(!append) {
                    ofstream fout(bfolder+to_string(Ns[i]));//lazy way to empty the file
                }
            }

            for(size_t test{0}; test < numTests; test++) {
                
                //Randomize order just in case that does something
                vector<size_t> order(benchFunctions.size());
                for(size_t i{0}; i < order.size(); i++) order[i] = i;
                shuffle(order.begin(), order.end(), generator);

                for(size_t c{0}; c < benchFunctions.size(); c++) {
                    size_t i = order[c];

                    string filterName = filterNames[i];
                    string bfolder = folder + "/" + filterName+"/";
                    ofstream fout(bfolder+to_string(Ns[i]), std::ios_base::app);
                    
                    TestResult t = benchFunctions[i]();
                    fout << t.lf << " " << t.insertTime << " " << t.successfulQueryTime << " " << t.randomQueryTime << " " << t.removeTime << " " << t.mixedTime << " " << t.falsePositiveRate << " " << t.sizeFilter << " " << t.sBLR << " " << t.rBLR << "\n";
                    cout << "Tested " << filterName << " with load factor " << t.lf << endl;
                }
            }
        }
};

auto getFilters(size_t numThreads) {
    map<string, function<TestResult(size_t, double)>> filters;
    
    if(numThreads == 1) {
        filters["PQF_22-8"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 22, 26, 18, 8, 32, 32, false, false>(N, lf);};
        filters["PQF_22-8-Batch"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 22, 26, 18, 8, 32, 32, false, true>(N, lf);};
        filters["PQF_22-8-FQR"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 22, 26, 18, 8, 32, 32, true>(N, lf);};
        filters["PQF_22-8-FQR-Batch"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 22, 26, 18, 8, 32, 32, true, true>(N, lf);};
        filters["PQF_22-8BB"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 22, 26, 37, 8, 32, 64>(N, lf);};
        filters["PQF_22-8BB-FQR"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 22, 26, 37, 8, 32, 64, true>(N, lf);};

        filters["PQF_31-8"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 31, 25, 17, 8, 32, 32>(N, lf);};
        filters["PQF_31-8-FQR"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 31, 25, 17, 8, 32, 32, true>(N, lf);};

        filters["PQF_62-8"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 62, 50, 34, 8, 64, 64>(N, lf);};
        filters["PQF_62-8-FQR"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 62, 50, 34, 8, 64, 64, true>(N, lf);};

        filters["PQF_53-8"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 53, 51, 35, 8, 64, 64>(N, lf);};
        filters["PQF_53-8-FQR"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 53, 51, 35, 8, 64, 64, true>(N, lf);};
        filters["PQF_53-8-Batch"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 53, 51, 35, 8, 64, 64, false, true>(N, lf);};
        filters["PQF_53-8-FQR-Batch"] = [] (size_t N, double lf) -> TestResult {return benchPQF<8, 53, 51, 35, 8, 64, 64, true, true>(N, lf);};

        filters["PQF16"] = [] (size_t N, double lf) -> TestResult {return benchPQF<16, 36, 28, 22, 8, 64, 64, false, false>(N, lf);};
        filters["PQF16-FRQ"] = [] (size_t N, double lf) -> TestResult {return benchPQF<16, 36, 28, 22, 8, 64, 64, true, false>(N, lf);};
        filters["PQF16-Batch"] = [] (size_t N, double lf) -> TestResult {return benchPQF<16, 36, 28, 22, 8, 64, 64, false, true>(N, lf);};
        filters["PQF16-FRQ-Batch"] = [] (size_t N, double lf) -> TestResult {return benchPQF<16, 36, 28, 22, 8, 64, 64, true, true>(N, lf);};


        using PF_TC_Wrapper = PFFilterAPIWrapper<Prefix_Filter<TC_shortcut>, sizePF<TC_shortcut, sizeTC>, false>;
        filters["PF-TC"] = [] (size_t N, double lf) -> TestResult {return benchFilter<PF_TC_Wrapper, false>(N, lf);};

        using CF12_Flex = cuckoofilter::CuckooFilterStable<uint64_t, 12>;
        using PF_CFF12_Wrapper = PFFilterAPIWrapper<Prefix_Filter<CF12_Flex>, sizePF<CF12_Flex, sizeCFF>>;
        filters["PF-CF12F"] = [] (size_t N, double lf) -> TestResult {return benchFilter<PF_CFF12_Wrapper, false>(N, lf);};

        using PF_BBFF_Wrapper = PFFilterAPIWrapper<Prefix_Filter<SimdBlockFilterFixed<>>, sizePF<SimdBlockFilterFixed<>, sizeBBFF>>;
        filters["PF-BBFF"] = [] (size_t N, double lf) -> TestResult {return benchFilter<PF_BBFF_Wrapper, false>(N, lf);};

        using TC_Wrapper = PFFilterAPIWrapper<TC_shortcut, sizeTC, true>;
        filters["TC"] = [] (size_t N, double lf) -> TestResult {return benchFilter<TC_Wrapper, true>(N, lf);};

        using CFF12_Wrapper = PFFilterAPIWrapper<CF12_Flex, sizeCFF, true>;
        filters["CF-12-Flex"] = [] (size_t N, double lf) -> TestResult {return benchFilter<CFF12_Wrapper, true>(N, lf);};

        using BBFF_Wrapper = PFFilterAPIWrapper<SimdBlockFilterFixed<>, sizeBBFF>;
        filters["BBF-Flex"] = [] (size_t N, double lf) -> TestResult {return benchFilter<BBFF_Wrapper, false>(N, lf);};

        using OriginalCF12 = CuckooWrapper<size_t, 12>;
        filters["OrigCF12"] = [] (size_t N, double lf) -> TestResult {return benchFilter<OriginalCF12, true>(N, lf);};

        using OriginalCF16 = CuckooWrapper<size_t, 16>;
        filters["OrigCF16"] = [] (size_t N, double lf) -> TestResult {return benchFilter<OriginalCF16, true>(N, lf);};

        filters["Morton"] = [] (size_t N, double lf) -> TestResult {return benchFilter<MortonWrapper<CompressedCuckoo::Morton3_12>, true, false, false>(N, lf);};
        filters["Morton-Batch"] = [] (size_t N, double lf) -> TestResult {return benchFilter<MortonWrapper<CompressedCuckoo::Morton3_12>, true, false, true>(N, lf);};

        filters["Morton18"] = [] (size_t N, double lf) -> TestResult {return benchFilter<MortonWrapper<CompressedCuckoo::Morton3_18>, true, false, false>(N, lf);};
        filters["Morton18-Batch"] = [] (size_t N, double lf) -> TestResult {return benchFilter<MortonWrapper<CompressedCuckoo::Morton3_18>, true, false, true>(N, lf);};
    }

    filters["VQF"] = [numThreads] (size_t N, double lf) -> TestResult {return benchFilter<VQFWrapper, true>(N, lf, numThreads);}; //be careful with this one as with >1 thread gotta recompile it

    filters["PQF_21-8-T"] = [numThreads] (size_t N, double lf) -> TestResult {return benchPQF<8, 21, 26, 18, 8, 32, 32, false, false, true>(N, lf, numThreads);};
    
    filters["PQF_21-8-FQR-T"] = [numThreads] (size_t N, double lf) -> TestResult {return benchPQF<8, 21, 26, 18, 8, 32, 32, true, false, true>(N, lf, numThreads);};
    
    filters["PQF_52-8-T"] = [numThreads] (size_t N, double lf) -> TestResult {return benchPQF<8, 52, 51, 35, 8, 64, 64, false, false, true>(N, lf, numThreads);};
    filters["PQF_52-8-FQR-T"] = [numThreads] (size_t N, double lf) -> TestResult {return benchPQF<8, 52, 51, 35, 8, 64, 64, true, false, true>(N, lf, numThreads);};

    filters["PQF16-T"] = [numThreads] (size_t N, double lf) -> TestResult {return benchPQF<16, 35, 28, 22, 8, 64, 64, false, false, true>(N, lf, numThreads);};
    filters["PQF16-FRQ-T"] = [numThreads] (size_t N, double lf) -> TestResult {return benchPQF<16, 35, 28, 22, 8, 64, 64, true, false, true>(N, lf, numThreads);};

    return filters;
}

int main(int argc, char* argv[]) {
    size_t N = 1ull << 24;
    size_t NumTests = 1;
    string filterConfigurationPath = "";
    string ofolder = "output";
    size_t threads = 1;
    bool append = false;
    for(size_t i=0; i < argc-1; i++) {
        string larg(argv[i]);
        transform(larg.begin(), larg.end(), larg.begin(), [](unsigned char c){ return tolower(c); }); //why the lambda necessary?
        
        if(larg == "-fc") { //filter configuration
            filterConfigurationPath = argv[i+1];
        }
        else if(larg == "-n") {
            N = atoll(argv[i+1]);
        }
        else if(larg == "-logn") {
            N = 1ull << atoll(argv[i+1]);
        }
        else if(larg == "-nt") { //Num tests
            NumTests = atoll(argv[i+1]);
        }
        else if(larg == "-of") { //output folder
            ofolder = argv[i+1];
        }
        else if(larg == "-tc") { //thread count
            threads = atoll(argv[i+1]);
        }
        else if(larg == "-a") { //toggles append mode. Say either 0 or 1
            append = atoi(argv[i+1]);
        }
        else {
            continue;
        }
        i++;
    }

    if(filterConfigurationPath == "") {
        cerr << "Must specify a filter confuration file with -fc!" << endl;
        exit(1);
    }
    cout << N << " " << NumTests << " " << threads << " " << append << " " << filterConfigurationPath << " " << ofolder << endl;

    auto filters = getFilters(threads);

    FilterTester ft;
    ifstream filterFeeder(filterConfigurationPath);
    string filterName;
    double minLF, maxLF, step;
    while(filterFeeder >> filterName >> minLF >> maxLF >> step) {
        if(filters.count(filterName)) {
            ft.addLoadFactors(filterName, filters[filterName], N, minLF, maxLF, step);
        }
        else {
            cerr << filterName << " is not a configured filter in the benchmark. Perhaps you have multiple threads but are using a single threaded filter?" << endl;
        }
    }
    
    ft.runAll(NumTests, ofolder, append);
}
