#pragma once
#include "framework/experiment.hpp"
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewController.hxx>
#include <V3d_View.hxx>
#include <QtWidgets/QWidget>

class Aspect_DisplayConnection;
class OpenGl_GraphicDriver;
class V3d_Viewer;

namespace geometry {
// Linux native-window adapter. Qt owns the window; OCCT owns GL rendering.
class Viewport : public QWidget, protected AIS_ViewController {
    Q_OBJECT
  public:
    explicit Viewport(QWidget *parent = nullptr);
    ~Viewport() override;
    void set_step(const StepResult &step);
    void set_visible(int index, bool visible);
    void select_shape(const TopoDS_Shape &shape);
    void set_selection_mode(TopAbs_ShapeEnum type);
    void set_wireframe(bool enabled);
    void fit_all();
    void standard_view(V3d_TypeOfOrientation orientation);
    bool ready() const {
        return !view_.IsNull();
    }
    bool save_image(const std::string &path);
    // Shared coordinate conversion for all mouse events, including high DPI.
    NCollection_Vec2<int> physical_point(const QPointF &point) const;
  Q_SIGNALS:
    void shape_selected(const TopoDS_Shape &shape);
    void viewer_error(const QString &message);

  protected:
    QPaintEngine *paintEngine() const override {
        return nullptr;
    }
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void OnSelectionChanged(const occ::handle<AIS_InteractiveContext> &,
                            const occ::handle<V3d_View> &) override;

  private:
    void initialize();
    void display_step();
    void flush();
    occ::handle<Aspect_DisplayConnection> display_;
    occ::handle<OpenGl_GraphicDriver> driver_;
    occ::handle<V3d_Viewer> viewer_;
    occ::handle<V3d_View> view_;
    occ::handle<AIS_InteractiveContext> context_;
    occ::handle<AIS_Shape> tree_highlight_;
    std::vector<occ::handle<AIS_Shape>> objects_;
    StepResult step_;
    TopAbs_ShapeEnum selection_mode_ = TopAbs_SHAPE;
    bool wireframe_ = false;
    bool initialization_failed_ = false;
};
} // namespace geometry
