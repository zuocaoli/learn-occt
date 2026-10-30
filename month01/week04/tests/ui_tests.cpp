import std;
#include "framework/ui/workbench.hpp"
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <TopExp_Explorer.hxx>

class UiTests : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void course_controls_and_results() {
        geometry::CommandLine command;
        command.experiment = "face-boundaries";
        geometry::Workbench window(geometry::make_experiments(), command);
        QVERIFY(window.last_run().status == geometry::RunStatus::Succeeded);
        QVERIFY(!window.last_run().steps.empty());
        QCOMPARE(window.object_tree()->topLevelItemCount(),
                 static_cast<int>(window.last_run().steps.size()));
        QSignalSpy finished(&window, &geometry::Workbench::run_finished);
        window.run_current();
        QCOMPARE(finished.count(), 1);
        QVERIFY(window.last_run().status == geometry::RunStatus::Succeeded);
    }
    void parameter_run_and_reset() {
        geometry::Workbench window(geometry::make_experiments());
        auto *length = window.findChild<QDoubleSpinBox *>("length");
        QVERIFY(length);
        length->setValue(60.0);
        QCOMPARE(std::get<double>(window.parameters().at("length")), 60.0);
        // Editing does not rewrite the last completed run's parameters.
        QCOMPARE(std::get<double>(window.run_parameters().at("length")), 40.0);
        QSignalSpy finished(&window, &geometry::Workbench::run_finished);
        window.findChild<QPushButton *>("runButton")->click();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(window.last_run().steps.back().measurements.at("volume"), 36000.0);
        window.reset_parameters();
        QCOMPARE(std::get<double>(window.parameters().at("length")), 40.0);
    }
    void topology_and_selection_sync() {
        geometry::Workbench window(geometry::make_experiments());
        auto *tree = window.object_tree();
        QCOMPARE(tree->topLevelItemCount(), 3);
        auto *stage = tree->topLevelItem(0);
        auto *object = stage->child(0);
        tree->setCurrentItem(object);
        QVERIFY(
            window.findChild<QPlainTextEdit *>("inspector")->toPlainText().contains("Vertices: 8"));
        object->setExpanded(true);
        QVERIFY(object->childCount() > 0);
        const auto shape = window.last_run().steps[0].shapes[0].shape;
        TopExp_Explorer faces(shape, TopAbs_FACE);
        QVERIFY(faces.More());
        window.viewport()->shape_selected(faces.Current());
        QVERIFY(tree->currentItem());
        QVERIFY(tree->currentItem()->text(0).contains("Face"));
        QVERIFY(window.findChild<QPlainTextEdit *>("inspector")->toPlainText().startsWith("Face"));
        object->setCheckState(0, Qt::Unchecked);
        QCOMPARE(object->checkState(0), Qt::Unchecked);
        window.viewport()->shape_selected({});
        QVERIFY(!tree->currentItem());
    }
    void typed_parameter_editors_and_failed_run() {
        auto experiments = geometry::make_experiments();
        experiments.push_back({"typed",
                               "Typed",
                               {{"count", "Count", 2, 1, 10, {}, ""},
                                {"enabled", "Enabled", true, {}, {}, {}, ""},
                                {"mode", "Mode", std::string("a"), {}, {}, {"a", "b"}, ""}},
                               [](const auto &, auto &) {
                                   throw std::runtime_error("Deliberate UI experiment failure");
                               }});
        geometry::Workbench window(std::move(experiments));
        auto *selector = window.findChild<QComboBox *>("experimentSelector");
        selector->setCurrentIndex(selector->count() - 1);
        window.findChild<QSpinBox *>("count")->setValue(4);
        window.findChild<QCheckBox *>("enabled")->setChecked(false);
        window.findChild<QComboBox *>("mode")->setCurrentIndex(1);
        auto parameters = window.parameters();
        QCOMPARE(std::get<int>(parameters.at("count")), 4);
        QVERIFY(!std::get<bool>(parameters.at("enabled")));
        QCOMPARE(std::get<std::string>(parameters.at("mode")), std::string("b"));
        window.run_current();
        QVERIFY(window.last_run().status == geometry::RunStatus::Failed);
        QVERIFY(window.last_run().steps.empty());
        QCOMPARE(window.object_tree()->topLevelItemCount(), 0);
        QVERIFY(window.findChild<QPushButton *>("runButton")->isEnabled());
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
