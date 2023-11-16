import subprocess
import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-td", "-testdir", type=str)
parser.add_argument("-ad", "-analdir", type=str)
parser.add_argument("-test", type=str)
parser.add_argument("-make", type=str) #either "no", "yes", or "clean". Only do "yes" or "no" if you are sure that the vqf is compiled correctly, as it needs to be recompiled to enable/disable threads
parser.add_argument("-nt", type=int)
parser.add_argument("-tc", type=int)

args = parser.parse_args()

test = "All"
if args.test:
    test = args.test

num_trials = 3
if args.nt:
    num_trials = args.nt

make = "clean"
if args.make:
    make = args.make

testdir = "test"
if args.td:
    testdir = args.td

analdir = "analysis"
if args.ad:
    analdir = args.ad

thread_count = 1
if args.tc:
    thread_count = args.tc

available_tests = {"ST" : (False, "SingleThreadedConfig.txt"), "Batch": (False, "SingleThreadedBatchConfig.txt"), "MT": (True, "MultiThreadedConfig.txt"), "NO": (False, "NewOptimizationsTesting.txt")}

tests = [test]
if test == "All":
    tests = ["ST", "Batch", "MT"] #laziness oops

previously_threaded = available_tests[tests[0]][0]
# if make == "yes":
#     subprocess.run(f"python3 buildscript.py -vt {previously_threaded}", shell=True)
# elif make == "clean":
#     subprocess.run(f"python3 buildscript.py -clean True -vt {previously_threaded}", shell=True)

if make == "yes":
    subprocess.run(f"python3 buildscript.py", shell=True)
elif make == "clean":
    subprocess.run(f"python3 buildscript.py -clean True", shell=True)

current_path = os.path.dirname(__file__)
rel_exec_path = "../build/BenchFilter"
exec_path = os.path.join(current_path, rel_exec_path)
parallel_rel_exec_path = "../build/BenchFilter"
parallel_exec_path = os.path.join(current_path, parallel_rel_exec_path)

log_sizes_to_test = [28] #make configurable later?
thread_counts_to_test = []
i = 1
while i <= thread_count:
    thread_counts_to_test.append(i)
    i = i * 2
if thread_count * 2 != i:
    thread_counts_to_test.append(thread_count)


for t in tests:
    threaded, rel_config_path = available_tests[t]
    # if threaded != previously_threaded:
    #     subprocess.run(f"python3 buildscript.py -clean True -vt {threaded}", shell=True)

    import errno
    # Taken from https://stackoverflow.com/a/600612/119527
    def mkdir_p(path):
        try:
            os.makedirs(path)
        except OSError as exc: # Python >2.5
            if exc.errno == errno.EEXIST and os.path.isdir(path):
                pass
            else: raise
    
    test_output_folder = os.path.join(current_path, testdir, t)
    analysis_output_folder = os.path.join(current_path, analdir, t)
    mkdir_p(test_output_folder)

    config_path = os.path.join(current_path, rel_config_path)
    lfbench_path = os.path.join(current_path, "lfbench.py")
    for lsize in log_sizes_to_test:
        if threaded:
            for tc in thread_counts_to_test:
                subprocess.run(f"{parallel_exec_path} -logn {lsize} -nt {num_trials} -of {test_output_folder}-{tc}T -tc {tc} -fc {config_path}", shell=True)
                subprocess.run(f"python3 {lfbench_path} -i {test_output_folder}-{tc}T -o {analysis_output_folder}-{tc}T", shell=True)
        else:
            subprocess.run(f"{exec_path} -logn {lsize} -nt {num_trials} -of {test_output_folder} -fc {config_path}", shell=True)
            subprocess.run(f"python3 {lfbench_path} -i {test_output_folder} -o {analysis_output_folder}", shell=True)
