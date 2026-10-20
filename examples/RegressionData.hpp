#pragma once
// Application-layer table loading. No file-format dependency is required by genetik_lib.
#include <Regression.hpp>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <string_view>

namespace regression_example {
struct TableLimits {
    std::size_t maxBytes = 16*1024*1024, maxLineBytes = 16384, maxRows = 50000;
    std::size_t maxColumns = 64, maxNameBytes = 128, maxNumberBytes = 256;
};
struct LoadedData {
    std::vector<std::string> header;
    std::vector<std::size_t> inputColumns;
    std::size_t targetColumn;
    std::vector<RegressionRow> rows;
};
inline std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t") - first + 1);
}
[[noreturn]] inline void dataError(const std::string& path, std::size_t row, std::size_t column, const std::string& detail) {
    throw std::invalid_argument(path + ":row " + std::to_string(row) + ":column " + std::to_string(column) + ": " + detail);
}
inline std::vector<std::string> splitColumns(std::string_view value) {
    std::vector<std::string> fields;
    for (;;) {
        const auto comma = value.find(',');
        fields.emplace_back(trim(value.substr(0, comma)));
        if (comma == std::string_view::npos) return fields;
        value.remove_prefix(comma + 1);
    }
}
inline double number(std::string_view text, const std::string& path, std::size_t row, std::size_t column,
                     const TableLimits& limits) {
    text = trim(text);
    if (text.empty() || text.size() > limits.maxNumberBytes) dataError(path,row,column,"missing or oversized numeric field");
    if (text.front() == '+') {
        text.remove_prefix(1);
        if (text.empty() || text.front() == '-' || text.front() == '+') dataError(path,row,column,"invalid numeric sign");
    }
    double value = 0;
    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size() || !std::isfinite(value))
        dataError(path,row,column,"expected a complete finite binary64 decimal/scientific number");
    return value;
}
inline LoadedData loadData(const std::string& path, const std::vector<std::string>& inputs, const std::string& target,
                           TableLimits limits = {}) {
    if (!limits.maxBytes || !limits.maxLineBytes || !limits.maxRows || !limits.maxColumns || !limits.maxNameBytes || !limits.maxNumberBytes)
        throw std::invalid_argument("table limits must be positive");
    if (inputs.empty() || target.empty()) throw std::invalid_argument("select at least one input and one target");
    std::set<std::string> selected;
    for (const auto& input : inputs)
        if (input.empty() || input == target || !selected.insert(input).second)
            throw std::invalid_argument(path+": inputs must be nonempty, unique and exclude target '"+target+"'");
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::invalid_argument(path+": cannot open data file");
    LoadedData result{};
    std::size_t bytes = 0, row = 0;
    bool headerRead = false;
    std::string line;
    auto process = [&] {
        ++row;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (trim(line).empty()) return;
        const auto quote = line.find('"');
        if (quote != std::string::npos)
            dataError(path,row,1+static_cast<std::size_t>(std::count(line.begin(),line.begin()+quote,',')),"quoting is unsupported");
        const auto fields = splitColumns(line);
        if (fields.size() > limits.maxColumns) dataError(path,row,limits.maxColumns+1,"column limit exceeded");
        if (!headerRead) {
            std::set<std::string> names;
            for (std::size_t i=0;i<fields.size();++i) {
                if (fields[i].empty() || fields[i].size()>limits.maxNameBytes || !names.insert(fields[i]).second)
                    dataError(path,row,i+1,"empty, oversized or duplicate header name");
                for (unsigned char ch:fields[i]) if (ch<32 || ch>126) dataError(path,row,i+1,"header names must be printable ASCII");
            }
            result.header = fields;
            const auto index = [&](const std::string& name) {
                const auto found = std::find(fields.begin(),fields.end(),name);
                if (found == fields.end()) dataError(path,row,1,"unknown selected column '"+name+"'");
                return static_cast<std::size_t>(found-fields.begin());
            };
            for (const auto& input:inputs) result.inputColumns.push_back(index(input));
            result.targetColumn = index(target); headerRead = true; return;
        }
        if (fields.size()!=result.header.size()) dataError(path,row,std::min(fields.size(),result.header.size())+1,"row width differs from header");
        if (result.rows.size()>=limits.maxRows) dataError(path,row,1,"data-row limit exceeded");
        std::vector<double> values;
        values.reserve(fields.size());
        for (std::size_t i=0;i<fields.size();++i) values.push_back(number(fields[i],path,row,i+1,limits));
        RegressionRow ready{{},values[result.targetColumn]};
        for (auto column:result.inputColumns) ready.inputs.push_back(values[column]);
        result.rows.push_back(std::move(ready));
    };
    char ch;
    while (file.get(ch)) {
        if (bytes == limits.maxBytes) dataError(path,row+1,1,"file byte limit exceeded");
        ++bytes;
        if (ch=='\n') {process();line.clear();}
        else {
            if (line.size() == limits.maxLineBytes) dataError(path,row+1,1,"line byte limit exceeded");
            line.push_back(ch);
        }
    }
    if (file.bad() || !file.eof()) throw std::runtime_error(path+": read failed");
    if (!line.empty()) process();
    if (!headerRead || result.rows.empty()) dataError(path,row+1,1,"empty dataset (header and numeric rows required)");
    return result; // only returned after the entire file has passed validation
}
} // namespace regression_example
