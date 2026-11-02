import std;
#include "framework/ui/workbench.hpp"
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtTest/QTest>
#include <TopExp_Explorer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void drag_mouse(QWidget *widget, Qt::MouseButton button, QPoint start, QPoint end) {
    QTest::mouseMove(widget, start);
    QTest::mousePress(widget, button, Qt::NoModifier, start);
    for (int i = 1; i <= 10; ++i) {
        const QPointF point(start + (end - start) * i / 10);
        QMouseEvent move(QEvent::MouseMove, point, widget->mapToGlobal(point.toPoint()),
                         Qt::NoButton, button, Qt::NoModifier);
        QApplication::sendEvent(widget, &move);
        QTest::qWait(10);
    }
    QTest::mouseRelease(widget, button, Qt::NoModifier, end);
}
void check_left_drag(geometry::Viewport *viewport, const int &selection_changes) {
    QTest::mouseMove(viewport, QPoint(5, 5));
    QTest::qWait(60);
    require(viewport->save_image("viewer-before-left-drag.png"), "Left drag baseline");
    const QImage before_drag("viewer-before-left-drag.png");
    const int changes_before = selection_changes;
    // Exercise held-button moves over both geometry and empty background,
    // before any right-button rotation has initialized the orbit controller.
    for (const auto &[start, end] : std::vector<std::pair<QPoint, QPoint>>{
             {viewport->rect().center(), viewport->rect().center() + QPoint(160, 100)},
             {QPoint(5, 5), QPoint(100, 80)}}) {
        drag_mouse(viewport, Qt::LeftButton, start, end);
        QTest::mouseMove(viewport, QPoint(5, 5));
        QTest::qWait(60);
        require(viewport->ready(), "Left drag keeps viewer alive");
        require(selection_changes == changes_before, "Left drag leaves selection unchanged");
        require(viewport->save_image("viewer-after-left-drag.png") &&
                    QImage("viewer-after-left-drag.png") == before_drag,
                "Left drag leaves model and camera unchanged");
    }
}
class SelectionViewport : public geometry::Viewport {
  public:
    QPoint screen_point(const gp_Pnt &point) const {
        int x, y;
        view_->Convert(point.X(), point.Y(), point.Z(), x, y);
        return {qRound(x / devicePixelRatioF()), qRound(y / devicePixelRatioF())};
    }

  protected:
    void HandleViewEvents(const occ::handle<AIS_InteractiveContext> &context,
                          const occ::handle<V3d_View> &view) override {
        view_ = view;
        AIS_ViewController::HandleViewEvents(context, view);
    }

  private:
    occ::handle<V3d_View> view_;
};
int yellow_pixels(const QImage &image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (color.red() > 180 && color.green() > 150 && color.blue() < 120)
                ++count;
        }
    return count;
}
void check_selection_highlights() {
    SelectionViewport viewport;
    viewport.resize(800, 600);
    const auto box = BRepPrimAPI_MakeBox(40, 30, 20).Shape();
    viewport.set_step({"Selection", {{"box", box}}, {}, {}, {}});
    viewport.show();
    require(QTest::qWaitForWindowExposed(&viewport), "Selection viewport exposure");
    QTest::qWait(150);
    TopoDS_Shape picked;
    QObject::connect(&viewport, &geometry::Viewport::shape_selected,
                     [&](const TopoDS_Shape &shape) { picked = shape; });
    struct PickCase {
        TopAbs_ShapeEnum mode;
        gp_Pnt point;
        const char *name;
    };
    for (const auto &test :
         {PickCase{TopAbs_SHAPE, {20, 0, 10}, "object"}, PickCase{TopAbs_FACE, {20, 0, 10}, "face"},
          PickCase{TopAbs_EDGE, {20, 0, 20}, "edge"},
          PickCase{TopAbs_VERTEX, {40, 0, 20}, "vertex"}}) {
        viewport.set_selection_mode(test.mode);
        const auto position = viewport.screen_point(test.point);
        QTest::mouseMove(&viewport, position);
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, position);
        // Remove hover highlighting; selection must remain visibly highlighted.
        QTest::mouseMove(&viewport, QPoint(5, 5));
        QTest::qWait(60);
        require(!picked.IsNull() && (test.mode == TopAbs_SHAPE ? picked.IsSame(box)
                                                               : picked.ShapeType() == test.mode),
                "Click picks the requested topology type");
        const auto path = std::format("selection-{}.png", test.name);
        require(viewport.save_image(path), "Selected topology image");
        require(yellow_pixels(QImage(QString::fromStdString(path))) >
                    (test.mode == TopAbs_SHAPE || test.mode == TopAbs_FACE ? 500 : 10),
                "Selected topology stays visibly yellow after pointer leaves");
        QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
        QTest::qWait(60);
        require(picked.IsNull(), "Background click clears selection");
        require(viewport.save_image("selection-cleared.png") &&
                    yellow_pixels(QImage("selection-cleared.png")) == 0,
                "Background click clears persistent selection highlighting");
    }
}
} // namespace
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "xcb");
    geometry::initialize_runtime();
    QApplication app(argc, argv);
    try {
        check_selection_highlights();
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
        int selection_changes = 0;
        TopoDS_Shape picked;
        QObject::connect(viewport, &geometry::Viewport::shape_selected,
                         [&](const TopoDS_Shape &shape) {
                             ++selection_changes;
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

        check_left_drag(viewport, selection_changes);
        // A click must still select after ignored drags.
        QTest::mouseMove(viewport, viewport->rect().center());
        QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, viewport->rect().center());
        QTest::qWait(60);
        require(selections > 1 && picked.ShapeType() == TopAbs_FACE, "Click after left drag");
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
        check_left_drag(viewport, selection_changes);
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
