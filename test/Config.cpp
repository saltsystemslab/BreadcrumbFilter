#include "Config.hpp"

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