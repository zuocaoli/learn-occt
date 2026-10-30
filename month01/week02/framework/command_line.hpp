#pragma once
#include "framework/experiment.hpp"

namespace geometry {
struct CommandLine {
    bool headless = false;
    bool help = false;
    bool list = false;
    std::string experiment = "box-transform";
    std::vector<std::pair<std::string, std::string>> parameters;
    std::optional<std::filesystem::path> output;
    std::optional<std::filesystem::path> input;
};
CommandLine parse_command_line(int argc, char **argv);
const Experiment &find_experiment(const std::vector<Experiment> &experiments,
                                  const std::string &id);
ParameterValues command_parameters(const Experiment &experiment, const CommandLine &command);
int run_console(const CommandLine &command, const std::vector<Experiment> &experiments);
std::string usage();
} // namespace geometry
