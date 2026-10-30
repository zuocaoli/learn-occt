#pragma once
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace geometry {
using ParameterValue = std::variant<double, int, bool, std::string>;
using ParameterValues = std::map<std::string, ParameterValue>;
struct ParameterSpec {
    std::string key;
    std::string label;
    ParameterValue default_value;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::vector<std::string> choices;
    std::string description;
};
struct ShapeItem {
    std::string name;
    TopoDS_Shape shape;
};
enum class PrimitiveKind { Point, Segment, Vector };
struct Primitive {
    std::string name;
    PrimitiveKind kind;
    gp_Pnt origin;
    gp_Vec vector;
};
enum class Severity { Info, Warning, Error };
struct Diagnostic {
    Severity severity = Severity::Info;
    std::string message;
    TopoDS_Shape shape;
};
struct StepResult {
    std::string name;
    std::vector<ShapeItem> shapes;
    std::vector<Primitive> primitives;
    std::map<std::string, double> measurements;
    std::vector<Diagnostic> diagnostics;
};
enum class RunStatus { Running, Succeeded, Failed };
struct RunContext {
    RunStatus status = RunStatus::Running;
    std::vector<StepResult> steps;
    std::vector<std::string> logs;
    std::string error;
    // Published shapes are read-only; copy before an in-place repair.
    void publish(StepResult step);
    void log(std::string message);
};
struct Experiment {
    std::string id;
    std::string title;
    std::vector<ParameterSpec> parameters;
    std::function<void(const ParameterValues &, RunContext &)> run;
    std::optional<std::filesystem::path> input_path = std::nullopt;
};

std::vector<Experiment> make_experiments();
ParameterValues defaults(const Experiment &experiment);
void set_parameter(const Experiment &experiment, ParameterValues &values, const std::string &key,
                   const std::string &text);
void validate(const Experiment &experiment, const ParameterValues &values);
std::string value_text(const ParameterValue &value);
RunContext execute(const Experiment &experiment, const ParameterValues &values);
std::string status_text(RunStatus status);
std::string occt_version();
void initialize_runtime();
} // namespace geometry
