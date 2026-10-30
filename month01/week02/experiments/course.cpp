import std;
#include "experiments/course.hpp"
#include "framework/inspect.hpp"
#include <Geom_Line.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Plane.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <Precision.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>

namespace geometry {
namespace {
void point_metrics(StepResult &step, const std::string &name, const gp_Pnt &point) {
    step.measurements[name + ".x"] = point.X();
    step.measurements[name + ".y"] = point.Y();
    step.measurements[name + ".z"] = point.Z();
}
gp_Vec unit(const gp_Vec &vector) {
    if (vector.Magnitude() <= 1e-12)
        throw std::runtime_error("Degenerate derivative: direction is undefined");
    return vector.Normalized();
}
void sample_geometry(const ParameterValues &values, RunContext &run) {
    const double radius = std::get<double>(values.at("radius"));
    const double height = std::get<double>(values.at("height"));
    const double angle = std::get<double>(values.at("angle_deg")) * std::numbers::pi / 180.0;
    const double axial = std::get<double>(values.at("axial"));
    const int samples = std::get<int>(values.at("samples"));
    if (axial > height)
        throw std::invalid_argument("axial must lie in the finite face interval [0,height]");
    const double tau = 2 * std::numbers::pi;
    occ::handle<Geom_Line> line = new Geom_Line(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0));
    auto line_edge = BRepBuilderAPI_MakeEdge(line, 0, 20).Edge();
    auto line_sample = line->EvalD1(5);
    StepResult line_step{
        "01 直线与有限 Edge",
        {{"line edge [0,20]", line_edge}},
        {{"t=5", PrimitiveKind::Point, line_sample.Point, {}},
         {"tangent", PrimitiveKind::Vector, line_sample.Point, unit(line_sample.D1) * 6}},
        {{"curve.type", static_cast<double>(BRepAdaptor_Curve(line_edge).GetType())},
         {"edge.first", BRepAdaptor_Curve(line_edge).FirstParameter()},
         {"edge.last", BRepAdaptor_Curve(line_edge).LastParameter()}},
        check_shape(line_edge)};
    point_metrics(line_step, "point", line_sample.Point);
    run.publish(std::move(line_step));

    occ::handle<Geom_Circle> circle =
        new Geom_Circle(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)), radius);
    auto circle_edge = BRepBuilderAPI_MakeEdge(circle, 0, tau).Edge();
    const auto circle_sample = circle->EvalD1(angle);
    const auto tangent = unit(circle_sample.D1);
    StepResult circle_step{
        "02 圆：参数、导数与周期",
        {{"circle", circle_edge}},
        {{"selected tangent", PrimitiveKind::Vector, circle_sample.Point, tangent * 6}},
        {{"curve.type", static_cast<double>(BRepAdaptor_Curve(circle_edge).GetType())},
         {"speed", circle_sample.D1.Magnitude()},
         {"tangent.x", tangent.X()},
         {"tangent.y", tangent.Y()},
         {"tangent.z", tangent.Z()},
         {"closure_error", circle->Value(0).Distance(circle->Value(tau))},
         {"parameter", angle}},
        check_shape(circle_edge)};
    point_metrics(circle_step, "point", circle_sample.Point);
    for (int i = 0; i <= samples; ++i) {
        double t = tau * i / samples;
        const auto sample = circle->EvalD1(t);
        circle_step.primitives.push_back(
            {std::format("t={:.6g}", t), PrimitiveKind::Point, sample.Point, {}});
        run.log(std::format("circle t={:.17g} P=({:.17g},{:.17g},{:.17g}) |D1|={:.17g}", t,
                            sample.Point.X(), sample.Point.Y(), sample.Point.Z(),
                            sample.D1.Magnitude()));
    }
    run.publish(std::move(circle_step));

    occ::handle<Geom_Plane> plane = new Geom_Plane(gp_Ax3());
    auto plane_face = BRepBuilderAPI_MakeFace(plane, 0, 40, 0, 30, Precision::Confusion()).Face();
    const auto plane_sample = plane->EvalD1(10, 5);
    const auto plane_normal = unit(plane_sample.D1U.Crossed(plane_sample.D1V));
    StepResult plane_step{
        "03 平面与有限 Face",
        {{"plane face", plane_face}},
        {{"(u,v)=(10,5)", PrimitiveKind::Point, plane_sample.Point, {}},
         {"geometric normal", PrimitiveKind::Vector, plane_sample.Point, plane_normal * 6}},
        {{"surface.type", static_cast<double>(BRepAdaptor_Surface(plane_face).GetType())},
         {"normal.z", plane_normal.Z()}},
        check_shape(plane_face)};
    point_metrics(plane_step, "point", plane_sample.Point);
    run.publish(std::move(plane_step));

    occ::handle<Geom_CylindricalSurface> cylinder = new Geom_CylindricalSurface(gp_Ax3(), radius);
    auto cylinder_face =
        BRepBuilderAPI_MakeFace(cylinder, 0, tau, 0, height, Precision::Confusion()).Face();
    const auto cylinder_sample = cylinder->EvalD1(angle, axial);
    const auto normal = unit(cylinder_sample.D1U.Crossed(cylinder_sample.D1V));
    StepResult cylinder_step{
        "04 圆柱面：角参数与轴向参数",
        {{"finite cylindrical face", cylinder_face}},
        {{"selected UV point", PrimitiveKind::Point, cylinder_sample.Point, {}},
         {"geometric normal", PrimitiveKind::Vector, cylinder_sample.Point, normal * 6}},
        {{"surface.type", static_cast<double>(BRepAdaptor_Surface(cylinder_face).GetType())},
         {"u", angle},
         {"v", axial},
         {"normal.x", normal.X()},
         {"normal.y", normal.Y()},
         {"normal.z", normal.Z()},
         {"period", cylinder->UPeriod()},
         {"period_error",
          cylinder->Value(angle, axial).Distance(cylinder->Value(angle + tau, axial))}},
        check_shape(cylinder_face)};
    point_metrics(cylinder_step, "point", cylinder_sample.Point);
    run.publish(std::move(cylinder_step));

    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(std::get<double>(values.at("translate_x")), 0, 0));
    const auto located = TopoDS::Edge(circle_edge.Moved(TopLoc_Location(translation)));
    TopLoc_Location location;
    double first = 0, last = 0;
    const auto raw_curve = BRep_Tool::Curve(located, location, first, last);
    if (raw_curve.IsNull())
        throw std::runtime_error("No 3D curve");
    const auto raw = raw_curve->EvalD1(angle);
    const auto world = raw.Point.Transformed(location.Transformation());
    const auto world_tangent = unit(raw.D1.Transformed(location.Transformation()));
    StepResult located_step{
        "05 Location：局部曲线到世界坐标",
        {{"located circle", located}},
        {{"raw point", PrimitiveKind::Point, raw.Point, {}},
         {"world point", PrimitiveKind::Point, world, {}},
         {"world tangent", PrimitiveKind::Vector, world, world_tangent * 6}},
        {{"adaptor_error", world.Distance(BRepAdaptor_Curve(located).Value(angle))},
         {"tangent.x", world_tangent.X()},
         {"tangent.y", world_tangent.Y()},
         {"edge.first", first},
         {"edge.last", last}},
        check_shape(located)};
    point_metrics(located_step, "raw", raw.Point);
    point_metrics(located_step, "world", world);
    run.publish(std::move(located_step));
    run.log("Underlying Geom_Line/Geom_Plane and cylinder axial direction are unbounded; Edge/Face "
            "ranges are finite.");
    run.log("Curve radius changes D1 magnitude, not the angle parameter. UV bounds alone do not "
            "describe holes.");
    run.log("This week computes geometric normals only; face orientation is studied in week04.");
}
} // namespace
Experiment make_course_experiment() {
    return {"geometry-sampling",
            "第二周：几何参数与采样",
            {{"radius", "圆/圆柱半径 / mm", 10.0, 0.001, 1000.0, {}, "X 参考方向为 +X，轴为 +Z"},
             {"height", "圆柱面高度 / mm", 20.0, 0.001, 1000.0, {}, "有限 Face 的轴向上界"},
             {"angle_deg", "角参数 / °", 90.0, 0.0, 360.0, {}, "转为弧度后采样"},
             {"axial", "轴向 v / mm", 5.0, 0.0, 1000.0, {}, "必须在 [0,height] 内"},
             {"samples", "圆周分段数", 8, 2, 64, {}, "包括周期两端点"},
             {"translate_x",
              "Location X / mm",
              25.0,
              -1000.0,
              1000.0,
              {},
              "只在 Location 实验应用一次"}},
            sample_geometry};
}
} // namespace geometry
