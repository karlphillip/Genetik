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
#include <RunCheckpoint.hpp>
#include <ScalarExpressionCheckpoint.hpp>
#include <ExpressionFormatting.hpp>

#include <DiscreteExpressionParsing.hpp>
#include <DiscreteExpressionFormatting.hpp>
#include <DiscreteSymbolicRegression.hpp>
#include <DiscreteExpressionCheckpoint.hpp>

#include <DiscreteRunCheckpoint.hpp>
