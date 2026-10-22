#include <IntegralRegression.hpp>
#pragma once
#include <SymbolicRegression.hpp>
#include <TypedSymbolicRegression.hpp>
#include <Creature.hpp>
#include <ExpressionParsing.hpp>
#include <TypedExpressionParsing.hpp>
static_assert(__cplusplus >= 202002L, "Genetik target must request C++20");
bool symbolic_check();

bool typed_check();

#include <ScalarTemplateRegression.hpp>

#include <ExpressionDerivative.hpp>

#include <DerivativeRegression.hpp>
