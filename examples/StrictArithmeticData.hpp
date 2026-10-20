#pragma once
#include "MultivariateData.hpp"

namespace regression_example {
inline std::vector<RegressionRow> arithmeticRows(int range) {
    std::vector<double> first,second;
    if(range==0){first={.5,1,2,4,8};second={-2,-1,0,1,2};}
    else if(range==1){first={.75,1.5,3,6};second={-1.5,-.5,.5,1.5};}
    else if(range==2){first={.25,16};second={-3,-1,1,3};}
    else throw std::invalid_argument("unknown arithmetic demonstration range");
    std::vector<RegressionRow> rows;
    for(double x:first)for(double y:second)rows.push_back({{x,y},std::log(x)+y/x});
    return rows;
}
inline void writeArithmeticFiles(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    const std::array names{"training.csv","interior.csv","exterior.csv"};
    for(int range=0;range<3;++range) {
        const auto path=directory/names[range];
        std::ofstream file(path);
        if(!file)throw std::runtime_error("cannot create "+path.string());
        file.imbue(std::locale::classic());file<<std::setprecision(std::numeric_limits<double>::max_digits10);
        file<<"first,second,target\n";
        for(const auto& row:arithmeticRows(range))file<<row.inputs[0]<<','<<row.inputs[1]<<','<<row.target<<'\n';
        file.close();if(!file)throw std::runtime_error("cannot finish "+path.string());
    }
}
} // namespace regression_example
