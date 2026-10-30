import std;
#include "tests/checks.hpp"
#include "experiments/course.hpp"
#include <TopExp.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopExp_Explorer.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>

int main() {
    try {
        using checks::near;
        using checks::require;
        const auto experiment = geometry::make_course_experiment();
        auto values = geometry::defaults(experiment);
        auto run = checks::run(experiment, values);
        require(run.steps.size() == 4, "Four topology comparisons");
        const auto &single = run.steps[0];
        near(single.measurements.at("vertices"), 8, "Unique vertices");
        near(single.measurements.at("edges"), 12, "Unique edges");
        near(single.measurements.at("faces"), 6, "Unique faces");
        near(single.measurements.at("edge_uses"), 24, "Per-face usages");
        // Cross-check the explicit face traversal using OCCT's unique ancestor API.
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                                   TopTools_ShapeMapHasher>
            ancestors;
        TopExp::MapShapesAndUniqueAncestors(single.shapes[0].shape, TopAbs_EDGE, TopAbs_FACE,
                                            ancestors);
        require(ancestors.Extent() == 12, "Independent ancestor count");
        for (int i = 1; i <= ancestors.Extent(); ++i) {
            require(ancestors.FindFromIndex(i).Size() == 2,
                    "Every box edge has two distinct faces");
            near(single.measurements.at(std::format("edge.{}.uses", i)), 2, "Recorded usage count");
            near(single.measurements.at(std::format("edge.{}.faces", i)), 2,
                 "Recorded distinct ancestor count");
        }
        near(run.steps[1].measurements.at("vertices"), 8, "Aliases keep vertices");
        near(run.steps[1].measurements.at("edges"), 12, "Aliases keep edges");
        near(run.steps[1].measurements.at("same"), 1, "Alias identity");
        near(run.steps[2].measurements.at("vertices"), 16, "Independent vertices");
        near(run.steps[2].measurements.at("edges"), 24, "Independent edges");
        near(run.steps[2].measurements.at("faces"), 12, "Independent faces");
        near(run.steps[2].measurements.at("partner"), 0, "No coordinate-based identity");
        const auto &identity = run.steps[3].measurements;
        near(identity.at("reversed.partner"), 1, "Reverse shares TShape");
        near(identity.at("reversed.same"), 1, "Same ignores orientation");
        near(identity.at("reversed.equal"), 0, "Equal includes orientation");
        near(identity.at("located.partner"), 1, "Location shares TShape");
        near(identity.at("located.same"), 0, "Same includes location");
        geometry::set_parameter(experiment, values, "offset", "0");
        geometry::set_parameter(experiment, values, "length", "60");
        run = checks::run(experiment, values);
        near(run.steps[3].measurements.at("empty_location.same"), 1,
             "Empty Location is the identity instance");
        near(run.steps[3].measurements.at("located.translation_x"), 0,
             "Zero translation has unchanged coordinates");
        const auto &comparison = run.steps[3].shapes;
        for (TopExp_Explorer v(comparison[1].shape, TopAbs_VERTEX); v.More(); v.Next()) {
            const auto point = BRep_Tool::Pnt(TopoDS::Vertex(v.Current()));
            double distance = 1e100;
            for (TopExp_Explorer original(comparison[0].shape, TopAbs_VERTEX); original.More();
                 original.Next())
                distance = std::min(
                    distance, point.Distance(BRep_Tool::Pnt(TopoDS::Vertex(original.Current()))));
            near(distance, 0,
                 "Zero translation geometry matches even when Location identity differs");
        }
        near(run.steps[0].measurements.at("edge_uses"), 24, "Size change keeps box topology");
        std::cout << "week03 course checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
