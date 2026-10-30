import std;
#include "framework/artifacts.hpp"
#include "framework/command_line.hpp"
#include "framework/inspect.hpp"
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>

namespace {
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class Function> void rejects(Function function, const std::string &message) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception &) {
        threw = true;
    }
    require(threw, message);
}
struct Scratch {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("geometry-core-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        std::filesystem::create_directories(path);
    }
    ~Scratch() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
} // namespace
int main() {
    try {
        geometry::initialize_runtime();
        auto experiments = geometry::make_experiments();
        const auto &experiment = experiments.at(0);
        auto values = geometry::defaults(experiment);
        geometry::set_parameter(experiment, values, "length", "60");
        auto run = geometry::execute(experiment, values);
        require(run.status == geometry::RunStatus::Succeeded && run.steps.size() == 3,
                "Box stages");
        for (const auto &step : run.steps) {
            auto stats = geometry::inspect(step.shapes.at(0).shape);
            require(stats.vertices == 8 && stats.edges == 12 && stats.faces == 6 &&
                        stats.solids == 1,
                    "Unique topology");
            require(stats.valid && std::abs(stats.volume - 36000.0) < 1e-6,
                    "Rigid transform volume");
        }
        require(!run.steps[0].shapes[0].shape.IsSame(run.steps[1].shapes[0].shape),
                "Preserved input snapshot");
        for (const auto *invalid : {"-1", "0", "nan", "inf", "12x", ""})
            rejects([&] { geometry::set_parameter(experiment, values, "length", invalid); },
                    "Reject invalid dimension");
        rejects([&] { geometry::set_parameter(experiment, values, "unknown", "1"); },
                "Reject unknown key");
        auto bad_type = values;
        bad_type["length"] = 40;
        require(geometry::execute(experiment, bad_type).status == geometry::RunStatus::Failed,
                "Reject wrong type");

        geometry::Experiment typed{"typed",
                                   "Typed",
                                   {{"count", "Count", 3, 1, 10, {}, ""},
                                    {"enabled", "Enabled", true, {}, {}, {}, ""},
                                    {"mode", "Mode", std::string("a"), {}, {}, {"a", "b"}, ""}},
                                   [](const auto &, auto &) {}};
        auto typed_values = geometry::defaults(typed);
        geometry::set_parameter(typed, typed_values, "count", "7");
        geometry::set_parameter(typed, typed_values, "enabled", "false");
        geometry::set_parameter(typed, typed_values, "mode", "b");
        require(std::get<int>(typed_values["count"]) == 7 &&
                    !std::get<bool>(typed_values["enabled"]),
                "Typed values");
        rejects([&] { geometry::set_parameter(typed, typed_values, "mode", "missing"); },
                "Enum validation");
        rejects([&] { geometry::set_parameter(typed, typed_values, "count", "1.5"); },
                "Integer validation");
        rejects([&] { geometry::set_parameter(typed, typed_values, "enabled", "1"); },
                "Boolean validation");

        geometry::Experiment failure{"failure", "Failure", {}, [&](const auto &, auto &context) {
                                         context.publish(run.steps[0]);
                                         throw Standard_Failure("Deliberate OCCT exception");
                                     }};
        auto failed = geometry::execute(failure, {});
        require(failed.status == geometry::RunStatus::Failed && failed.steps.size() == 1 &&
                    !failed.error.empty(),
                "Failure preserves stages");
        failure.run = [](const auto &, auto &) { throw std::runtime_error("standard error"); };
        require(geometry::execute(failure, {}).error == "standard error", "Standard exception");

        BRep_Builder builder;
        TopoDS_Wire wire;
        builder.MakeWire(wire);
        builder.Add(wire, BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(1, 0, 0)).Edge());
        builder.Add(wire, BRepBuilderAPI_MakeEdge(gp_Pnt(3, 0, 0), gp_Pnt(4, 0, 0)).Edge());
        require(!geometry::inspect(wire).valid, "Disconnected wire invalid");
        auto diagnostics = geometry::check_shape(wire);
        require(std::any_of(diagnostics.begin(), diagnostics.end(),
                            [](const auto &d) {
                                return d.severity == geometry::Severity::Error && !d.shape.IsNull();
                            }),
                "Problem shape associated");
        geometry::Experiment invalid_model{
            "invalid-model", "Invalid model", {}, [&](const auto &, auto &context) {
                context.publish({"Invalid wire", {{"wire", wire}}, {}, {}, diagnostics});
            }};
        require(geometry::execute(invalid_model, {}).status == geometry::RunStatus::Succeeded,
                "Invalid model is inspectable output");

        Scratch scratch;
        geometry::export_run(experiment, values, run, scratch.path / "export");
        auto reloaded = geometry::read_brep(scratch.path / "export/step-2-object-0.brep");
        require(std::abs(geometry::inspect(reloaded).volume - 36000.0) < 1e-6, "BREP roundtrip");
        geometry::export_run(failure, {}, failed, scratch.path / "failed");
        require(std::filesystem::exists(scratch.path / "failed/step-0-object-0.brep"),
                "Failure export");
        rejects([&] { geometry::export_run(experiment, values, run, scratch.path / "export"); },
                "Do not overwrite cases");
        rejects([&] { geometry::read_brep(scratch.path / "missing.brep"); }, "Read error");
        rejects([&] { geometry::write_brep(reloaded, scratch.path / "missing/out.brep"); },
                "Write error");
        auto text = geometry::report(experiment, values, run);
        require(text.find("--param 'length=60'") != std::string::npos &&
                    text.find(geometry::occt_version()) != std::string::npos,
                "Replay parameters and version");
        auto loaded_experiment =
            geometry::brep_experiment(scratch.path / "export/step-2-object-0.brep");
        auto loaded_run = geometry::execute(loaded_experiment, {});
        require(loaded_run.status == geometry::RunStatus::Succeeded &&
                    geometry::report(loaded_experiment, {}, loaded_run).find("--input '") !=
                        std::string::npos,
                "Imported BREP replay uses input path");
        std::cout << "Core checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
