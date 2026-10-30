import std;
#include "tests/checks.hpp"
#include "experiments/course.hpp"
#include "framework/inspect.hpp"
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>

int main() {
    try {
        using checks::near;
        using checks::require;
        const auto experiment = geometry::make_course_experiment();
        auto values = geometry::defaults(experiment);
        auto run = checks::run(experiment, values);
        require(run.steps.size() == 5, "Five independently observable stages");
        near(run.steps[0].measurements.at("curve.type"), GeomAbs_Line, "Line identification");
        near(run.steps[0].measurements.at("point.x"), 5, "Line sampling");
        near(run.steps[0].measurements.at("edge.last"), 20, "Finite edge range");
        const auto &circle = run.steps[1].measurements;
        near(circle.at("curve.type"), GeomAbs_Circle, "Circle identification");
        near(circle.at("point.x"), 0, "Circle x");
        near(circle.at("point.y"), 10, "Circle y");
        near(circle.at("speed"), 10, "D1 is not a unit tangent");
        near(circle.at("tangent.x"), -1, "Unit tangent x");
        near(circle.at("tangent.y"), 0, "Unit tangent y");
        near(circle.at("closure_error"), 0, "Period closes");
        near(run.steps[2].measurements.at("surface.type"), GeomAbs_Plane, "Plane identification");
        near(run.steps[2].measurements.at("normal.z"), 1, "Plane geometric normal");
        const auto &cylinder = run.steps[3].measurements;
        near(cylinder.at("surface.type"), GeomAbs_Cylinder, "Cylinder identification");
        near(cylinder.at("point.x"), 0, "Cylinder x");
        near(cylinder.at("point.y"), 10, "Cylinder y");
        near(cylinder.at("point.z"), 5, "Cylinder axial coordinate");
        near(cylinder.at("normal.x"), 0, "Cylinder normal x");
        near(cylinder.at("normal.y"), 1, "Cylinder normal y");
        near(cylinder.at("period_error"), 0, "Cylinder periodicity");
        near(run.steps[4].measurements.at("raw.x"), 0, "Raw curve remains local");
        near(run.steps[4].measurements.at("world.x"), 25, "Location applied once");
        near(run.steps[4].measurements.at("world.y"), 10, "Location world y");
        near(run.steps[4].measurements.at("adaptor_error"), 0, "Independent adaptor agrees");
        for (const auto &step : run.steps)
            for (const auto &item : step.shapes)
                require(geometry::inspect(item.shape).valid, "Sample carrier remains valid");
        geometry::set_parameter(experiment, values, "radius", "20");
        geometry::set_parameter(experiment, values, "angle_deg", "180");
        run = checks::run(experiment, values);
        near(run.steps[1].measurements.at("point.x"), -20, "Changed radius and angle");
        near(run.steps[1].measurements.at("speed"), 20, "Parameter is not arc length");
        near(run.steps[3].measurements.at("normal.x"), -1, "Changed cylinder normal");
        geometry::set_parameter(experiment, values, "height", "1");
        auto failed = geometry::execute(experiment, values);
        require(failed.status == geometry::RunStatus::Failed &&
                    failed.error.find("axial") != std::string::npos,
                "Reject out-of-face axial sample");
        std::cout << "week02 course checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
