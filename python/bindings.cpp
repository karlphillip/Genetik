#include "RegressionFacade.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstring>

namespace py = pybind11;
namespace {

py::array_t<double, py::array::c_style | py::array::forcecast> asRealArray(const py::array& source,
                                                                            const char* name) {
    const char kind = source.dtype().kind();
    if (kind != 'b' && kind != 'i' && kind != 'u' && kind != 'f')
        throw py::type_error(std::string(name) + " must have a real numerical dtype");
    if (source.dtype().itemsize() > 8)
        throw py::type_error(std::string(name) + " dtypes wider than float64/int64 are unsupported");
    auto converted = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(source);
    if (!converted) throw py::type_error(std::string(name) + " cannot be converted to float64");
    return converted;
}

std::vector<RegressionRow> trainingRows(const py::array& x, const py::array& y) {
    auto features = asRealArray(x, "X");
    auto targets = asRealArray(y, "y");
    const auto xi = features.request();
    const auto yi = targets.request();
    if (xi.ndim != 2) throw py::value_error("X must have shape (n_samples, n_features)");
    if (yi.ndim != 1) throw py::value_error("y must have shape (n_samples,)");
    if (xi.shape[0] == 0 || xi.shape[1] == 0) throw py::value_error("X must have nonempty samples and features");
    if (yi.shape[0] != xi.shape[0]) throw py::value_error("X and y sample counts differ");
    const auto* xData = static_cast<const double*>(xi.ptr);
    const auto* yData = static_cast<const double*>(yi.ptr);
    std::vector<RegressionRow> rows(static_cast<std::size_t>(xi.shape[0]));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].inputs.assign(xData + i * static_cast<std::size_t>(xi.shape[1]),
                              xData + (i + 1) * static_cast<std::size_t>(xi.shape[1]));
        rows[i].target = yData[i];
    }
    return rows;
}

std::vector<std::vector<double>> predictionRows(const py::array& x, std::size_t expectedFeatures) {
    auto features = asRealArray(x, "X");
    const auto info = features.request();
    if (info.ndim != 2) throw py::value_error("X must have shape (n_samples, n_features)");
    if (static_cast<std::size_t>(info.shape[1]) != expectedFeatures)
        throw py::value_error("X feature count differs from fitted model");
    const auto* data = static_cast<const double*>(info.ptr);
    std::vector<std::vector<double>> rows(static_cast<std::size_t>(info.shape[0]));
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i].assign(data + i * expectedFeatures, data + (i + 1) * expectedFeatures);
    return rows;
}

ExpressionKind operatorKind(const std::string& name) {
    if (name == "add") return ExpressionKind::Add;
    if (name == "sub") return ExpressionKind::Subtract;
    if (name == "mul") return ExpressionKind::Multiply;
    if (name == "div") return ExpressionKind::Divide;
    if (name == "log") return ExpressionKind::Log;
    if (name == "sin") return ExpressionKind::Sin;
    if (name == "cos") return ExpressionKind::Cos;
    if (name == "exp") return ExpressionKind::Exp;
    if (name == "sqrt") return ExpressionKind::Sqrt;
    if (name == "powi") return ExpressionKind::Powi;
    if (name == "pow") return ExpressionKind::Pow;
    throw py::value_error("unsupported Genetik operator: " + name);
}

py::dict statsDict(const PythonRegressionStats& s) {
    py::dict result;
    result["fitted"] = s.fitted;
    result["timed_out"] = s.timedOut;
    result["stop_reason"] = searchStopName(s.stop);
    result["loss_attempts"] = s.lossAttempts;
    result["row_attempts"] = s.rowAttempts;
    result["fitting_invocations"] = s.fittingInvocations;
    result["fitting_loss_attempts"] = s.fittingLossAttempts;
    result["fitting_proposals"] = s.fittingProposals;
    result["fitting_accepted"] = s.fittingAccepted;
    result["invalid_losses"] = s.invalidLosses;
    result["candidate_evaluations"] = s.candidateEvaluations;
    result["attempted_generations"] = s.attemptedGenerations;
    result["published_generations"] = s.publishedGenerations;
    result["best_generation"] = s.bestGeneration;
    if (s.trainingMse) result["training_mse"] = *s.trainingMse;
    else result["training_mse"] = py::none();
    result["nodes"] = s.nodes;
    result["depth"] = s.depth;
    result["search_seconds"] = s.searchSeconds;
    result["rng_state"] = s.rngState;
    result["raw_initial_expressions"] = s.rawInitialExpressions;
    result["prepared_initial_expressions"] = s.preparedInitialExpressions;
    py::list initialFitness;
    for (const auto& v : s.preparedInitialLosses)
        initialFitness.append(v ? py::cast(*v) : py::none());
    result["prepared_initial_losses"] = std::move(initialFitness);
    return result;
}

} // namespace

PYBIND11_MODULE(_native, module) {
    module.doc() = "Native bindings to Genetik's existing C++ symbolic-regression engine";
    py::class_<PythonRegressionFacade>(module, "Regressor")
        .def(py::init([](std::uint32_t seed, unsigned workers, std::uint64_t evaluationBudget,
                         std::uint64_t lossBudget,
                         std::size_t population, std::uint32_t maxGenerations,
                         std::size_t maxNodes, std::size_t maxDepth, double maxAbsConstant,
                         bool fitConstants, std::uint64_t fitLossAttempts, double continuousProbability,
                         const std::vector<std::string>& operators, py::object maxTime,
                         double trainingThreshold, double fitThreshold) {
            PythonRegressionConfig config;
            config.seed = seed;
            config.workers = workers;
            config.evaluationBudget = evaluationBudget;
            config.lossBudget = lossBudget;
            config.population = population;
            config.maxGenerations = maxGenerations;
            config.maxNodes = maxNodes;
            config.maxDepth = maxDepth;
            config.maxAbsConstant = maxAbsConstant;
            config.fitConstants = fitConstants;
            config.fitLossAttempts = fitLossAttempts;
            config.continuousProbability = continuousProbability;
            config.operators.clear();
            for (const auto& op : operators) config.operators.push_back(operatorKind(op));
            if (!maxTime.is_none()) config.maxTimeSeconds = maxTime.cast<double>();
            config.trainingThreshold = trainingThreshold;
            config.fitThreshold = fitThreshold;
            return PythonRegressionFacade(std::move(config));
        }), py::arg("seed"), py::arg("workers"), py::arg("evaluation_budget"), py::arg("loss_budget"), py::arg("population"),
            py::arg("max_generations"), py::arg("max_nodes"), py::arg("max_depth"),
            py::arg("max_abs_constant"), py::arg("fit_constants"), py::arg("fit_loss_attempts"),
            py::arg("continuous_probability"), py::arg("operators"), py::arg("max_time") = py::none(),
            py::arg("training_threshold") = 1e-14, py::arg("fit_threshold") = 1e-14)
        .def("fit", [](PythonRegressionFacade& self, const py::array& x, const py::array& y) {
            const auto rows = trainingRows(x, y); // all Python-owned memory is copied before releasing the GIL
            {
                py::gil_scoped_release release;
                self.fit(rows);
            }
            return statsDict(self.stats());
        })
        .def("predict", [](const PythonRegressionFacade& self, const py::array& x) {
            const auto rows = predictionRows(x, self.featureCount());
            std::vector<double> values;
            {
                py::gil_scoped_release release;
                values = self.predict(rows);
            }
            py::array_t<double> result(values.size());
            std::copy(values.begin(), values.end(), result.mutable_data());
            return result;
        })
        .def("sympy_expression", &PythonRegressionFacade::sympyExpression, py::arg("feature_names"))
        .def_property_readonly("fitted", &PythonRegressionFacade::fitted)
        .def_property_readonly("expression", &PythonRegressionFacade::expression)
        .def_property_readonly("stats", [](const PythonRegressionFacade& self) { return statsDict(self.stats()); });
}
