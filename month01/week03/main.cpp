import std;
#include "framework/ui/workbench.hpp"
#include <QtWidgets/QApplication>

int main(int argc, char **argv) {
    try {
        geometry::initialize_runtime();
        const auto command = geometry::parse_command_line(argc, argv);
        auto experiments = geometry::make_experiments();
        // This branch runs before QApplication, so it never opens a display.
        if (command.headless || command.help || command.list)
            return geometry::run_console(command, experiments);
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "xcb");
        QApplication application(argc, argv);
        geometry::Workbench window(std::move(experiments), command);
        window.show();
        return application.exec();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
