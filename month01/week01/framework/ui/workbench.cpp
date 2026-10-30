import std;
#include "framework/ui/workbench.hpp"
#include "framework/artifacts.hpp"
#include "framework/inspect.hpp"
#include <TopoDS_Iterator.hxx>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtCore/QSignalBlocker>

namespace geometry {
namespace {
class CompactDoubleSpinBox : public QDoubleSpinBox {
  protected:
    QString textFromValue(double value) const override {
        return locale().toString(value, 'g', 15);
    }
};
QString text(const std::string &value) {
    return QString::fromStdString(value);
}
QString shape_label(const TopoDS_Shape &shape) {
    static const char *names[] = {"Compound", "CompSolid", "Solid",  "Shell", "Face",
                                  "Wire",     "Edge",      "Vertex", "Shape"};
    return QString::fromLatin1(names[shape.ShapeType()]) + " / " +
           QString::fromLatin1(shape.Orientation() == TopAbs_REVERSED  ? "Reversed"
                               : shape.Orientation() == TopAbs_FORWARD ? "Forward"
                                                                       : "Internal/External");
}
} // namespace
Workbench::Workbench(std::vector<Experiment> experiments, const CommandLine &command,
                     QWidget *parent)
    : QMainWindow(parent), experiments_(std::move(experiments)) {
    if (experiments_.empty())
        throw std::invalid_argument("Register at least one experiment");
    setWindowTitle("OCCT 几何实验 · 每周独立框架");
    resize(1440, 900);
    auto *toolbar = addToolBar("实验与视图");
    toolbar->setMovable(false);
    experiment_box_ = new QComboBox;
    experiment_box_->setObjectName("experimentSelector");
    for (const auto &experiment : experiments_)
        experiment_box_->addItem(text(experiment.title));
    toolbar->addWidget(experiment_box_);
    run_button_ = new QPushButton("运行");
    run_button_->setObjectName("runButton");
    toolbar->addWidget(run_button_);
    connect(run_button_, &QPushButton::clicked, this, &Workbench::run_current);
    connect(toolbar->addAction("重置参数"), &QAction::triggered, this,
            &Workbench::reset_parameters);
    connect(toolbar->addAction("读取 BREP"), &QAction::triggered, this, &Workbench::load_brep);
    connect(toolbar->addAction("导出本次运行"), &QAction::triggered, this,
            &Workbench::export_results);
    toolbar->addSeparator();
    viewport_ = new Viewport(this);
    setCentralWidget(viewport_);
    connect(toolbar->addAction("适合窗口"), &QAction::triggered, viewport_, &Viewport::fit_all);
    for (const auto &[label, orientation] :
         std::vector<std::pair<QString, V3d_TypeOfOrientation>>{{"轴测", V3d_XposYnegZpos},
                                                                {"俯视", V3d_Zpos},
                                                                {"前视", V3d_Yneg},
                                                                {"右视", V3d_Xpos}})
        connect(toolbar->addAction(label), &QAction::triggered, this,
                [this, orientation] { viewport_->standard_view(orientation); });
    auto *wireframe = toolbar->addAction("线框");
    wireframe->setCheckable(true);
    connect(wireframe, &QAction::toggled, viewport_, &Viewport::set_wireframe);
    auto *modes = new QComboBox;
    for (const auto &[label, mode] :
         std::vector<std::pair<QString, TopAbs_ShapeEnum>>{{"选整体", TopAbs_SHAPE},
                                                           {"选顶点", TopAbs_VERTEX},
                                                           {"选边", TopAbs_EDGE},
                                                           {"选面", TopAbs_FACE}})
        modes->addItem(label, static_cast<int>(mode));
    toolbar->addWidget(modes);
    connect(modes, &QComboBox::currentIndexChanged, this, [this, modes] {
        viewport_->set_selection_mode(static_cast<TopAbs_ShapeEnum>(modes->currentData().toInt()));
    });

    auto *left = new QWidget;
    auto *left_layout = new QVBoxLayout(left);
    auto *parameter_widget = new QWidget;
    parameter_form_ = new QFormLayout(parameter_widget);
    auto *parameter_scroll = new QScrollArea;
    parameter_scroll->setWidgetResizable(true);
    parameter_scroll->setWidget(parameter_widget);
    parameter_scroll->setMinimumHeight(250);
    left_layout->addWidget(parameter_scroll, 1);
    tree_ = new QTreeWidget;
    tree_->setObjectName("objectTree");
    tree_->setHeaderLabel("阶段 / 对象 / 拓扑");
    tree_->setToolTip("节点编号只在本次运行有效，不是持久拓扑名称");
    left_layout->addWidget(tree_, 2);
    auto *left_dock = new QDockWidget("参数与模型", this);
    left_dock->setWidget(left);
    addDockWidget(Qt::LeftDockWidgetArea, left_dock);
    inspector_ = new QPlainTextEdit;
    inspector_->setObjectName("inspector");
    inspector_->setReadOnly(true);
    auto *inspector_dock = new QDockWidget("对象信息", this);
    inspector_dock->setWidget(inspector_);
    addDockWidget(Qt::RightDockWidgetArea, inspector_dock);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    diagnostics_ = new QListWidget;
    auto *bottom = new QSplitter;
    bottom->addWidget(log_);
    bottom->addWidget(diagnostics_);
    auto *bottom_dock = new QDockWidget("运行 / 测量 / 诊断", this);
    bottom_dock->setWidget(bottom);
    addDockWidget(Qt::BottomDockWidgetArea, bottom_dock);
    resizeDocks({left_dock, inspector_dock}, {330, 280}, Qt::Horizontal);
    resizeDocks({bottom_dock}, {180}, Qt::Vertical);

    connect(experiment_box_, &QComboBox::currentIndexChanged, this, &Workbench::build_parameters);
    connect(tree_, &QTreeWidget::itemExpanded, this, &Workbench::expand_item);
    connect(tree_, &QTreeWidget::currentItemChanged, this,
            [this](auto *current, auto *) { select_item(current); });
    connect(tree_, &QTreeWidget::itemChanged, this, [this](auto *item, int) {
        auto found = entries_.find(item);
        if (found != entries_.end() && found->second.stage == active_stage_ &&
            found->second.object >= 0)
            viewport_->set_visible(found->second.object, item->checkState(0) == Qt::Checked);
    });
    connect(viewport_, &Viewport::shape_selected, this, &Workbench::selected_in_view);
    connect(viewport_, &Viewport::viewer_error, this, [this](const QString &message) {
        log_->appendPlainText("Viewer error: " + message);
        statusBar()->showMessage("视窗不可用，核心实验结果仍可导出：" + message);
    });
    connect(diagnostics_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (active_stage_ < 0 || row < 0)
            return;
        const auto &items = run_.steps[active_stage_].diagnostics;
        if (row >= static_cast<int>(items.size()))
            return;
        const auto &shape = items[row].shape;
        selected_in_view(shape);
        viewport_->select_shape(shape);
    });

    if (command.input) {
        experiments_.push_back(brep_experiment(*command.input));
        experiment_box_->addItem(text(experiments_.back().title));
        experiment_box_->setCurrentIndex(static_cast<int>(experiments_.size()) - 1);
    } else {
        const auto &selected = find_experiment(experiments_, command.experiment);
        experiment_box_->setCurrentIndex(static_cast<int>(&selected - experiments_.data()));
    }
    build_parameters();
    const auto values =
        command.input ? defaults(experiments_.back())
                      : command_parameters(experiments_[experiment_box_->currentIndex()], command);
    for (const auto &[key, value] : values) {
        auto *editor = editors_.at(key);
        if (auto *real = qobject_cast<QDoubleSpinBox *>(editor))
            real->setValue(std::get<double>(value));
        else if (auto *integer = qobject_cast<QSpinBox *>(editor))
            integer->setValue(std::get<int>(value));
        else if (auto *boolean = qobject_cast<QCheckBox *>(editor))
            boolean->setChecked(std::get<bool>(value));
        else
            qobject_cast<QComboBox *>(editor)->setCurrentText(text(std::get<std::string>(value)));
    }
    run_current();
    if (command.output)
        export_run(run_experiment_, run_parameters_, run_, *command.output);
}
void Workbench::build_parameters() {
    editors_.clear();
    while (parameter_form_->rowCount())
        parameter_form_->removeRow(0);
    const auto &experiment = experiments_.at(experiment_box_->currentIndex());
    for (const auto &spec : experiment.parameters) {
        QWidget *editor = nullptr;
        std::visit(
            [&](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, double>) {
                    auto *spin = new CompactDoubleSpinBox;
                    spin->setDecimals(15);
                    spin->setRange(spec.minimum.value_or(-1e100), spec.maximum.value_or(1e100));
                    spin->setValue(value);
                    editor = spin;
                } else if constexpr (std::is_same_v<T, int>) {
                    auto *spin = new QSpinBox;
                    spin->setRange(
                        static_cast<int>(spec.minimum.value_or(std::numeric_limits<int>::min())),
                        static_cast<int>(spec.maximum.value_or(std::numeric_limits<int>::max())));
                    spin->setValue(value);
                    editor = spin;
                } else if constexpr (std::is_same_v<T, bool>) {
                    auto *checkbox = new QCheckBox;
                    checkbox->setChecked(value);
                    editor = checkbox;
                } else {
                    auto *choices = new QComboBox;
                    for (const auto &choice : spec.choices)
                        choices->addItem(text(choice));
                    choices->setCurrentText(text(value));
                    editor = choices;
                }
            },
            spec.default_value);
        editor->setObjectName(text(spec.key));
        editor->setToolTip(text(spec.description));
        parameter_form_->addRow(text(spec.label), editor);
        editors_.emplace(spec.key, editor);
    }
    statusBar()->showMessage("参数修改后点击运行；导出始终保存上次实际执行的参数。");
}
ParameterValues Workbench::parameters() const {
    const auto &experiment = experiments_.at(experiment_box_->currentIndex());
    auto values = defaults(experiment);
    for (const auto &[key, editor] : editors_) {
        if (auto *real = qobject_cast<QDoubleSpinBox *>(editor))
            values[key] = real->value();
        else if (auto *integer = qobject_cast<QSpinBox *>(editor))
            values[key] = integer->value();
        else if (auto *boolean = qobject_cast<QCheckBox *>(editor))
            values[key] = boolean->isChecked();
        else
            values[key] = qobject_cast<QComboBox *>(editor)->currentText().toStdString();
    }
    return values;
}
void Workbench::reset_parameters() {
    build_parameters();
}
void Workbench::run_current() {
    run_button_->setEnabled(false);
    experiment_box_->setEnabled(false);
    active_stage_ = -1;
    viewport_->set_step({});
    run_ = {};
    inspector_->clear();
    diagnostics_->clear();
    entries_.clear();
    tree_->clear();
    run_experiment_ = experiments_.at(experiment_box_->currentIndex());
    run_parameters_ = parameters();
    run_ = execute(run_experiment_, run_parameters_);
    build_tree();
    log_->setPlainText(text(report(run_experiment_, run_parameters_, run_)));
    statusBar()->showMessage(text("Run " + status_text(run_.status) + ": " + run_.error));
    run_button_->setEnabled(true);
    experiment_box_->setEnabled(true);
    Q_EMIT run_finished();
}
void Workbench::build_tree() {
    QSignalBlocker blocker(tree_);
    for (int s = 0; s < static_cast<int>(run_.steps.size()); ++s) {
        const auto &step = run_.steps[s];
        auto *stage = new QTreeWidgetItem(tree_, {text(step.name)});
        entries_[stage] = {s, -1, {}, true};
        for (int i = 0; i < static_cast<int>(step.shapes.size()); ++i) {
            const auto &object = step.shapes[i];
            auto *item = new QTreeWidgetItem(stage, {QString("[%1] ").arg(i) + text(object.name)});
            item->setCheckState(0, Qt::Checked);
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            entries_[item] = {s, i, object.shape, false};
        }
        stage->setExpanded(true);
    }
    if (tree_->topLevelItemCount()) {
        auto *last = tree_->topLevelItem(tree_->topLevelItemCount() - 1);
        tree_->setCurrentItem(last);
        show_stage(tree_->topLevelItemCount() - 1);
    }
}
void Workbench::expand_item(QTreeWidgetItem *item) {
    auto found = entries_.find(item);
    if (found == entries_.end() || found->second.expanded || found->second.shape.IsNull())
        return;
    found->second.expanded = true;
    const auto entry = found->second;
    int index = 0;
    for (TopoDS_Iterator child(entry.shape); child.More(); child.Next()) {
        auto *node =
            new QTreeWidgetItem(item, {QString("[%1] ").arg(index++) + shape_label(child.Value())});
        entries_[node] = {entry.stage, -1, child.Value(), false};
        if (child.Value().ShapeType() != TopAbs_VERTEX)
            node->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    }
    if (index == 0)
        item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
}
void Workbench::show_stage(int index) {
    if (index == active_stage_ || index < 0 || index >= static_cast<int>(run_.steps.size()))
        return;
    active_stage_ = index;
    const auto &step = run_.steps[index];
    viewport_->set_step(step);
    for (const auto &[item, entry] : entries_)
        if (entry.stage == index && entry.object >= 0)
            viewport_->set_visible(entry.object, item->checkState(0) == Qt::Checked);
    QSignalBlocker blocker(diagnostics_);
    diagnostics_->clear();
    for (const auto &diagnostic : step.diagnostics)
        diagnostics_->addItem(QString("[%1] ").arg(static_cast<int>(diagnostic.severity)) +
                              text(diagnostic.message));
    inspector_->setPlainText(text(step.name));
}
void Workbench::show_description(const TopoDS_Shape &shape) {
    try {
        inspector_->setPlainText(text(describe(shape)));
    } catch (const std::exception &error) {
        inspector_->setPlainText(QString("Inspection error: ") + error.what());
    }
}
void Workbench::select_item(QTreeWidgetItem *item) {
    auto found = entries_.find(item);
    if (found == entries_.end())
        return;
    show_stage(found->second.stage);
    if (!found->second.shape.IsNull())
        show_description(found->second.shape);
    viewport_->select_shape(found->second.shape);
}
QTreeWidgetItem *Workbench::find_shape(QTreeWidgetItem *item, const TopoDS_Shape &shape) {
    const auto entry = entries_.at(item);
    if (!entry.shape.IsNull() && entry.shape.IsSame(shape))
        return item;
    expand_item(item);
    for (int i = 0; i < item->childCount(); ++i)
        if (auto *found = find_shape(item->child(i), shape))
            return found;
    return nullptr;
}
void Workbench::selected_in_view(const TopoDS_Shape &shape) {
    show_description(shape);
    QSignalBlocker blocker(tree_);
    if (active_stage_ >= 0 && !shape.IsNull()) {
        auto *root = tree_->topLevelItem(active_stage_);
        for (int i = 0; i < root->childCount(); ++i) {
            if (auto *item = find_shape(root->child(i), shape)) {
                tree_->setCurrentItem(item);
                tree_->scrollToItem(item);
                return;
            }
        }
    }
    tree_->setCurrentItem(nullptr);
}
void Workbench::export_results() {
    const auto directory = QFileDialog::getExistingDirectory(this, "选择空目录保存本次运行");
    if (directory.isEmpty())
        return;
    try {
        export_run(run_experiment_, run_parameters_, run_, directory.toStdString());
        statusBar()->showMessage("已导出：" + directory);
    } catch (const std::exception &error) {
        QMessageBox::warning(this, "导出失败", QString::fromUtf8(error.what()));
    }
}
void Workbench::load_brep() {
    const auto path =
        QFileDialog::getOpenFileName(this, "读取 BREP", {}, "BREP (*.brep *.brp);;所有文件 (*)");
    if (path.isEmpty())
        return;
    experiments_.push_back(brep_experiment(path.toStdString()));
    experiment_box_->addItem("读取：" + path);
    experiment_box_->setCurrentIndex(static_cast<int>(experiments_.size()) - 1);
    run_current();
}
} // namespace geometry
