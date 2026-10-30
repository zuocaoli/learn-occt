import std;
#include "experiments/course.hpp"
#include "framework/inspect.hpp"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Trsf.hxx>

namespace geometry {
namespace {
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;
TopoDS_Compound pair(const TopoDS_Shape &first, const TopoDS_Shape &second) {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);
    return compound;
}
StepResult statistics(const std::string &name, const TopoDS_Shape &shape) {
    const auto stats = inspect(shape);
    return {name,
            {{"model", shape}},
            {},
            {{"vertices", static_cast<double>(stats.vertices)},
             {"edges", static_cast<double>(stats.edges)},
             {"faces", static_cast<double>(stats.faces)},
             {"solids", static_cast<double>(stats.solids)}},
            check_shape(shape)};
}
void topology_sharing(const ParameterValues &values, RunContext &run) {
    auto real = [&](const char *key) { return std::get<double>(values.at(key)); };
    const auto box = BRepPrimAPI_MakeBox(real("length"), real("width"), real("height")).Shape();
    auto first = statistics("01 单一长方体：使用次数与唯一对象", box);
    ShapeMap edges, faces;
    TopExp::MapShapes(box, TopAbs_EDGE, edges);
    TopExp::MapShapes(box, TopAbs_FACE, faces);
    std::map<int, std::set<int>> adjacency;
    std::map<int, int> uses, forward, reverse;
    int all_uses = 0;
    for (int f = 1; f <= faces.Extent(); ++f) {
        for (TopExp_Explorer explorer(faces(f), TopAbs_EDGE); explorer.More(); explorer.Next()) {
            const auto &edge = explorer.Current();
            const int e = edges.FindIndex(edge); // identity ignores orientation, not Location
            adjacency[e].insert(f);
            ++uses[e];
            ++all_uses;
            if (edge.Orientation() == TopAbs_FORWARD)
                ++forward[e];
            if (edge.Orientation() == TopAbs_REVERSED)
                ++reverse[e];
            run.log(std::format("face {} uses edge {} orientation {}", f, e,
                                static_cast<int>(edge.Orientation())));
        }
    }
    first.measurements["edge_uses"] = all_uses;
    first.diagnostics.clear();
    for (int e = 1; e <= edges.Extent(); ++e) {
        first.measurements[std::format("edge.{}.uses", e)] = uses[e];
        first.measurements[std::format("edge.{}.faces", e)] = adjacency[e].size();
        std::string owners;
        for (int f : adjacency[e])
            owners += std::to_string(f) + " ";
        const auto message =
            std::format("edge {}: uses={}, distinct faces=[{}], Forward={}, Reversed={}", e,
                        uses[e], owners, forward[e], reverse[e]);
        first.diagnostics.push_back({Severity::Info, message, edges(e)});
        run.log(message);
    }
    run.publish(std::move(first));

    const auto alias = box;
    auto alias_step = statistics("02 普通赋值：共享底层拓扑", pair(box, alias));
    alias_step.measurements["partner"] = box.IsPartner(alias);
    alias_step.measurements["same"] = box.IsSame(alias);
    alias_step.measurements["equal"] = box.IsEqual(alias);
    run.publish(std::move(alias_step));

    const auto independent =
        BRepPrimAPI_MakeBox(real("length"), real("width"), real("height")).Shape();
    auto independent_step = statistics("03 同位独立构造：坐标相同但不共享", pair(box, independent));
    independent_step.measurements["partner"] = box.IsPartner(independent);
    independent_step.measurements["same"] = box.IsSame(independent);
    independent_step.measurements["equal"] = box.IsEqual(independent);
    run.publish(std::move(independent_step));

    const auto reversed = box.Reversed();
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(real("offset"), 0, 0));
    const auto located = box.Moved(TopLoc_Location(translation));
    StepResult identity{"04 Orientation 与 Location 的身份差别",
                        {{"original", box}, {"located instance", located}},
                        {},
                        {{"reversed.partner", box.IsPartner(reversed)},
                         {"reversed.same", box.IsSame(reversed)},
                         {"reversed.equal", box.IsEqual(reversed)},
                         {"located.partner", box.IsPartner(located)},
                         {"located.same", box.IsSame(located)},
                         {"located.equal", box.IsEqual(located)},
                         {"located.translation_x", translation.Value(1, 4)},
                         {"empty_location.same", box.IsSame(box.Moved(TopLoc_Location()))}},
                        check_shape(box)};
    run.publish(std::move(identity));
    run.log("IsPartner: TShape; IsSame: TShape+Location; IsEqual: TShape+Location+Orientation.");
    run.log("An explicitly constructed zero-translation Location need not be the same Location "
            "identity as an empty one.");
    run.log("Counts deduplicate by IsSame. Edge usage orientation is kept separately; ids only "
            "belong to this run.");
    run.log("Coincident independent solids are a comparison compound, not a Boolean union or one "
            "connected solid.");
}
} // namespace
Experiment make_course_experiment() {
    return {"topology-sharing",
            "第三周：拓扑身份与共享",
            {{"length", "长度 / mm", 40.0, 0.001, 10000.0, {}, "独立模型使用相同尺寸"},
             {"width", "宽度 / mm", 30.0, 0.001, 10000.0, {}, "Y 尺寸"},
             {"height", "高度 / mm", 20.0, 0.001, 10000.0, {}, "Z 尺寸"},
             {"offset",
              "实例平移 / mm",
              60.0,
              0.0,
              10000.0,
              {},
              "0 时几何重合，比较显式 Location 与空 Location 的身份"}},
            topology_sharing};
}
} // namespace geometry
