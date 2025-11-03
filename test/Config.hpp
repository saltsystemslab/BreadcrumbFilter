#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <vector>
#include <string>
#include <set>
#include <iostream>
#include <cmath>
#include <tuple>
#include <optional>
#include <map>
#include <cassert>

std::vector<std::pair<std::string, std::vector<double>>> readConfig(const char* filename, std::set<std::string> keywords);

using namespace std;
using namespace std::literals::string_view_literals;

struct Settings {
    //need a somewhat better way to do settings, since now the setting for every type of thing (filter type, test type, the general test handler) are all in the same struct. Not sure exactly what a better (and still simple) design would be
    static auto SettingTypes() {
        return std::set < std::string >
               {"NumKeys", "NumTrials", "NumThreads", "NumReplicants", "LoadFactorTicks", "MaxLoadFactor",
                "MinLoadFactor", "MaxInsertDeleteRatio", 
                "WiredTigerInsertCacheSize", "WiredTigerQueryCacheSize", "WiredTigerKeySize", 
                "WiredTigerValSize", "WiredTigerInMem", "WiredTigerInvFracNonrandom", "WiredTigerQueryN",
                "WiredTigerUseHashFunc"};
    }

    std::string TestName;
    std::string FTName;
    size_t N;
    size_t numReplicants = 1;
    size_t numThreads{1};
    size_t loadFactorTicks = 1;
    std::optional<double> maxLoadFactor; //optional since its a necessary value to set and has no reasonable default
    double minLoadFactor = 0.0;
    double maxInsertDeleteRatio = 10.0;
    std::map<std::string, double> other_settings;
    size_t nops_mixed = 0;

    auto getTuple() const {
        // std::cout << "gettuple" << std::endl;
        return std::tie(TestName, FTName, N, numReplicants, numThreads, loadFactorTicks, maxLoadFactor, minLoadFactor,
                        maxInsertDeleteRatio,other_settings);
    }

    bool operator==(const Settings &rhs) const {
        // std::cout << "CMP ==" << std::endl;
        return getTuple() == rhs.getTuple();
    }

    bool operator<(const Settings &rhs) const {
        // std::cout << "CMP <" << std::endl;
        return getTuple() < rhs.getTuple();
    }

    //doesn't set testname or ftname here that is done externally. Yeah not great system whatever
    void setval(std::string type, std::vector<double> values) {
        if (SettingTypes().count(type) == 0) {
            std::cerr << "Set an incorrect setting: " << type << std::endl;
            exit(-1);
        }
        if (values.size() != 1) {
            std::cerr << "Can only set one value!" << std::endl;
            exit(-1);
        }

        if (type == "NumKeys"s) {
            N = static_cast<size_t>(values[0]);
        } else if (type == "NumThreads"s) {
            numThreads = static_cast<size_t>(values[0]);
        } else if (type == "NumReplicants"s) {
            numReplicants = static_cast<size_t>(values[0]);
        } else if (type == "LoadFactorTicks"s) {
            loadFactorTicks = static_cast<size_t>(values[0]);
        } else if (type == "MaxLoadFactor") {
            maxLoadFactor = values[0];
        } else if (type == "MinLoadFactor") {
            minLoadFactor = values[0];
        } else if (type == "MaxInsertDeleteRatio") {
            maxInsertDeleteRatio = values[0];
        }
        else {
            other_settings[type] = values[0];
        }
    }
};

std::ostream &operator<<(std::ostream &os, const Settings &s);

template<typename T, typename LambdaT>
auto transform_vector(std::vector <T> &v, LambdaT lambda) {
    using TElemType =
    decltype(std::declval<LambdaT>()(std::declval<T>()));
    // std::vector<typename FunctionSignature<LambdaT>::RetT> retv;
    std::vector <TElemType> retv;
    for (T &t: v) {
        retv.push_back(lambda(t));
    }
    return retv;
}

struct CompressedSettings {
    std::string TestName;
    std::string FTName;
    std::vector <size_t> Ns;
    std::optional <size_t> numTrials;
    size_t numReplicants = 1;
    std::vector <size_t> numThreads{1};
    size_t loadFactorTicks;
    std::optional<double> maxLoadFactor;
    double minLoadFactor = 0.0;
    double maxInsertDeleteRatio = 10.0;
    std::map<std::string, double> other_settings;

    std::vector <Settings> getSettingsCombos();

    void setval(std::string type, std::vector<double> values);
};

#endif