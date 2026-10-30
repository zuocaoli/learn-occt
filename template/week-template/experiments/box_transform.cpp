import std;
#include "framework/experiment.hpp"
#include "framework/inspect.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>

namespace geometry {
namespace {
void box_transform(const ParameterValues &values, RunContext &run) {
    auto real = [&](const char *key) { return std::get<double>(values.at(key)); };
    // These OCCT calls intentionally remain visible in the course experiment.
    BRepPrimAPI_MakeBox builder(real("length"), real("width"), real("height"));
    const auto box = builder.Shape();
    auto stats = inspect(box);
    run.publish(
        {"01 原始长方体",
         {{"box", box}},
         {{"origin", PrimitiveKind::Point, gp_Pnt(0, 0, 0), {}},
          {"Z axis", PrimitiveKind::Vector, gp_Pnt(0, 0, 0), gp_Vec(0, 0, real("height") * 1.3)}},
         {{"volume", stats.volume}},
         check_shape(box)});

    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(real("translate_x"), 0, 0));
    auto moved = BRepBuilderAPI_Transform(box, translation, true).Shape();
    run.publish({"02 沿 X 平移",
                 {{"translated", moved}},
                 {},
                 {{"volume", inspect(moved).volume}},
                 check_shape(moved)});

    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)),
                         real("angle_deg") * std::numbers::pi / 180.0);
    auto rotated = BRepBuilderAPI_Transform(moved, rotation, true).Shape();
    run.publish({"03 绕世界 Z 轴旋转",
                 {{"rotated", rotated}},
                 {},
                 {{"volume", inspect(rotated).volume}},
                 check_shape(rotated)});
    run.log("Coordinates are model coordinates. Length convention: mm; volume: mm^3.");
}
} // namespace
std::vector<Experiment> make_experiments() {
    return {{"box-transform",
             "长方体创建与刚体变换",
             {{"length", "长度 / mm", 40.0, 0.001, 10000.0, {}, "矩形底面的 X 尺寸"},
              {"width", "宽度 / mm", 30.0, 0.001, 10000.0, {}, "矩形底面的 Y 尺寸"},
              {"height", "高度 / mm", 20.0, 0.001, 10000.0, {}, "沿 Z 方向的高度"},
              {"translate_x", "X 平移 / mm", 50.0, -10000.0, 10000.0, {}, "模型变换，不是相机移动"},
              {"angle_deg", "旋转角 / °", 30.0, -360.0, 360.0, {}, "平移后，绕世界原点 Z 轴旋转"}},
             box_transform}};
}
} // namespace geometry
