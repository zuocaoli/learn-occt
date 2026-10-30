#pragma once
#include "framework/experiment.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
namespace checks {
inline void require(bool ok, const std::string &message) {
    if (!ok)
        throw std::runtime_error(message);
}
inline void near(double value, double expected, const std::string &message,
                 double tolerance = 1e-9) {
    require(std::isfinite(value) && std::abs(value - expected) <= tolerance, message);
}
inline geometry::RunContext run(const geometry::Experiment &experiment,
                                const geometry::ParameterValues &values) {
    auto result = geometry::execute(experiment, values);
    require(result.status == geometry::RunStatus::Succeeded, result.error);
    return result;
}
} // namespace checks
