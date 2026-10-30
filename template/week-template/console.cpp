import std;
#include "framework/command_line.hpp"
#include <Standard_Failure.hxx>

int main(int argc, char **argv) {
    try {
        geometry::initialize_runtime();
        return geometry::run_console(geometry::parse_command_line(argc, argv),
                                     geometry::make_experiments());
    } catch (const Standard_Failure &error) {
        std::cerr << "OCCT: " << error.what() << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
