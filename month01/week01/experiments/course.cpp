import std;
#include "experiments/course.hpp"
#include "framework/inspect.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>

namespace geometry {
namespace {
void transform_order(const ParameterValues &values, RunContext &run) {
    auto real = [&](const char *key) { return std::get<double>(values.at(key)); };
    const auto box = BRepPrimAPI_MakeBox(real("length"), real("width"), real("height")).Shape();
    const gp_Pnt reference(real("length"), 0, 0);
    const gp_Vec direction(1, 0, 0);
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(real("translate_x"), 0, 0));
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                         real("angle_deg") * std::numbers::pi / 180.0);
    // Physical displacement of the standard frame, not the inverse coordinate conversion.
    gp_Trsf local_to_world;
    local_to_world.SetDisplacement(gp_Ax3(),
                                   gp_Ax3(gp_Pnt(5, 10, 0), gp_Dir(0, 0, 1), gp_Dir(0, 1, 0)));
    const auto local_point = gp_Pnt(2, 0, 0).Transformed(local_to_world);
    const auto local_vector = direction.Transformed(local_to_world);
    auto publish = [&](const std::string &name, const gp_Trsf &transform, bool original = false) {
        auto shape = original ? box : BRepBuilderAPI_Transform(box, transform, true).Shape();
        const auto point = reference.Transformed(transform);
        const auto vector = direction.Transformed(transform);
        StepResult step{name,
                        {{"box", shape}},
                        {{"reference P", PrimitiveKind::Point, point, {}},
                         {"transformed X direction", PrimitiveKind::Vector, point, vector * 10.0}},
                        {{"point.x", point.X()},
                         {"point.y", point.Y()},
                         {"point.z", point.Z()},
                         {"vector.x", vector.X()},
                         {"vector.y", vector.Y()},
                         {"vector.z", vector.Z()},
                         {"volume", inspect(shape).volume}},
                        check_shape(shape)};
        if (original) {
            step.primitives.push_back({"local (2,0,0)", PrimitiveKind::Point, local_point, {}});
            step.primitives.push_back(
                {"local X in world", PrimitiveKind::Vector, gp_Pnt(5, 10, 0), local_vector * 10});
            step.measurements.insert({{"local_point.x", local_point.X()},
                                      {"local_point.y", local_point.Y()},
                                      {"local_vector.x", local_vector.X()},
                                      {"local_vector.y", local_vector.Y()}});
        }
        run.publish(std::move(step));
    };
    publish("01 原始长方体与局部坐标", gp_Trsf(), true);
    publish("02 A：先平移 T", translation);
    // Rightmost transform acts first.
    publish("03 A：再旋转 R（R*T）", rotation * translation);
    publish("04 B：先旋转 R", rotation);
    publish("05 B：再平移 T（T*R）", translation * rotation);
    run.log("P is an actual original box vertex. R rotates about world origin +Z; T is world +X.");
    run.log("gp_Vec ignores translation; gp_Dir is normalized and cannot represent a zero vector.");
    run.log("Local frame origin=(5,10,0), X=(0,1,0): local point (2,0,0) -> world (5,12,0).");
    run.log(
        "Units: mm and mm^3; angle input degrees converted to radians. Camera changes no shape.");
}
} // namespace
Experiment make_course_experiment() {
    return {"transform-order",
            "第一周：坐标与变换顺序",
            {{"length", "长度 / mm", 40.0, 0.001, 10000.0, {}, "P=(length,0,0)"},
             {"width", "宽度 / mm", 30.0, 0.001, 10000.0, {}, "Y 尺寸"},
             {"height", "高度 / mm", 20.0, 0.001, 10000.0, {}, "Z 尺寸"},
             {"translate_x", "世界 X 平移 / mm", 50.0, -10000.0, 10000.0, {}, "固定世界坐标轴"},
             {"angle_deg", "绕世界 Z 旋转 / °", 90.0, -360.0, 360.0, {}, "绕世界原点，右手规则"}},
            transform_order};
}
} // namespace geometry
