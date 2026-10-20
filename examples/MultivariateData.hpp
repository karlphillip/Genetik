#pragma once
#include <Regression.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>

namespace regression_example {
inline std::vector<RegressionRow> multivariateRows(int range) {
    std::vector<double> values;
    if(range==0)values={-2,-1,0,1,2};
    else if(range==1)values={-1.5,-.5,.5,1.5};
    else if(range==2)values={-3,0,3};
    else throw std::invalid_argument("unknown demonstration range");
    std::vector<RegressionRow> rows;
    for(double first:values)for(double second:values) {
        if(range==2 && first==0 && second==0)continue;
        rows.push_back({{first,second},2.5*first-.75*second+1.2});
    }
    return rows;
}
inline void writeMultivariateFiles(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    const std::array names{"training.csv","interior.csv","exterior.csv"};
    for(int range=0;range<3;++range) {
        const auto path=directory/names[range];
        std::ofstream file(path);
        if(!file)throw std::runtime_error("cannot create "+path.string());
        file.imbue(std::locale::classic());file<<std::setprecision(std::numeric_limits<double>::max_digits10);
        file<<"first,second,target\n";
        for(const auto& row:multivariateRows(range))file<<row.inputs[0]<<','<<row.inputs[1]<<','<<row.target<<'\n';
        file.close();if(!file)throw std::runtime_error("cannot finish "+path.string());
    }
}
} // namespace regression_example
