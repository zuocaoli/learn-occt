#pragma once
#include "framework/experiment.hpp"

namespace geometry {
struct ShapeStats {
    int vertices = 0;
    int edges = 0;
    int faces = 0;
    int solids = 0;
    double volume = 0;
    bool valid = false;
};
ShapeStats inspect(const TopoDS_Shape &shape);
std::string describe(const TopoDS_Shape &shape);
std::vector<Diagnostic> check_shape(const TopoDS_Shape &shape);
} // namespace geometry
