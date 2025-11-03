#include "Config.hpp"
#include "TesterTools.hpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <functional>

double stodExit(const std::string& s, size_t line) {
    double i;
    try {
        size_t pos;
        i = std::stod(s, &pos);
        if(pos != s.size()) {
            std::cerr << "There is some error in the config file on line " << line << " (not an integer where should be)!" << std::endl;
            std::cerr << "What was passed in was " << s << std::endl;
            exit(-1);
        }
    } catch(...) {
        std::cerr << "There is some error in the config file on line " << line << " (not an integer where should be)!" << std::endl;
        std::cerr << "What was passed in was " << s << std::endl;
        exit(-1);
    }
    return i;
}

//DOES NOT WORK FOR std::vector<bool>!!!
// template<typename T>
// T& vecAtExit(std::vector<T>& v, size_t line, size_t index) {
//     try
//     {
//         return v.at(index);
//     }
//     catch (...)
//     {
//         std::cerr << "There is some error in the config file on line " << line << " (missing some elements)!" << std::endl;
//         exit(-1);
//     }
// }

std::vector<std::pair<std::string, std::vector<double>>> readConfig(const char* filename, std::set<std::string> keywords) {
    
    std::ifstream reader(filename);
    std::string line;
    std::vector<std::string> tokens;
    std::vector<size_t> lineNumbers;
    for(size_t lineNum = 0; std::getline(reader, line); lineNum++) {
        std::stringstream ss(line);

        std::string token;
        while(ss >> token) {
            if(token[0] == '#') { //then this is a comment from now on in the line
                break;
            }
            tokens.push_back(token);
            lineNumbers.push_back(lineNum);
        }
        lineNum++;
    }

    size_t curLine = 0;
    std::vector<std::pair<std::string, std::vector<double>>> output;
    for(size_t i=0; i < tokens.size();) {
        // curLine = vecAtExit(lineNumbers, curLine, i);
        // std::string configType = vecAtExit(tokens, curLine, i);
        std::string configType = tokens[i];
        // std::cout << "Configtype: " << configType << std::endl;
        curLine = lineNumbers[i];

        if(keywords.count(configType) == 0) {
            std::cerr << "Not a valid config type (" << configType << ")" << std::endl;
            std::cerr << "(Error was on line " << curLine << ")" << std::endl;
        }

        std::vector<double> configVals;
        i++;
        while(i < tokens.size() && keywords.count(tokens[i]) == 0) {
            // std::cout << tokens[i] << " ";
            curLine = lineNumbers[i];
            configVals.push_back(stodExit(tokens[i], curLine));
            i++;
        }
        // std::cout << std::endl;
        // i++;
        // curLine = vecAtExit(lineNumbers, curLine, i);
        // while(i < tokens.size() && keywords.count(vecAtExit(tokens, curLine, i)) == 0) {
        //     configVals.push_back(stodExit(vecAtExit(tokens, curLine, i), curLine));
        //     curLine = vecAtExit(lineNumbers, curLine, i);

        //     i++;
        // }

        output.push_back({configType, configVals});
    }

    return output;
}


std::ostream &operator<<(std::ostream &os, const Settings &s) {
    os << "# Settings for a particular run:" << "\n";
    os << s.TestName << "\n";
    os << "NumKeys " << s.N << "\n";
    os << "NumThreads " << s.numThreads << "\n";
    os << "NumReplicants " << s.numReplicants << "\n";
    os << "LoadFactorTicks " << s.loadFactorTicks << "\n";
    if (s.maxLoadFactor)
        os << "MaxLoadFactor " << (*(s.maxLoadFactor)) << "\n";
    os << "MinLoadFactor " << s.minLoadFactor << "\n";
    os << "MaxInsertDeleteRatio " << s.maxInsertDeleteRatio << "\n";
    for (auto it = s.other_settings.begin(); it != s.other_settings.end(); it++)
    {
        // std::cout << "ps " << it->first << " " << ((size_t)it->second) << std::endl;
        os << it->first   
              << " "
              << ((size_t)it->second)
              << std::endl;
    }
    os << s.FTName << "\n";
    return os;
}


size_t roundDoublePos(double d) {
    long long l = std::llround(d);
    if (l < 0) {
        std::cerr << "Trying to round a double to positive number, but it was negative (probably settings issue)"
                  << std::endl;
        exit(-1);
    }
    return static_cast<size_t>(l);
}

std::vector<Settings> CompressedSettings::getSettingsCombos() {
    std::vector <Settings> output;
    for (size_t N: Ns) {
        for (size_t NT: numThreads) {
            assert(numTrials.has_value());
            for (size_t i = 0; i < (*numTrials); i++) {
                output.push_back(Settings{TestName, FTName, N, numReplicants, NT, loadFactorTicks, maxLoadFactor,
                                            minLoadFactor, maxInsertDeleteRatio, other_settings});
            }
        }
    }
    return output;
}

void CompressedSettings::setval(std::string type, std::vector<double> values) {
    if (Settings::SettingTypes().count(type) == 0) {
        std::cerr << "Set an incorrect setting: " << type << std::endl;
        exit(-1);
    }

    if (type == "NumKeys"s) {
        Ns = transform_vector(values, &roundDoublePos);
    } else if (type == "NumThreads"s) {
        numThreads = transform_vector(values, &roundDoublePos);
    } else {
        if (values.size() != 1) {
            std::cerr << "Can only set one value for " << type << std::endl;
            exit(-1);
        }
        if (type == "NumTrials") {
            numTrials = static_cast<size_t>(values[0]);
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
}