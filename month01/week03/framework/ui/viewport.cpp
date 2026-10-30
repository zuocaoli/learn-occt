import std;
#include "framework/ui/viewport.hpp"
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Xw_Window.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_PointAspect.hxx>
#include <Image_PixMap.hxx>
#include <Quantity_Color.hxx>
#include <Standard_Failure.hxx>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QImage>

namespace geometry {
namespace {
Aspect_VKeyMouse mouse_buttons(Qt::MouseButtons buttons) {
    unsigned int mask = Aspect_VKeyMouse_NONE;
    if (buttons & Qt::LeftButton)
        mask |= Aspect_VKeyMouse_LeftButton;
    if (buttons & Qt::MiddleButton)
        mask |= Aspect_VKeyMouse_MiddleButton;
    if (buttons & Qt::RightButton)
        mask |= Aspect_VKeyMouse_RightButton;
    return mask;
}
Aspect_VKeyFlags key_flags(Qt::KeyboardModifiers modifiers) {
    unsigned int mask = Aspect_VKeyFlags_NONE;
    if (modifiers & Qt::ShiftModifier)
        mask |= Aspect_VKeyFlags_SHIFT;
    if (modifiers & Qt::ControlModifier)
        mask |= Aspect_VKeyFlags_CTRL;
    if (modifiers & Qt::AltModifier)
        mask |= Aspect_VKeyFlags_ALT;
    return mask;
}
} // namespace
Viewport::Viewport(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
    auto &gestures = ChangeMouseGestureMap();
    gestures.Clear();
    gestures.Bind(Aspect_VKeyMouse_RightButton, AIS_MouseGesture_RotateOrbit);
    gestures.Bind(Aspect_VKeyMouse_MiddleButton, AIS_MouseGesture_Pan);
}
Viewport::~Viewport() {
    if (!context_.IsNull())
        context_->RemoveAll(false);
    tree_highlight_.Nullify();
    objects_.clear();
    context_.Nullify();
    if (!view_.IsNull())
        view_->Remove();
    view_.Nullify();
    viewer_.Nullify();
    driver_.Nullify();
    display_.Nullify();
}
void Viewport::initialize() {
    if (ready() || initialization_failed_)
        return;
    try {
        if (QGuiApplication::platformName() != "xcb")
            throw std::runtime_error("Viewer requires QT_QPA_PLATFORM=xcb (X11/XWayland)");
        display_ = new Aspect_DisplayConnection();
        driver_ = new OpenGl_GraphicDriver(display_);
        viewer_ = new V3d_Viewer(driver_);
        viewer_->SetDefaultLights();
        viewer_->SetLightOn();
        context_ = new AIS_InteractiveContext(viewer_);
        context_->SetDisplayMode(AIS_Shaded, false);
        view_ = viewer_->CreateView();
        view_->SetWindow(new Xw_Window(display_, static_cast<Aspect_Drawable>(winId())));
        view_->SetBackgroundColor(Quantity_Color(0.08, 0.11, 0.16, Quantity_TOC_RGB));
        view_->SetProj(V3d_XposYnegZpos);
        view_->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.09, V3d_ZBUFFER);
        display_step();
    } catch (const std::exception &error) {
        initialization_failed_ = true;
        if (!view_.IsNull())
            view_->Remove();
        view_.Nullify();
        Q_EMIT viewer_error(QString::fromUtf8(error.what()));
    }
}
void Viewport::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    initialize();
}
void Viewport::paintEvent(QPaintEvent *) {
    if (ready()) {
        // Qt may deliver resizeEvent before X11 commits the new native geometry.
        // Refresh at paint time as well, after the layout/native window update.
        view_->Window()->DoResize();
        view_->MustBeResized();
        FlushViewEvents(context_, view_, true);
        view_->Redraw();
    }
}
void Viewport::resizeEvent(QResizeEvent *) {
    if (ready()) {
        view_->Window()->DoResize();
        view_->MustBeResized();
        update();
    }
}
void Viewport::set_step(const StepResult &step) {
    step_ = step;
    if (ready())
        display_step();
}
void Viewport::display_step() {
    context_->RemoveAll(false);
    objects_.clear();
    tree_highlight_.Nullify();
    static constexpr Quantity_NameOfColor colors[] = {Quantity_NOC_LIGHTSTEELBLUE,
                                                      Quantity_NOC_CORAL, Quantity_NOC_PALEGREEN};
    for (std::size_t i = 0; i < step_.shapes.size(); ++i) {
        if (step_.shapes[i].shape.IsNull()) {
            objects_.push_back({});
            continue;
        }
        occ::handle<AIS_Shape> object = new AIS_Shape(step_.shapes[i].shape);
        object->SetColor(colors[i % 3]);
        object->SetDisplayMode(wireframe_ ? AIS_WireFrame : AIS_Shaded);
        object->Attributes()->SetFaceBoundaryDraw(true);
        context_->Display(object, wireframe_ ? AIS_WireFrame : AIS_Shaded,
                          AIS_Shape::SelectionMode(selection_mode_), false);
        objects_.push_back(object);
    }
    auto auxiliary = [&](const TopoDS_Shape &shape, Quantity_NameOfColor color) {
        occ::handle<AIS_Shape> object = new AIS_Shape(shape);
        object->SetColor(color);
        object->SetWidth(2.0);
        object->Attributes()->SetPointAspect(
            new Prs3d_PointAspect(Aspect_TOM_O_POINT, Quantity_Color(color), 5.0));
        context_->Display(object, AIS_Shaded, -1, false);
    };
    for (const auto &primitive : step_.primitives) {
        if (primitive.kind == PrimitiveKind::Point) {
            auxiliary(BRepBuilderAPI_MakeVertex(primitive.origin).Vertex(), Quantity_NOC_YELLOW);
        } else if (primitive.vector.Magnitude() > 1e-12) {
            auxiliary(BRepBuilderAPI_MakeEdge(primitive.origin,
                                              primitive.origin.Translated(primitive.vector))
                          .Edge(),
                      Quantity_NOC_YELLOW);
            if (primitive.kind == PrimitiveKind::Vector) {
                const double length = primitive.vector.Magnitude();
                auto base = primitive.origin.Translated(primitive.vector * 0.85);
                auxiliary(BRepPrimAPI_MakeCone(gp_Ax2(base, gp_Dir(primitive.vector)),
                                               length * 0.035, 0, length * 0.15)
                              .Shape(),
                          Quantity_NOC_YELLOW);
            }
        }
    }
    fit_all();
}
void Viewport::set_visible(int index, bool visible) {
    if (!ready() || index < 0 || index >= static_cast<int>(objects_.size()) ||
        objects_[index].IsNull())
        return;
    if (visible)
        context_->Display(objects_[index], false);
    else {
        context_->Erase(objects_[index], false);
        context_->ClearSelected(false);
        if (!tree_highlight_.IsNull()) {
            context_->Remove(tree_highlight_, false);
            tree_highlight_.Nullify();
        }
    }
    update();
}
void Viewport::select_shape(const TopoDS_Shape &shape) {
    if (!ready())
        return;
    context_->ClearSelected(false);
    if (!tree_highlight_.IsNull()) {
        context_->Remove(tree_highlight_, false);
        tree_highlight_.Nullify();
    }
    if (!shape.IsNull()) {
        tree_highlight_ = new AIS_Shape(shape);
        tree_highlight_->SetColor(Quantity_NOC_YELLOW);
        tree_highlight_->SetWidth(4.0);
        tree_highlight_->SetTransparency(0.45);
        context_->Display(tree_highlight_, AIS_Shaded, -1, false);
    }
    update();
}
void Viewport::set_selection_mode(TopAbs_ShapeEnum type) {
    selection_mode_ = type;
    if (!ready())
        return;
    context_->ClearSelected(false);
    if (!tree_highlight_.IsNull()) {
        context_->Remove(tree_highlight_, false);
        tree_highlight_.Nullify();
    }
    for (const auto &object : objects_)
        if (!object.IsNull()) {
            context_->Deactivate(object);
            context_->Activate(object, AIS_Shape::SelectionMode(type));
        }
    update();
}
void Viewport::set_wireframe(bool enabled) {
    wireframe_ = enabled;
    if (ready()) {
        for (const auto &object : objects_)
            if (!object.IsNull())
                context_->SetDisplayMode(object, enabled ? AIS_WireFrame : AIS_Shaded, false);
        update();
    }
}
void Viewport::fit_all() {
    if (ready()) {
        view_->FitAll(0.08, false);
        view_->ZFitAll();
        update();
    }
}
void Viewport::standard_view(V3d_TypeOfOrientation orientation) {
    if (ready()) {
        view_->SetProj(orientation);
        fit_all();
    }
}
bool Viewport::save_image(const std::string &path) {
    if (!ready())
        return false;
    Image_PixMap pixels;
    int width_px = 0;
    int height_px = 0;
    view_->Window()->Size(width_px, height_px);
    if (!view_->ToPixMap(pixels, width_px, height_px))
        return false;
    // OCCT without FreeImage may write PPM even for a .png filename. Qt encodes
    // the requested format explicitly, independent of OCCT image codecs.
    QImage image(width_px, height_px, QImage::Format_RGB32);
    for (int y = 0; y < height_px; ++y) {
        auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width_px; ++x) {
            const auto color = pixels.PixelColor(x, y).GetRGB();
            row[x] = qRgb(qRound(color.Red() * 255), qRound(color.Green() * 255),
                          qRound(color.Blue() * 255));
        }
    }
    return image.save(QString::fromStdString(path));
}
NCollection_Vec2<int> Viewport::physical_point(const QPointF &point) const {
    return NCollection_Vec2<int>(qRound(point.x() * devicePixelRatioF()),
                                 qRound(point.y() * devicePixelRatioF()));
}
void Viewport::flush() {
    if (ready()) {
        FlushViewEvents(context_, view_, true);
        update();
    }
}
void Viewport::mousePressEvent(QMouseEvent *event) {
    if (!ready())
        return;
    setFocus();
    UpdateMouseButtons(physical_point(event->position()), mouse_buttons(event->buttons()),
                       key_flags(event->modifiers()), false);
    flush();
}
void Viewport::mouseReleaseEvent(QMouseEvent *event) {
    if (!ready())
        return;
    UpdateMouseButtons(physical_point(event->position()), mouse_buttons(event->buttons()),
                       key_flags(event->modifiers()), false);
    flush();
}
void Viewport::mouseMoveEvent(QMouseEvent *event) {
    if (!ready())
        return;
    UpdateMousePosition(physical_point(event->position()), mouse_buttons(event->buttons()),
                        key_flags(event->modifiers()), false);
    flush();
}
void Viewport::wheelEvent(QWheelEvent *event) {
    if (!ready())
        return;
    UpdateMouseScroll(Aspect_ScrollDelta(physical_point(event->position()),
                                         event->angleDelta().y() / 120.0,
                                         key_flags(event->modifiers())));
    flush();
}
void Viewport::focusOutEvent(QFocusEvent *event) {
    ResetViewInput();
    QWidget::focusOutEvent(event);
}
void Viewport::OnSelectionChanged(const occ::handle<AIS_InteractiveContext> &context,
                                  const occ::handle<V3d_View> &) {
    if (!tree_highlight_.IsNull()) {
        context_->Remove(tree_highlight_, false);
        tree_highlight_.Nullify();
    }
    context->InitSelected();
    Q_EMIT shape_selected(context->MoreSelected() && context->HasSelectedShape()
                              ? context->SelectedShape()
                              : TopoDS_Shape{});
}
} // namespace geometry
