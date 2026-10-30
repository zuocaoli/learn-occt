import std;
#include "framework/command_line.hpp"
#include "framework/artifacts.hpp"

namespace geometry {
CommandLine parse_command_line(int argc, char **argv) {
    CommandLine command;
    command.experiment = "geometry-sampling";
    bool experiment_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        auto next = [&]() -> std::string {
            if (++i >= argc)
                throw std::invalid_argument("Missing value after " + arg);
            return argv[i];
        };
        if (arg == "--headless")
            command.headless = true;
        else if (arg == "--help")
            command.help = true;
        else if (arg == "--list")
            command.list = true;
        else if (arg == "--experiment") {
            command.experiment = next();
            experiment_set = true;
        } else if (arg == "--output")
            command.output = next();
        else if (arg == "--input")
            command.input = next();
        else if (arg == "--param") {
            auto value = next();
            auto separator = value.find('=');
            if (separator == std::string::npos || separator == 0)
                throw std::invalid_argument("Expected --param key=value");
            command.parameters.emplace_back(value.substr(0, separator),
                                            value.substr(separator + 1));
        } else
            throw std::invalid_argument("Unknown argument: " + arg);
    }
    if (command.input && (!command.parameters.empty() || experiment_set))
        throw std::invalid_argument("--input cannot be combined with experiment parameters");
    return command;
}
const Experiment &find_experiment(const std::vector<Experiment> &experiments,
                                  const std::string &id) {
    auto found = std::find_if(experiments.begin(), experiments.end(),
                              [&](const auto &item) { return item.id == id; });
    if (found == experiments.end())
        throw std::invalid_argument("Unknown experiment: " + id);
    return *found;
}
ParameterValues command_parameters(const Experiment &experiment, const CommandLine &command) {
    auto values = defaults(experiment);
    for (const auto &[key, text] : command.parameters)
        set_parameter(experiment, values, key, text);
    return values;
}
std::string usage() {
    return "Geometry experiments\n"
           "  ./run.sh [--headless] [--experiment ID] [--param key=value] [--output EMPTY_DIR]\n"
           "  ./run.sh [--headless] --input model.brep [--output EMPTY_DIR]\n"
           "  ./run.sh --list | --help\n"
           "Without --headless, open the Qt/OCCT workbench.\n";
}
int run_console(const CommandLine &command, const std::vector<Experiment> &experiments) {
    if (command.help) {
        std::cout << usage();
        return 0;
    }
    if (command.list) {
        for (const auto &experiment : experiments)
            std::cout << experiment.id << " : " << experiment.title << '\n';
        return 0;
    }
    Experiment loaded;
    if (command.input) {
        loaded = brep_experiment(*command.input);
    }
    const auto &experiment =
        command.input ? loaded : find_experiment(experiments, command.experiment);
    auto parameters =
        command.input ? defaults(experiment) : command_parameters(experiment, command);
    auto run = execute(experiment, parameters);
    std::cout << report(experiment, parameters, run);
    if (command.output)
        export_run(experiment, parameters, run, *command.output);
    return run.status == RunStatus::Succeeded ? 0 : 1;
}
} // namespace geometry
