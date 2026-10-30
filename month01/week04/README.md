# 第一月第四周：面边界、方向、pcurve 与基础检查

## 本周训练

- **承接与前置**：[第三周](../week03/README.md)已区分拓扑身份与使用次数；会读取曲线/曲面参数、导数和 Location。
- **目标**：读取边界环和方向，比较三维曲线与参数域表示，观察圆柱接缝，理解容差和有效性证据的范围。
- **独立练习**：观察 XY 平面上 40×30 mm、中心 (20,15)、孔半径 5 mm 的给定 Face；每次边使用至少取 5 个内点，再观察半径 10、高 20 的圆柱侧面接缝。
- **验收**：一个外环、一个内环，面积约 1121.460184 mm²；采样偏差符合对象容差，接缝使用两次但只关联一个不同面。最后给定断开 Wire 被定位为无效。

## 1. 支撑曲面与 Face 边界环怎样配合，外环和内环怎样表达孔？

支撑平面可在无限参数域中取点，但 Face 的材料区域受边界环限定。外环围住矩形，内环排除圆孔，因此面域不是简单 UV 包围矩形。程序用 FaceClassifier 验证中心落在孔内（OUT）、(5,5) 位于材料（IN）、(45,5) 位于矩形外（OUT）。面积为 1200−πR²，不能用实体体积验收。给定样例的生成放在独立 [fixtures.cpp](experiments/fixtures.cpp)，本周阅读边界，不把构造步骤作为练习。

## 2. Edge、Wire、Face 的方向怎样配合，为什么不能只看屏幕上的顺逆时针？

方向属于拓扑使用实例，必须结合所属面、参数域和遍历语境解释；相机翻转就可能改变屏幕上的顺逆时针。程序列出环与边使用的方向，并把整个 Face 反向：底层支撑曲面仍共享，参数几何法向不变，按该 Face 的 FORWARD/REVERSED 使用解释的法向相反。这个局部对照不等于已经证明整个壳有正确的外向方向；INTERNAL/EXTERNAL 也不能直接套同一正负规则。

## 3. pcurve 怎样把曲面参数域中的边界映射回三维？

pcurve 给出 (u(t),v(t))，支撑曲面再计算 S(u,v)。程序先检查三维曲线、pcurve 与 SameParameter 条件，取二者有效范围的交集，分别应用曲线/曲面的 Location，再比较 C₃D(t) 与 S(pcurve(t))。记录每点 t、UV、两路世界坐标及偏差，汇总最大值。平面上的 pcurve 可以临时计算得到，不保证已存储在 Edge 中；日志的 stored 字段反映这一点。

## 4. 为什么圆柱面的同一条接缝边可能在一个面中出现两次？

周期圆柱展开为参数矩形，u=0 与 u=2π 是不同的参数边界，却对应同一条三维接缝。按 Wire 顺序读取的两次 Edge 使用有各自的方向及 pcurve；本例两条 pcurve 的中点 u 相差 2π。唯一接缝数为 1、使用次数为 2、不同相邻 Face 数为 1，因此第三周长方体的“两不同面”结论不能泛化。这里仅观察接缝，不进行面分割或裁剪。

## 5. 对象容差和有效性检查分别说明什么，检查通过还不能证明什么？

容差描述对象表示所容许的偏差，不是任意算法统一的精度旋钮。程序把采样偏差与边/面容差之和及按坐标尺度估计的浮点舍入余量对照，不改变对象容差。有限采样一致不证明整个参数区间一致，BRepCheck 通过也不证明尺寸、边界和用途符合设计；因此另查面积、区域分类和接缝关系。最后阶段故意保留不连通 Wire：实验执行成功，但模型无效，诊断关联到具体形状且仍可导出。没有进行修复。

## 代码与执行轨迹

主要逻辑：[experiments/course.cpp](experiments/course.cpp)；独立验收：[tests/course_tests.cpp](tests/course_tests.cpp)。

```text
01 给定带孔面 → 两环、面积、区域分类、逐边三维/UV 采样
02 反向 Face → 支撑曲面共享，使用法向反转
03 圆柱侧面 → 一条接缝、两次使用、两条 pcurve、一个不同面
04 断开 Wire → valid=false，诊断定位；整体运行仍 succeeded
```

界面选择第一/第三阶段观察模型，底部选择诊断条目可突出显示该次使用对应的边；样本点位于三维边上，UV 数值见日志。无需专用 UV 视图。对象编号仅用于本次运行，不作持久命名。

## 独立构建与运行

使用 **C++23、import std、Clang + CMake + Ninja + clangd**。本周自带完整框架和给定样例，只引用第三方 `../../occt`；依赖、IDE 和视窗验收命令见 [tools/README.md](tools/README.md)。在本周目录执行：

```bash
./run.sh
./run.sh --headless --param radius=7 --param translate_x=25
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
./build/demo --headless --output build/results/boundaries
./build/demo --input build/results/boundaries/step-0-object-0.brep
```

默认实验 `face-boundaries`。radius=7 时面积约 1046.061960 mm²，Location 平移不改变面积；radius≥15 时孔不再严格在矩形内部，实验拒绝并返回失败。输出目录必须为空或不存在；报告与 BREP 包括无效 Wire，便于复现。圆柱样例尺寸固定，radius 参数只修改平面圆孔。

## 月末独立验收

不看代码说明长方体 12 条唯一边与 24 次使用、圆柱接缝 1/2/1 的区别；从带孔面解释 Surface/Face，指出一次三维/UV 对照的前提、证据和限制。下一月再学习从边界主动构造面、壳与实体；求交、布尔和系统修复留到对应月份。

返回[课程目录](../../README.md)。
