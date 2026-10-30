#pragma once
#include "framework/command_line.hpp"
#include "framework/ui/viewport.hpp"
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTreeWidget>

class QComboBox;
class QFormLayout;
class QPlainTextEdit;
class QListWidget;
class QPushButton;

namespace geometry {
class Workbench : public QMainWindow {
    Q_OBJECT
  public:
    explicit Workbench(std::vector<Experiment> experiments, const CommandLine &command = {},
                       QWidget *parent = nullptr);
    void run_current();
    void reset_parameters();
    ParameterValues parameters() const;
    const RunContext &last_run() const {
        return run_;
    }
    const ParameterValues &run_parameters() const {
        return run_parameters_;
    }
    Viewport *viewport() const {
        return viewport_;
    }
    QTreeWidget *object_tree() const {
        return tree_;
    }
  Q_SIGNALS:
    void run_finished();

  private:
    struct TreeEntry {
        int stage = -1;
        int object = -1;
        TopoDS_Shape shape;
        bool expanded = false;
    };
    void build_parameters();
    void build_tree();
    void expand_item(QTreeWidgetItem *item);
    void select_item(QTreeWidgetItem *item);
    void show_stage(int index);
    void selected_in_view(const TopoDS_Shape &shape);
    QTreeWidgetItem *find_shape(QTreeWidgetItem *item, const TopoDS_Shape &shape);
    void show_description(const TopoDS_Shape &shape);
    void export_results();
    void load_brep();
    std::vector<Experiment> experiments_;
    Experiment run_experiment_;
    ParameterValues run_parameters_;
    RunContext run_;
    std::map<std::string, QWidget *> editors_;
    std::map<QTreeWidgetItem *, TreeEntry> entries_;
    int active_stage_ = -1;
    QComboBox *experiment_box_;
    QFormLayout *parameter_form_;
    QPushButton *run_button_;
    Viewport *viewport_;
    QTreeWidget *tree_;
    QPlainTextEdit *inspector_;
    QPlainTextEdit *log_;
    QListWidget *diagnostics_;
};
} // namespace geometry
