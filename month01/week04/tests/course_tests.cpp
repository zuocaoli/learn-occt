import std;
#include "tests/checks.hpp"
#include "experiments/course.hpp"
#include "framework/inspect.hpp"
#include "framework/artifacts.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_State.hxx>

int main() {
    try {
        using checks::near;
        using checks::require;
        const auto experiment = geometry::make_course_experiment();
        auto values = geometry::defaults(experiment);
        auto run = checks::run(experiment, values);
        require(run.steps.size() == 4, "Four observation stages");
        const auto &plane = run.steps[0].measurements;
        near(plane.at("area"), 1200 - 25 * std::numbers::pi, "Analytic perforated area", 1e-6);
        near(plane.at("wires"), 2, "Two boundary loops");
        near(plane.at("outer_wires"), 1, "One outer loop");
        near(plane.at("inner_wires"), 1, "One hole");
        near(plane.at("hole.state"), TopAbs_OUT, "Center excluded by actual trimmed face");
        near(plane.at("material.state"), TopAbs_IN, "Material included");
        near(plane.at("outside.state"), TopAbs_OUT, "Point outside rectangle excluded");
        near(plane.at("within_tolerance"), 1, "Sampled representations agree");
        require(plane.at("samples") >= 25 && plane.at("max_error") < 1e-9,
                "At least five samples per use");
        near(run.steps[1].measurements.at("support_shared"), 1, "Reversed face shares support");
        near(run.steps[1].measurements.at("oriented_normal_dot"), -1, "Reversed usage normal");
        require(geometry::inspect(run.steps[0].shapes[0].shape).valid &&
                    geometry::inspect(run.steps[1].shapes[0].shape).valid,
                "Standalone face orientation does not invalidate support");
        const auto &seam = run.steps[2].measurements;
        near(seam.at("seam.unique_edges"), 1, "One unique seam");
        near(seam.at("seam.uses"), 2, "Seam used twice");
        near(seam.at("seam.distinct_faces"), 1, "Only one distinct face");
        near(seam.at("seam.u_difference"), 2 * std::numbers::pi,
             "Two UV representations differ by a period");
        near(seam.at("within_tolerance"), 1, "Cylinder 3D/UV consistency");
        near(seam.at("area"), 400 * std::numbers::pi, "Cylindrical side area", 1e-6);
        near(run.steps[3].measurements.at("valid"), 0, "Deliberate disconnected wire invalid");
        require(std::any_of(run.steps[3].diagnostics.begin(), run.steps[3].diagnostics.end(),
                            [](const auto &d) {
                                return d.severity == geometry::Severity::Error && !d.shape.IsNull();
                            }),
                "Invalid object is locatable");
        geometry::set_parameter(experiment, values, "radius", "7");
        geometry::set_parameter(experiment, values, "translate_x", "25");
        run = checks::run(experiment, values);
        near(run.steps[0].measurements.at("area"), 1200 - 49 * std::numbers::pi,
             "Changed hole radius", 1e-6);
        near(run.steps[0].measurements.at("within_tolerance"), 1,
             "Both representations correctly located");
        auto path = std::filesystem::temp_directory_path() /
                    std::format("geometry-week04-{}.brep",
                                std::chrono::steady_clock::now().time_since_epoch().count());
        geometry::write_brep(run.steps[0].shapes[0].shape, path);
        const auto loaded = geometry::read_brep(path);
        std::filesystem::remove(path);
        GProp_GProps area;
        BRepGProp::SurfaceProperties(loaded, area);
        near(area.Mass(), 1200 - 49 * std::numbers::pi, "Face BREP area roundtrip", 1e-6);
        geometry::set_parameter(experiment, values, "radius", "16");
        require(geometry::execute(experiment, values).status == geometry::RunStatus::Failed,
                "Reject hole crossing rectangle");
        std::cout << "week04 course checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
