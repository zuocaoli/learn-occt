import std;
#include "experiments/course.hpp"
#include "experiments/fixtures.hpp"
#include "framework/inspect.hpp"
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <BRepGProp.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <Geom2d_Curve.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <Precision.hxx>
#include <gp_Trsf.hxx>

namespace geometry {
namespace {
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;
StepResult examine(const std::string &title, const TopoDS_Face &face, int samples,
                   RunContext &run) {
    StepResult step{title, {{"face", face}}, {}, {}, check_shape(face)};
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    step.measurements["area"] = std::abs(properties.Mass());
    step.measurements["face_tolerance"] = BRep_Tool::Tolerance(face);
    const auto outer = BRepTools::OuterWire(face);
    TopLoc_Location surface_location;
    const auto surface = BRep_Tool::Surface(face, surface_location);
    ShapeMap unique_edges, seams;
    std::map<int, std::vector<double>> seam_u;
    int wire_count = 0, outer_count = 0, use_count = 0, seam_uses = 0, total_samples = 0;
    double max_error = 0;
    bool all_within = true;
    for (TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) {
        const auto wire = TopoDS::Wire(w.Current());
        ++wire_count;
        const bool is_outer = wire.IsSame(outer);
        if (is_outer)
            ++outer_count;
        run.log(std::format("wire {} outer={} orientation={}", wire_count, is_outer,
                            static_cast<int>(wire.Orientation())));
        for (BRepTools_WireExplorer e(wire, face); e.More(); e.Next()) {
            const auto edge = e.Current();
            ++use_count;
            unique_edges.Add(edge);
            TopLoc_Location curve_location;
            double c_first = 0, c_last = 0, p_first = 0, p_last = 0;
            const auto curve = BRep_Tool::Curve(edge, curve_location, c_first, c_last);
            bool stored = false;
            // Edge use orientation selects one of the two pcurves on a seam.
            const auto pcurve = BRep_Tool::CurveOnSurface(edge, face, p_first, p_last, &stored);
            if (curve.IsNull() || pcurve.IsNull() || !BRep_Tool::SameParameter(edge))
                throw std::runtime_error(
                    "Fixture edge lacks comparable 3D/pcurve SameParameter representations");
            const double first = std::max(c_first, p_first), last = std::min(c_last, p_last);
            if (!std::isfinite(first) || !std::isfinite(last) || first >= last)
                throw std::runtime_error("No finite common parameter interval");
            double error = 0;
            bool within = true;
            const double tolerance = BRep_Tool::Tolerance(edge) + BRep_Tool::Tolerance(face);
            const bool seam = BRep_Tool::IsClosed(edge, face);
            for (int i = 0; i < samples; ++i) {
                const double t = first + (last - first) * (i + 0.5) / samples;
                const auto uv = pcurve->Value(t);
                const auto p3 = curve->Value(t).Transformed(curve_location.Transformation());
                const auto ps =
                    surface->Value(uv.X(), uv.Y()).Transformed(surface_location.Transformation());
                const double residual = p3.Distance(ps);
                const double roundoff =
                    64 * std::numeric_limits<double>::epsilon() *
                    std::max({1.0, p3.Distance(gp_Pnt(0, 0, 0)), ps.Distance(gp_Pnt(0, 0, 0))});
                error = std::max(error, residual);
                within = within && residual <= tolerance + roundoff;
                ++total_samples;
                step.primitives.push_back(
                    {std::format("use{} t={:.5g}", use_count, t), PrimitiveKind::Point, p3, {}});
                run.log(std::format(
                    "use {} t={:.17g} UV=({:.17g},{:.17g}) C3=({:.17g},{:.17g},{:.17g}) "
                    "S=({:.17g},{:.17g},{:.17g}) residual={:.17g}",
                    use_count, t, uv.X(), uv.Y(), p3.X(), p3.Y(), p3.Z(), ps.X(), ps.Y(), ps.Z(),
                    residual));
            }
            if (seam) {
                ++seam_uses;
                const int id = seams.Add(edge);
                seam_u[id].push_back(pcurve->Value((first + last) / 2).X());
            }
            max_error = std::max(max_error, error);
            all_within = all_within && within;
            step.measurements[std::format("use.{}.max_error", use_count)] = error;
            step.measurements[std::format("use.{}.tolerance", use_count)] = tolerance;
            const auto message = std::format("use {}: orientation={}, seam={}, pcurve stored={}, "
                                             "max error={:.6g}, tolerance={:.6g}",
                                             use_count, static_cast<int>(edge.Orientation()), seam,
                                             stored, error, tolerance);
            step.diagnostics.push_back({within ? Severity::Info : Severity::Error, message, edge});
            run.log(message);
        }
    }
    step.measurements.insert({{"wires", wire_count},
                              {"outer_wires", outer_count},
                              {"inner_wires", wire_count - outer_count},
                              {"unique_edges", unique_edges.Extent()},
                              {"edge_uses", use_count},
                              {"samples", total_samples},
                              {"max_error", max_error},
                              {"within_tolerance", all_within},
                              {"seam.unique_edges", seams.Extent()},
                              {"seam.uses", seam_uses}});
    if (seams.Extent() > 0) {
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                   TopTools_ShapeMapHasher>
            owners;
        TopExp::MapShapesAndUniqueAncestors(face, TopAbs_EDGE, TopAbs_FACE, owners);
        step.measurements["seam.distinct_faces"] = owners.FindFromKey(seams(1)).Size();
        const auto &us = seam_u.at(1);
        const auto [low, high] = std::minmax_element(us.begin(), us.end());
        step.measurements["seam.u_difference"] = *high - *low;
        for (double u : us)
            run.log(std::format("seam pcurve midpoint u={:.17g}", u));
    }
    return step;
}
void boundaries(const ParameterValues &values, RunContext &run) {
    const auto radius = std::get<double>(values.at("radius"));
    const int samples = std::get<int>(values.at("samples"));
    auto face = fixtures::perforated_face(radius);
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(std::get<double>(values.at("translate_x")), 0, 0));
    face = TopoDS::Face(face.Moved(TopLoc_Location(translation)));
    auto plane_step = examine("01 给定带孔面：边界与三维/UV 对照", face, samples, run);
    plane_step.measurements["hole.state"] =
        BRepClass_FaceClassifier(face, gp_Pnt2d(20, 15), Precision::Confusion()).State();
    plane_step.measurements["material.state"] =
        BRepClass_FaceClassifier(face, gp_Pnt2d(5, 5), Precision::Confusion()).State();
    plane_step.measurements["outside.state"] =
        BRepClass_FaceClassifier(face, gp_Pnt2d(45, 5), Precision::Confusion()).State();
    run.publish(std::move(plane_step));

    const auto reversed = TopoDS::Face(face.Reversed());
    TopLoc_Location location, reversed_location;
    const auto support = BRep_Tool::Surface(face, location);
    const auto reversed_support = BRep_Tool::Surface(reversed, reversed_location);
    const auto derivatives = support->EvalD1(5, 5);
    auto geometric_normal =
        derivatives.D1U.Crossed(derivatives.D1V).Transformed(location.Transformation());
    if (geometric_normal.Magnitude() <= 1e-12)
        throw std::runtime_error("Degenerate surface normal");
    geometric_normal.Normalize();
    const auto point = derivatives.Point.Transformed(location.Transformation());
    const auto oriented =
        face.Orientation() == TopAbs_REVERSED ? -geometric_normal : geometric_normal;
    StepResult direction{
        "02 反向 Face：支撑曲面保持，使用方向反转",
        {{"reversed face", reversed}},
        {{"geometric normal", PrimitiveKind::Vector, point, geometric_normal * 8},
         {"reversed oriented normal", PrimitiveKind::Vector, point, -oriented * 8}},
        {{"support_shared", support == reversed_support},
         {"oriented_normal_dot", oriented.Dot(-oriented)},
         {"geometric_normal.z", geometric_normal.Z()}},
        check_shape(reversed)};
    run.publish(std::move(direction));

    const auto cylinder = BRepPrimAPI_MakeCylinder(10, 20).Shape();
    TopoDS_Face side;
    for (TopExp_Explorer f(cylinder, TopAbs_FACE); f.More(); f.Next()) {
        auto candidate = TopoDS::Face(f.Current());
        if (BRepAdaptor_Surface(candidate).GetType() == GeomAbs_Cylinder) {
            side = candidate;
            break;
        }
    }
    if (side.IsNull())
        throw std::runtime_error("Cylindrical face missing");
    run.publish(examine("03 圆柱接缝：同一边的两条 pcurve", side, samples, run));

    // Given defect, intentionally not repaired: successful execution may produce invalid topology.
    BRep_Builder builder;
    TopoDS_Wire broken;
    builder.MakeWire(broken);
    builder.Add(broken, BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0)).Edge());
    builder.Add(broken, BRepBuilderAPI_MakeEdge(gp_Pnt(20, 0, 0), gp_Pnt(30, 0, 0)).Edge());
    run.publish({"04 给定缺陷：不连通 Wire 的检查",
                 {{"disconnected wire", broken}},
                 {},
                 {{"valid", inspect(broken).valid}},
                 check_shape(broken)});
    run.log("CurveOnSurface on a plane may compute a pcurve instead of retrieving a stored one.");
    run.log(
        "The seam has two uses and two UV representations, but only one distinct adjacent face.");
    run.log("Residual sampling is evidence, not a proof over the whole parameter interval; "
            "tolerance is not increased.");
    run.log("Last stage is deliberately invalid and remains exportable. No healing, Boolean or "
            "face splitting is performed.");
}
} // namespace
Experiment make_course_experiment() {
    return {
        "face-boundaries",
        "第四周：面边界与 pcurve",
        {{"radius", "给定圆孔半径 / mm", 5.0, 0.001, 30.0, {}, "必须严格小于 15，保持孔在矩形内"},
         {"samples", "每次边使用采样数", 5, 5, 32, {}, "有效区间内点；不证明全域一致"},
         {"translate_x",
          "面 Location X / mm",
          0.0,
          -1000.0,
          1000.0,
          {},
          "三维曲线与支撑曲面各自应用 Location"}},
        boundaries};
}
} // namespace geometry
