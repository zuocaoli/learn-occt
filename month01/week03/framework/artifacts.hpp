#pragma once
#include "framework/experiment.hpp"

namespace geometry {
void write_brep(const TopoDS_Shape &shape, const std::filesystem::path &path);
TopoDS_Shape read_brep(const std::filesystem::path &path);
Experiment brep_experiment(const std::filesystem::path &path);
std::string report(const Experiment &experiment, const ParameterValues &parameters,
                   const RunContext &run);
void export_run(const Experiment &experiment, const ParameterValues &parameters,
                const RunContext &run, const std::filesystem::path &directory);
} // namespace geometry
