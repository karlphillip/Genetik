#pragma once
#include <ExpressionParsing.hpp>
#include <fstream>

namespace genetik_example {
// Bound before append; never use unbounded getline/iterator construction for input.
inline std::string readExpressionFile(const std::string& path,std::size_t maxBytes) {
    if(!maxBytes)throw std::invalid_argument("expression file byte allowance must be positive");
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::invalid_argument(path+": cannot open expression file");
    std::string text;char c;
    while(input.get(c)) {
        if(text.size()==maxBytes)throw ExpressionParseError(ExpressionParseCategory::Resource,maxBytes,"expression file byte limit");
        text.push_back(c);
    }
    if(input.bad())throw std::runtime_error(path+": expression file read failed");
    return text;
}
}
