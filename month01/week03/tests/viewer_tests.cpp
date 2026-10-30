import std;
#include "framework/ui/workbench.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtGui/QScreen>
#include <QtGui/QWheelEvent>
#include <QtTest/QTest>
#include <TopExp_Explorer.hxx>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "xcb");
    geometry::initialize_runtime();
    QApplication app(argc, argv);
    try {
        geometry::Workbench window(geometry::make_experiments());
        window.show();
        require(QTest::qWaitForWindowExposed(&window), "Window exposure");
        QTest::qWait(150);
        auto *viewport = window.viewport();
        require(viewport->ready(), "GLX viewer initialization");
        auto *first = window.object_tree()->topLevelItem(0)->child(0);
        window.object_tree()->setCurrentItem(first);
        viewport->fit_all();
        QTest::qWait(60);
        require(viewport->save_image("viewer.png"), "OCCT image export");
        QImage image("viewer.png");
        require(!image.isNull(), "Image decoding");
        require(std::abs(image.width() -
                         qRound(viewport->width() * viewport->devicePixelRatioF())) <= 1 &&
                    std::abs(image.height() -
                             qRound(viewport->height() * viewport->devicePixelRatioF())) <= 1,
                "Viewer/native window dimensions after docking layout");
        // A real model must produce pixels different from its background.
        const auto background = image.pixelColor(5, 5);
        int distinct = 0;
        for (int y = 20; y < image.height() - 20; y += 4)
            for (int x = 20; x < image.width() - 20; x += 4)
                if (image.pixelColor(x, y) != background)
                    ++distinct;
        require(distinct > 500, "Nonblank model rendering");

        int selections = 0;
        TopoDS_Shape picked;
        QObject::connect(viewport, &geometry::Viewport::shape_selected,
                         [&](const TopoDS_Shape &shape) {
                             if (!shape.IsNull()) {
                                 ++selections;
                                 picked = shape;
                             }
                         });
        viewport->set_selection_mode(TopAbs_FACE);
        QTest::mouseMove(viewport, viewport->rect().center());
        QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, viewport->rect().center());
        QTest::qWait(60);
        require(selections > 0 && picked.ShapeType() == TopAbs_FACE, "Mouse face selection");
        require(window.object_tree()->currentItem()->text(0).contains("Face"),
                "Viewer to tree selection");
        const auto point = viewport->physical_point(QPointF(25.0, 40.0));
        require(point.x() == qRound(25 * viewport->devicePixelRatioF()) &&
                    point.y() == qRound(40 * viewport->devicePixelRatioF()),
                "Physical coordinate conversion");

        QTest::mousePress(viewport, Qt::RightButton, Qt::NoModifier, viewport->rect().center());
        QTest::mouseMove(viewport, viewport->rect().center() + QPoint(40, 30), 20);
        QTest::mouseRelease(viewport, Qt::RightButton, Qt::NoModifier,
                            viewport->rect().center() + QPoint(40, 30));
        QTest::qWait(60);
        require(viewport->save_image("viewer-rotated.png") && QImage("viewer-rotated.png") != image,
                "Orbit gesture changes rendered camera");
        QImage rotated("viewer-rotated.png");
        QTest::mousePress(viewport, Qt::MiddleButton, Qt::NoModifier, viewport->rect().center());
        QTest::mouseMove(viewport, viewport->rect().center() + QPoint(20, 10), 20);
        QTest::mouseRelease(viewport, Qt::MiddleButton, Qt::NoModifier,
                            viewport->rect().center() + QPoint(20, 10));
        QTest::qWait(60);
        require(viewport->save_image("viewer-panned.png") && QImage("viewer-panned.png") != rotated,
                "Pan gesture changes rendered camera");
        QImage panned("viewer-panned.png");
        const QPointF center(viewport->rect().center());
        QWheelEvent wheel(center, viewport->mapToGlobal(center.toPoint()), {}, QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(viewport, &wheel);
        QTest::qWait(60);
        require(viewport->save_image("viewer-zoomed.png") && QImage("viewer-zoomed.png") != panned,
                "Wheel zoom changes rendered camera");
        viewport->set_wireframe(true);
        QTest::qWait(60);
        require(viewport->save_image("viewer-wireframe.png") &&
                    QImage("viewer-wireframe.png") != QImage("viewer-zoomed.png"),
                "Wireframe changes rendering");
        viewport->set_wireframe(false);
        first->setCheckState(0, Qt::Unchecked);
        QTest::qWait(60);
        require(viewport->save_image("viewer-hidden.png") &&
                    QImage("viewer-hidden.png") != QImage("viewer-zoomed.png"),
                "Visibility changes rendering");
        first->setCheckState(0, Qt::Checked);
        viewport->standard_view(V3d_XposYnegZpos);
        QTest::qWait(80);
        require(window.screen()->grabWindow(window.winId()).save("workbench.png"),
                "Workbench screenshot");
        // Exercise the actual weekly experiment, not just the copied foundation box.
        auto *selector = window.findChild<QComboBox *>("experimentSelector");
        selector->setCurrentIndex(1);
        window.run_current();
        require(window.last_run().status == geometry::RunStatus::Succeeded,
                "Weekly experiment execution");
        for (int stage = 0; stage < window.object_tree()->topLevelItemCount(); ++stage) {
            window.object_tree()->setCurrentItem(window.object_tree()->topLevelItem(stage));
            viewport->fit_all();
            QTest::qWait(40);
            const auto path = std::format("course-stage-{}.png", stage);
            require(viewport->save_image(path), "Weekly stage image export");
            QImage stage_image(QString::fromStdString(path));
            require(!stage_image.isNull(), "Weekly stage image decoding");
            const auto bg = stage_image.pixelColor(5, 5);
            int ink = 0;
            for (int y = 10; y < stage_image.height() - 100; y += 3)
                for (int x = 10; x < stage_image.width() - 10; x += 3)
                    if (stage_image.pixelColor(x, y) != bg)
                        ++ink;
            require(ink > 10, "Weekly geometry renders beyond the axis trihedron");
        }
        auto *course_object = window.object_tree()->topLevelItem(0)->child(0);
        window.object_tree()->setCurrentItem(course_object);
        viewport->select_shape({});
        viewport->fit_all();
        QTest::qWait(60);
        require(window.screen()->grabWindow(window.winId()).save("course-workbench.png"),
                "Weekly screenshot");
        std::cout << "Real viewer checks passed; DPR=" << viewport->devicePixelRatioF() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
