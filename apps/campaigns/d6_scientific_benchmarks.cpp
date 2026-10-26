#include <ExpressionFormatting.hpp>
#include <ExpressionParsing.hpp>
#include <ScientificRegression.hpp>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
Expression expression(const std::string &text, std::size_t inputs) {
  return parseExpression(text, inputs, {}, ExpressionLimits{23, 6, 8, 1000});
}
ScientificEquationSystem oscillator(bool assessment) {
  std::vector<ScientificObservation> dx, dv;
  for (double x : assessment ? std::vector{-.75, .75, 1.75}
                             : std::vector{-2., -1., 0., 1., 2.})
    for (double v :
         assessment ? std::vector{-.5, .5} : std::vector{-1., 0., 1.}) {
      dx.push_back({{0, x, v}, v});
      dv.push_back({{0, x, v}, -.25 * v - 2 * x});
    }
  return {3,
          {expression("x2", 5), expression("((-1)*x3*x2)-x4*x1", 5)},
          {{0, 1, 1, dx}, {1, 2, 1, dv}},
          {{1, 0, 4}, {1, 0, 4}}};
}
ScientificEquationSystem heat(bool assessment) {
  std::vector<ScientificObservation> residual, initial, boundary;
  for (double t :
       assessment ? std::vector{.25, .75, 1.25} : std::vector{0., .5, 1.})
    for (double x :
         assessment ? std::vector{-.75, .75} : std::vector{-.5, .5, 1.5}) {
      double u = std::exp(-.25 * t) * std::sin(x);
      residual.push_back({{-.25 * u, -u, u}, 0});
    }
  for (double x : {-.5, .5, 1.5})
    initial.push_back({{0, 0, std::sin(x)}, std::sin(x)});
  boundary.push_back({{0, 0, 0}, 0});
  // Fields supplied explicitly: x0=u_t, x1=u_xx, x2=u. x3=kappa.
  return {3,
          {expression("x0-x3*x1", 4), expression("x2", 4)},
          {{0, 1, 1, residual}, {1, 1, 1, initial}, {1, 1, 1, boundary}},
          {{1, 0, 2}}};
}
std::vector<ComplexObservation> complexRows(bool held) {
  std::vector<ComplexObservation> rows;
  for (double x :
       held ? std::vector{-.75, .75, 1.75} : std::vector{-1., -.5, 0., .5, 1.})
    rows.push_back({{{x, 0}, {0, 1}}, {std::cos(x), std::sin(x)}});
  return rows;
}
std::vector<TensorScaleObservation> tensorRows(bool held) {
  std::vector<TensorScaleObservation> rows;
  auto shape = TensorShape(std::array<std::size_t, 3>{2, 2, 2});
  auto a = BoundedTensor(shape, std::array{1., 2., 3., 4., 5., 6., 7., 8.});
  for (double x :
       held ? std::vector{.25, .75, 1.75} : std::vector{.5, 1., 1.5, 2.}) {
    std::vector<double> target;
    for (double v : a.values())
      target.push_back(1.5 * x * v);
    rows.push_back({{x}, a, BoundedTensor(shape, target)});
  }
  return rows;
}
std::vector<RegressionRow> gaussian(bool held) {
  std::vector<RegressionRow> rows;
  for (double x : held ? std::vector{-1.75, -.75, .25, 1.25, 2.25}
                       : std::vector{-2., -1., 0., 1., 2., 3.}) {
    double z = (x - .5) / 1.25;
    rows.push_back(
        {{x}, std::exp(-.5 * z * z) / (1.25 * std::sqrt(2 * std::acos(-1.)))});
  }
  return rows;
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 4) {
      std::cerr << "TASK oscillator|heat|complex|tensor|gaussian SEED "
                   "OUTPUT_DIRECTORY\n";
      return 2;
    }
    std::string task = argv[1];
    if (task != "heat" && task != "oscillator-fit" && task != "oscillator" &&
        task != "complex" && task != "tensor" && task != "gaussian" &&
        task != "gaussian-fit")
      throw std::invalid_argument("unknown benchmark task");
    auto seed = static_cast<std::uint32_t>(std::stoul(argv[2]));
    std::filesystem::path directory = argv[3];
    std::filesystem::create_directories(directory);
    auto start = std::chrono::steady_clock::now();
    auto limits = ExpressionLimits{23, 6, 8, 1000};
    if (task == "heat" || task == "oscillator-fit" || task == "gaussian-fit") {
      auto gaussianSystem = [&](bool held) {
        std::vector<ScientificObservation> rows;
        for (const auto &r : gaussian(held))
          rows.push_back({r.inputs, r.target});
        return ScientificEquationSystem(
            1, {expression("normal_pdf(x0,x1,x2)", 3)}, {{0, 1, 1, rows}},
            {{0, -4, 4}, {1, .125, 4}});
      };
      auto train = task == "gaussian-fit" ? gaussianSystem(false)
                   : task == "heat"       ? heat(false)
                                          : oscillator(false);
      ConstantFitConfig config;
      config.lower = task == "gaussian-fit" ? -4 : 0;
      config.upper = 4;
      config.maxLossAttempts = 512;
      auto fitted = fitScientificParameters(train, config);
      if (!fitted.loss)
        throw std::runtime_error(
            "unscorable initial scientific parameter model");
      auto held = (task == "gaussian-fit" ? gaussianSystem(true)
                   : task == "heat"       ? heat(true)
                                          : oscillator(true))
                      .assess(fitted.parameters);
      CheckpointWriter w;
      w.text(train.identity());
      for (double p : fitted.parameters)
        w.real(p);
      std::ofstream output(directory / "result.tsv");
      output.precision(17);
      output << "task\tseed\ttraining_loss\theld_"
                "loss\tattempts\tseconds\toperations\tfields\tparameters\n"
             << task << '\t' << seed << '\t' << fitted.loss.value() << '\t'
             << *held.loss << '\t' << fitted.work.lossAttempts << '\t'
             << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                              start)
                    .count()
             << '\t' << fitted.supplemental.mathematicalOperations << '\t'
             << fitted.supplemental.fieldEvaluations << '\t';
      for (double p : fitted.parameters)
        output << p << ',';
      output << '\n';
      return 0;
    }
    ScalarMathematicsProblem problem =
        task == "oscillator"
            ? scientificSystemProblem(oscillator(false), 1, limits)
        : task == "complex"
            ? complexRegressionProblem(complexRows(false), limits)
        : task == "tensor"
            ? tensorScaleRegressionProblem(tensorRows(false), limits)
            : scientificScalarRegressionProblem(gaussian(false), limits);
    SymbolicRegressionConfig config;
    config.population = 24;
    config.elites = 2;
    config.maxGenerationAttempts = 20;
    config.maxEvaluations = 504;
    config.maxTrainingLossAttempts = 2048;
    config.grammar.initialMaxDepth = 4;
    config.grammar.mutationMaxDepth = 3;
    config.grammar.constants = {-2, -1, 0, .5, 1, 2};
    config.grammar.operators = {ExpressionKind::Add, ExpressionKind::Subtract,
                                ExpressionKind::Multiply};
    if (task == "complex")
      config.grammar.operators.push_back(ExpressionKind::Exp);
    if (task == "gaussian")
      config.grammar.operators = {ExpressionKind::NormalPdf,
                                  ExpressionKind::Add,
                                  ExpressionKind::Multiply};
    ConstantFitConfig fitting;
    fitting.maxLossAttempts = 32;
    fitting.lower = -8;
    fitting.upper = 8;
    config.fitting = fitting;
    RandomContext random(seed, {2});
    ScalarMathematicsRun run(problem, config, random);
    run.save(directory / "initial.gtkc");
    std::optional<SearchStop> interrupted;
    try {
      while (!run.stoppingReason()) {
        run.step();
        run.save(directory / "latest.gtkc");
      }
    } catch (...) {
      auto error = std::current_exception();
      if (!isEvaluationStop(error))
        throw;
      interrupted = isEvaluationBudget(error) ? SearchStop::EvaluationBudget
                                              : SearchStop::Cancelled;
    }
    auto best = run.population().bestSeen();
    if (!best)
      throw std::runtime_error("no viable scientific model");
    auto selected = best->expression();
    run.save(directory / "final.gtkc");
    // No assessment data is read until the complete selected model is frozen.
    auto assessment =
        task == "oscillator"
            ? scientificSystemProblem(oscillator(true), 1, limits)
        : task == "complex"
            ? complexRegressionProblem(complexRows(true), limits)
        : task == "tensor"
            ? tensorScaleRegressionProblem(tensorRows(true), limits)
            : scientificScalarRegressionProblem(gaussian(true), limits);
    auto held = assessment.problem()->measure(selected);
    std::ofstream output(directory / "result.tsv");
    output.precision(17);
    output << "task\tseed\ttraining_loss\theld_"
              "loss\tgeneration\tattempts\tfitting_"
              "attempts\trows\toperations\tfields\tseconds\tstop\tnodes\tdepth"
              "\tcanonical\tpretty\n";
    auto text = task == "complex"
                    ? ComplexExpression::fromScalarGenome(selected).format()
                    : selected.toString();
    auto pretty = task == "complex"
                      ? ComplexExpression::fromScalarGenome(selected).format(
                            ExpressionTextFormat::Unicode)
                      : formatExpression(selected);
    output << task << '\t' << seed << '\t' << best->fitness() << '\t'
           << (held.loss.state() == FitnessState::Valid ? held.loss.score()
                                                        : -1)
           << '\t' << run.population().generation().value_or(0) << '\t'
           << run.work().lossAttempts << '\t' << run.work().fittingLossAttempts
           << '\t' << run.work().rowAttempts << '\t'
           << run.supplementalWork().mathematicalOperations << '\t'
           << run.supplementalWork().fieldEvaluations << '\t'
           << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                            start)
                  .count()
           << '\t'
           << searchStopName(interrupted.value_or(
                  run.stoppingReason().value_or(SearchStop::GenerationLimit)))
           << '\t' << selected.nodeCount() << '\t' << selected.depth() << '\t'
           << text << '\t' << pretty << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::function<void(const std::exception &, unsigned)> report =
        [&](const std::exception &error, unsigned depth) {
          std::cerr << std::string(depth, ' ') << error.what() << '\n';
          if (auto failure =
                  dynamic_cast<const ScientificInitializationFailure *>(
                      &error)) {
            std::cerr << "attempts=" << failure->work().lossAttempts
                      << " fitting_attempts="
                      << failure->work().fittingLossAttempts
                      << " rows=" << failure->work().rowAttempts
                      << " operations="
                      << failure->supplementalWork().mathematicalOperations
                      << " fields="
                      << failure->supplementalWork().fieldEvaluations
                      << " candidate_requests=" << failure->candidateAttempts()
                      << '\n';
          }
          if (depth < 16) {
            try {
              std::rethrow_if_nested(error);
            } catch (const std::exception &nested) {
              report(nested, depth + 1);
            } catch (...) {
              std::cerr << "nonstandard nested failure\n";
            }
          }
        };
    report(e, 0);
    return 1;
  }
}
