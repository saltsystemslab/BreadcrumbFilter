README
# Breadcrumb Filter
To test the build, we installed a fresh copy of Ubuntu Server 24.0.3 LTS. Note that running the benchmarks requires a processor with full support for AVX512. Additionally, while it is possible to run benchmarks on very small test cases, some of the default provided test configuration files require several dozen gigabytes of RAM to run (particularly for WiredTiger benchmarks).
## TODO: test with default g++ installed!
## Steps To Build
- First, we need to install required packages. We will be using g++-11 as the compiller, and we need to install swig for wiredtiger to work:
```shell
sudo apt update
sudo apt install swig g++-11 make cmake libssl-dev python3-dev
```
- Temporarily set g++-11 as the default compiler version:
```shell
mkdir -p /tmp/bin
ln -sf /usr/bin/g++-11 /tmp/bin/g++
export PATH="/tmp/bin:$PATH"
```
- Please update the submodules using the command 
```shell
git submodule update --init
```
- Build the VQF files. Please execute the following commands.
```shell
cd test/vqf
git checkout master
git pull
make clean && make THREAD=1 
cd ../..
```
### Disclaimer:
While running the single threaded VQF benchmarks, please make VQF without the ```THREAD = 1``` flag.
- Build the CQF files. Please execute the following commands.
```shell
cd test/cqf
git checkout master
git pull
make clean && make
cd ../..
```
- Build wiredtiger. Note that this will take a while. Please execute the following commands:
```shell
cd test/wiredtiger
mkdir build
cd build
cmake .. -DENABLE_STATIC=1 -DCMAKE_CXX_COMPILER=/usr/bin/g++-11
make
cd ../../..
```
## TODO: Seems like you have to run the make command twice?? First time I did not have libwiredtiger.a
- Build the Tester file. This is the main file required to run the benchmarks.
```shell
cd build
cmake ..
make clean && make
```

## TODO: There is some really weird issue where after running make with the avx2 file it just refuses to compile afterwards

## TODO: Move the config files from build to some other directory. Because I think the above is caused by some weird cmake cache issue and we want to just be able to do rm -rf build

### Building for AVX2 Benchmarks
If you want to run VQF with AVX2, then either compile VQF with a machine that does not support AVX512, or copy the file Makefile_VQF_AVX2 to test/vqf and run 
```make clean && make Makefile_VQF_AVX2```
in test/vqf to compile VQF with AVX2 set to true instead of AVX512. Here is the full set of commands to do so. Start from the base directory (not build):
```shell
cd test/vqf
cp ../Makefile_VQF_AVX2 Makefile_VQF_AVX2
make clean && make -f Makefile_VQF_AVX2
cd ../..
```

Additionally, a flag needs to be set with cmake, "USE_AVX512 = OFF." That is, instead of ```cmake ..``` run ```cmake .. -DUSE_AVX512=OFF``` (make sure to run make clean before any recompiling after switching this flag). That is:
```shell
cd build
cmake .. -DUSE_AVX512=OFF
make clean && make
```
### Note: if you switch between AVX512 and AVX2 in the same build directory, it may be necessary to clear cmake cache by running ```rm -rf build```.
## TODO: actually maybe just switch it to build_avx2 that would be nicer anyways.

## Steps To Run
Please use the following command format to run the code
```shell
numactl -N 0 -m 0 ./Tester <config_file> <output_file> <output_directory>
```
## Single-Threaded Benchmarks
We provide the following single-threaded benchmarks.
- Standard Benchmark: This benchmark will insert a certain number of keys into the filter, perform queries for the same keys, perform queries for keys not in the filter to measure false positives, and perform deletes if the filter supports it.
- Mixed Workload Benchmark: This benchmark will first insert a certain number of keys into the filter. It will then execute 1M operations consisting of inserts/queries/deletes in the proportion (0.3/0.4/0.3).
- Insert Delete Benchmark: This benchmark will first insert a certain number of keys, then attempt to delete them sequentially, and also at random.
- Load Factor Benchmark: This benchmark will measure how many inserts it takes for a filter to fail at various load factors.
### How To Run Single-Threaded Benchmarks
## TODO: Modify the ST_Config.txt file to clean it up.
- In order to run the single-threaded benchmarks, you need to modify the ```ST_Config.txt``` file available in the build folder. The file has the following format.
```text
<Benchmark Name>
NumTrials <Number of trials>
NumReplicants <Number of replicants>
NumKeys <Number of keys>
LoadFactorTicks <number of sections to divide the workload into>
MaxLoadFactor <between 0 and 1>
<Filter name>
```
The ```ST_Config.txt``` file contains configurations which just need to be changed in order to run any benchmark. For example, you will find the following benchmark.
```text
Benchmark
NumKeys 1048576
NumThreads 1
NumTrials 1
NumReplicants 1
LoadFactorTicks 20
MaxLoadFactor 0.83
PQF_8_22
```
You can modify the number of keys, threads, replicants, load factor ticks, max load factor and the filter name. Examples of all of these are provided in the test configuration files. All you need to do is uncomment those. 
An example command of how to run the single threaded tests is:
```shell
numactl -N 0 -m 0 ./Tester ST_Config.txt outST.txt analysis
```

We have provided several examples of such configurations in the configuration files themselves. If you want to run the experiment for a particular filter, please remove the ```#``` before it's name.
### Disclaimer
Please delete the ```outST.txt```/```outMT.txt``` files after running the single/multi-threaded benchmarks. Running with these files present will cause the experiments to not run at all.
The results of all the tests can be found in the path ```build/analysis/<BenchmarkName>/<FilterName>```

### AVX2 Disclaimer
If built with AVX2, only run using filters that support AVX2 (see example AVX2 file in build/ST_Config_AVX2.txt).

## Multithreaded Benchmarks
We provide the following multithreaded benchmarks.
- Multithreaded inserts/queries: This benchmark first measures the time it takes to perform inserts with various number of threads, after which it performs queries and reports the throughput for both.
- Mixed Workload Benchmark: This benchmark functions the same way as the one for single thread. It just executes the benchmark with multiple threads.

These benchmark configurations are found in a file called ```MT_Config.txt```. It can be run the same way as the single threaded tests. For example,
```shell
numactl -N 0 -m 0 ./Tester MT_Config.txt outMT.txt analysis 
```
**Please be advised that running the same benchmark consecutively will erase previous results of the same benchmark.**

## WiredTiger Benchmarks
## TODO: Flesh this out at least with some specific commands. Also update the files since I think there was some reason that in cache and in mem had to be separate files
We additionally support incorporating each of the filters into WiredTiger. We benchmark by first inserting many key-value pairs into WiredTiger. Then, for each filter, we reconfigure the cache size of WiredTiger so that the sum of the cache size and the filter size is WiredTigerQueryCacheSize. After this, the database is queried for many keys, with the (inverse of) the fraction of negative queries being a configurable parameter. Before querying the database, the filter is queried, and negative queries are filtered out.

See example WiredTiger benchmarks in the build directory. One important thing to note is that these benchmarks may use a significant amount of disk space, depending on the dataset size. Additionally, tests should be run fixing the configuration for WiredTiger: the program normally avoids rebuilding the database between consecutive runs, but if the parameters change, it must. Additionally, there may be some bugs if a test file contains several different database configurations; we did not test this case. Finally, do not run multiple of these tests simultaneously, as every test creates the same directory for the database.

## Building With Cuckoo XOR Hash Function
The code using the PQF hashing mechanism by default. If you would like to use the cuckoo hash function for load factor experiments, please rebuild the CMake with the following command:
```shell
cmake -DUSE_CUCKOO_HASH=ON ..
make
```
After doing so, you can run the load factor experiments to see how the failure load factor changes.
