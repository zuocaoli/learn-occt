import std;
#include "framework/inspect.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>

namespace geometry {
ShapeStats inspect(const TopoDS_Shape &shape) {
    ShapeStats stats;
    if (shape.IsNull())
        return stats;
    auto count = [&](TopAbs_ShapeEnum type) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
        TopExp::MapShapes(shape, type, map);
        return map.Extent();
    };
    stats.vertices = count(TopAbs_VERTEX);
    stats.edges = count(TopAbs_EDGE);
    stats.faces = count(TopAbs_FACE);
    stats.solids = count(TopAbs_SOLID);
    GProp_GProps properties;
    if (stats.solids > 0)
        BRepGProp::VolumeProperties(shape, properties);
    stats.volume = properties.Mass();
    stats.valid = BRepCheck_Analyzer(shape).IsValid();
    return stats;
}
std::vector<Diagnostic> check_shape(const TopoDS_Shape &shape) {
    if (shape.IsNull())
        return {{Severity::Error, "Null shape", {}}};
    BRepCheck_Analyzer analyzer(shape);
    std::vector<Diagnostic> diagnostics;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> parts;
    TopExp::MapShapes(shape, parts);
    for (int i = 1; i <= parts.Extent(); ++i) {
        const auto &part = parts(i);
        const auto &result = analyzer.Result(part);
        if (result.IsNull())
            continue;
        auto record = [&](const auto &statuses) {
            for (const auto status : statuses) {
                if (status != BRepCheck_NoError) {
                    std::ostringstream message;
                    BRepCheck::Print(status, message);
                    diagnostics.push_back({Severity::Error, message.str(), part});
                }
            }
        };
        record(result->Status());
        // Some defects only occur in the context of an owning face/wire.
        for (result->InitContextIterator(); result->MoreShapeInContext();
             result->NextShapeInContext())
            record(result->StatusOnShape());
    }
    if (!analyzer.IsValid() && diagnostics.empty())
        diagnostics.push_back(
            {Severity::Error, "Invalid BREP; inspect ancestor relationships", shape});
    if (diagnostics.empty())
        diagnostics.push_back({Severity::Info, "BREP valid", shape});
    return diagnostics;
}
std::string describe(const TopoDS_Shape &shape) {
    if (shape.IsNull())
        return "No shape";
    static constexpr const char *names[] = {"Compound", "CompSolid", "Solid",  "Shell", "Face",
                                            "Wire",     "Edge",      "Vertex", "Shape"};
    static constexpr const char *orientations[] = {"Forward", "Reversed", "Internal", "External"};
    std::ostringstream out;
    out << std::setprecision(12) << names[shape.ShapeType()]
        << "\nOrientation: " << orientations[shape.Orientation()] << "\nLocation:\n";
    const auto transform = shape.Location().Transformation();
    for (int row = 1; row <= 3; ++row) {
        for (int column = 1; column <= 4; ++column)
            out << transform.Value(row, column) << ' ';
        out << '\n';
    }
    if (shape.ShapeType() == TopAbs_VERTEX) {
        auto vertex = TopoDS::Vertex(shape);
        auto point = BRep_Tool::Pnt(vertex);
        out << "Point: " << point.X() << ", " << point.Y() << ", " << point.Z()
            << "\nTolerance: " << BRep_Tool::Tolerance(vertex);
    } else if (shape.ShapeType() == TopAbs_EDGE) {
        auto edge = TopoDS::Edge(shape);
        out << "Tolerance: " << BRep_Tool::Tolerance(edge);
        if (!BRep_Tool::Degenerated(edge)) {
            BRepAdaptor_Curve curve(edge);
            out << "\nCurve type (GeomAbs): " << static_cast<int>(curve.GetType())
                << "\nParameter: " << curve.FirstParameter() << " .. " << curve.LastParameter();
        }
    } else if (shape.ShapeType() == TopAbs_FACE) {
        auto face = TopoDS::Face(shape);
        out << "Tolerance: " << BRep_Tool::Tolerance(face) << "\nSurface type (GeomAbs): "
            << static_cast<int>(BRepAdaptor_Surface(face).GetType());
    }
    const auto stats = inspect(shape);
    out << "\nVertices: " << stats.vertices << "\nEdges: " << stats.edges
        << "\nFaces: " << stats.faces << "\nSolids: " << stats.solids
        << "\nVolume: " << stats.volume << "\nValid: " << std::boolalpha << stats.valid;
    return out.str();
}
} // namespace geometry
