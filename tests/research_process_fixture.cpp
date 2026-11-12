#include <iostream>
#include <iterator>
#include <string>

int main() {
    const std::string input(std::istreambuf_iterator<char>(std::cin), {});
    std::cout << "CPP:" << input;
    std::cerr << "CPPERR";
    return std::cout && std::cerr ? 0 : 1;
}
