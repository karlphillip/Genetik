#include <Expression.hpp>
#include <array>
static_assert(__cplusplus>=202002L);
int main(){return Expression::variable(0).evaluate(std::array{2.}).value()==2 ? 0:1;}
