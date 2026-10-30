import std;
#include "framework/artifacts.hpp"
#include "framework/inspect.hpp"
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <Standard_Failure.hxx>

namespace geometry {
namespace {
std::string shell_quote(const std::string &text) {
    std::string quoted = "'";
    for (char character : text)
        quoted += character == '\'' ? "'\\''" : std::string(1, character);
    return quoted + "'";
}
void write_text(const std::filesystem::path &path, const std::string &text) {
    std::ofstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(path);
    file << text;
    file.close();
}
} // namespace
void write_brep(const TopoDS_Shape &shape, const std::filesystem::path &path) {
    if (shape.IsNull() || !BRepTools::Write(shape, path.c_str()))
        throw std::runtime_error("Cannot write BREP: " + path.string());
}
TopoDS_Shape read_brep(const std::filesystem::path &path) {
    TopoDS_Shape shape;
    BRep_Builder builder;
    if (!BRepTools::Read(shape, path.c_str(), builder) || shape.IsNull())
        throw std::runtime_error("Cannot read BREP: " + path.string());
    return shape;
}
Experiment brep_experiment(const std::filesystem::path &path) {
    const auto input = std::filesystem::absolute(path);
    return {"loaded-brep",
            "读取 BREP",
            {},
            [input](const auto &, auto &context) {
                auto shape = read_brep(input);
                context.log("Input: " + input.string());
                context.publish(
                    {"读取模型", {{input.filename().string(), shape}}, {}, {}, check_shape(shape)});
            },
            input};
}
std::string report(const Experiment &experiment, const ParameterValues &parameters,
                   const RunContext &run) {
    std::ostringstream out;
    out << std::setprecision(17) << "Experiment: " << experiment.id << "\nOCCT: " << occt_version()
        << "\nStatus: " << status_text(run.status) << "\n";
    if (!run.error.empty())
        out << "Error: " << run.error << '\n';
    out << "Reproduce (from this week directory):\n./run.sh --headless";
    if (experiment.input_path)
        out << " --input " << shell_quote(experiment.input_path->string());
    else
        out << " --experiment " << shell_quote(experiment.id);
    for (const auto &[key, value] : parameters)
        out << " --param " << shell_quote(key + "=" + value_text(value));
    out << '\n';
    for (std::size_t s = 0; s < run.steps.size(); ++s) {
        const auto &step = run.steps[s];
        out << "\nStep " << s << ": " << step.name << '\n';
        for (std::size_t i = 0; i < step.shapes.size(); ++i) {
            const auto &item = step.shapes[i];
            out << "Object " << i << ": " << item.name << " [step-" << s << "-object-" << i
                << ".brep]\n";
            try {
                out << describe(item.shape) << '\n';
            } catch (const Standard_Failure &) {
                out << "Inspection failed; see saved BREP\n";
            }
        }
        for (const auto &[key, value] : step.measurements)
            out << key << '=' << value << '\n';
        for (const auto &primitive : step.primitives)
            out << "Primitive: " << primitive.name << " kind=" << static_cast<int>(primitive.kind)
                << " origin=" << primitive.origin.X() << ',' << primitive.origin.Y() << ','
                << primitive.origin.Z() << " vector=" << primitive.vector.X() << ','
                << primitive.vector.Y() << ',' << primitive.vector.Z() << '\n';
        for (const auto &diagnostic : step.diagnostics)
            out << "Diagnostic [" << static_cast<int>(diagnostic.severity)
                << "]: " << diagnostic.message << '\n';
    }
    for (const auto &log : run.logs)
        out << "Log: " << log << '\n';
    return out.str();
}
void export_run(const Experiment &experiment, const ParameterValues &parameters,
                const RunContext &run, const std::filesystem::path &directory) {
    // Do not silently mix two runs or overwrite an earlier failure case.
    if (std::filesystem::exists(directory) && !std::filesystem::is_empty(directory))
        throw std::runtime_error("Output directory must be empty: " + directory.string());
    std::filesystem::create_directories(directory);
    write_text(directory / "report.txt", report(experiment, parameters, run));
    std::string parameter_text;
    for (const auto &[key, value] : parameters)
        parameter_text += key + "=" + value_text(value) + "\n";
    write_text(directory / "parameters.txt", parameter_text);
    for (std::size_t s = 0; s < run.steps.size(); ++s) {
        const auto &step = run.steps[s];
        for (std::size_t i = 0; i < step.shapes.size(); ++i) {
            const auto &shape = step.shapes[i].shape;
            const auto stem = "step-" + std::to_string(s) + "-object-" + std::to_string(i);
            if (!shape.IsNull())
                write_brep(shape, directory / (stem + ".brep"));
        }
        for (std::size_t i = 0; i < step.diagnostics.size(); ++i) {
            const auto &shape = step.diagnostics[i].shape;
            if (!shape.IsNull())
                write_brep(shape, directory / ("step-" + std::to_string(s) + "-diagnostic-" +
                                               std::to_string(i) + ".brep"));
        }
    }
}
} // namespace geometry
