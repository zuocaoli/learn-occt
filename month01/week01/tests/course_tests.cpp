import std;
#include "tests/checks.hpp"
#include "experiments/course.hpp"
#include "framework/artifacts.hpp"
#include "framework/inspect.hpp"
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <BRep_Tool.hxx>

int main() {
    try {
        using checks::near;
        using checks::require;
        const auto experiment = geometry::make_course_experiment();
        auto parameters = geometry::defaults(experiment);
        auto run = checks::run(experiment, parameters);
        require(run.steps.size() == 5, "Two branches retain intermediate stages");
        near(run.steps[2].measurements.at("point.x"), 0, "R*T x");
        near(run.steps[2].measurements.at("point.y"), 90, "R*T y");
        near(run.steps[4].measurements.at("point.x"), 50, "T*R x");
        near(run.steps[4].measurements.at("point.y"), 40, "T*R y");
        near(run.steps[0].measurements.at("local_point.x"), 5, "Local frame translation");
        near(run.steps[0].measurements.at("local_point.y"), 12, "Local frame rotation");
        near(run.steps[0].measurements.at("local_vector.x"), 0, "Local vector x");
        near(run.steps[0].measurements.at("local_vector.y"), 1, "Vector has no origin offset");
        near(run.steps[1].measurements.at("vector.x"), 1, "Translation does not change vector");
        for (const auto &step : run.steps) {
            const auto &shape = step.shapes[0].shape;
            const auto stats = geometry::inspect(shape);
            require(stats.valid && stats.vertices == 8 && stats.edges == 12 && stats.faces == 6,
                    "Rigid topology");
            near(stats.volume, 24000, "Rigid volume", 1e-6);
            const gp_Pnt point(step.measurements.at("point.x"), step.measurements.at("point.y"),
                               step.measurements.at("point.z"));
            double nearest = std::numeric_limits<double>::max();
            for (TopExp_Explorer vertices(shape, TopAbs_VERTEX); vertices.More(); vertices.Next())
                nearest = std::min(
                    nearest, BRep_Tool::Pnt(TopoDS::Vertex(vertices.Current())).Distance(point));
            near(nearest, 0, "Reported point agrees with actual BREP vertex");
        }
        auto path = std::filesystem::temp_directory_path() /
                    std::format("geometry-week01-{}.brep",
                                std::chrono::steady_clock::now().time_since_epoch().count());
        geometry::write_brep(run.steps[2].shapes[0].shape, path);
        const auto loaded = geometry::read_brep(path);
        std::filesystem::remove(path);
        near(geometry::inspect(loaded).volume, 24000, "BREP volume roundtrip", 1e-6);
        double nearest = 1e100;
        for (TopExp_Explorer v(loaded, TopAbs_VERTEX); v.More(); v.Next())
            nearest = std::min(
                nearest, BRep_Tool::Pnt(TopoDS::Vertex(v.Current())).Distance(gp_Pnt(0, 90, 0)));
        near(nearest, 0, "BREP placement roundtrip");
        geometry::set_parameter(experiment, parameters, "angle_deg", "0");
        run = checks::run(experiment, parameters);
        near(run.steps[2].measurements.at("point.x"), 90, "Zero angle R*T");
        near(run.steps[4].measurements.at("point.x"), 90, "Zero angle T*R");
        std::cout << "week01 course checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
