import std;
#include "experiments/fixtures.hpp"
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pln.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax3.hxx>

namespace geometry::fixtures {
TopoDS_Face perforated_face(double radius) {
    if (radius <= 0 || radius >= 15)
        throw std::invalid_argument(
            "Hole radius must be positive and strictly inside the rectangle (radius<15)");
    BRepBuilderAPI_MakePolygon rectangle;
    rectangle.Add(gp_Pnt(0, 0, 0));
    rectangle.Add(gp_Pnt(40, 0, 0));
    rectangle.Add(gp_Pnt(40, 30, 0));
    rectangle.Add(gp_Pnt(0, 30, 0));
    rectangle.Close();
    const auto circular_edge =
        BRepBuilderAPI_MakeEdge(
            gp_Circ(gp_Ax2(gp_Pnt(20, 15, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)), radius))
            .Edge();
    auto hole = BRepBuilderAPI_MakeWire(circular_edge).Wire();
    hole.Reverse();
    BRepBuilderAPI_MakeFace face(gp_Pln(gp_Ax3()), rectangle.Wire(), true);
    face.Add(hole);
    if (!face.IsDone())
        throw std::runtime_error("Cannot build given face fixture");
    return face.Face();
}
} // namespace geometry::fixtures
