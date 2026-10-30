# 几何课程框架模板

复制到任意 `几何引擎/monthNN/weekYY/` 后独立使用。模板不是一周正式课程，也不预先实现六个月的建模算法。

**工具链：Clang + CMake + Ninja + clangd。** 本模板选择 **C++23、`import std;`**；项目允许 C++20 及以上，但这份模板不兼容 C++20。标准库导入用于实现文件，OCCT/Qt 及公共类型通过头文件接入。

## 运行

以下命令均在本模板或复制后的周目录执行。完整依赖、IDE 和源码调试配置见 [tools/README.md](tools/README.md)。

```bash
./run.sh
./run.sh --headless --experiment box-transform --param length=60
```

默认实验依次创建长方体、沿 X 平移、绕世界 Z 轴旋转。参数以 mm、角度以度约定；OCCT 本身不自动赋予坐标单位。刚体变换不改变模型尺寸和体积，相机操作不修改模型。

界面中左键选择，右键拖动旋转，中键拖动平移，滚轮缩放。顶部切换整体、顶点、边、面选择及标准视角；左侧选择阶段、展开拓扑和勾选对象显隐；右侧查看所选对象；底部选择诊断可定位关联形状。

修改参数后点击“运行”。“导出本次运行”保存上次**实际执行**的参数，尚未运行的面板修改不会覆盖结果。无效模型仍能观察和导出；失败运行保留此前已发布的阶段。每次重跑重建结果，不提供撤销。

## 保存与读取

```bash
./run.sh --headless --param length=60 --output build/results/length60
./run.sh --headless --input build/results/length60/step-2-object-0.brep
./run.sh --input build/results/length60/step-2-object-0.brep
```

输出目录必须不存在或为空，防止覆盖失败案例。目录中有 `parameters.txt`、`report.txt`、阶段模型及关联诊断对象的 BREP。报告包含参数、阶段测量、辅助图元、诊断、版本和复现命令；文件编号与报告对应，不是持久拓扑标识。读取外部 BREP 的复现仍依赖输入文件，分享案例时一并保留它。

## 复制与扩展

从仓库根目录复制时排除构建产物：

```bash
mkdir -p 几何引擎/month01/week01
tar -C 几何引擎/template/week-template --exclude=./build -cf - . \
  | tar -C 几何引擎/month01/week01 -xf -
```

各周默认读取 `../../occt`；没有跨周源码、脚本或构建依赖。复制后：

1. 替换本 README，集中回答该周五题，补充承接、新目标、独立练习和验收。
2. 在 `experiments/` 添加本周实验，在 `make_experiments()` 中注册，并在本周 CMake 中加入源文件及所需 OCCT targets。
3. 参数声明驱动 GUI 和命令行；在实验函数中直接编写 OCCT 调用，使用 `RunContext::publish()` 发布阶段，`log()` 记录解释。
4. 发布后的形状按只读使用；原地修复前复制输入。阶段可带点、线段、向量、测量和关联子形状的诊断。
5. 新增对应的核心验收；不需要每道题都写程序。

所有注册实验必须提供完整有效的参数默认值；枚举参数使用字符串默认值及非空 `choices`。实数必须有限，整数必须完整解析，布尔命令行只接受 `true` / `false`。参数范围是入口约束，壁厚等几何条件由实验判断。

## 验证与预期

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
```

默认尺寸 40 × 30 × 20：三个阶段均为 8 个去重顶点、12 条边、6 个面、1 个实体，体积约 24000 mm³，有效性为 true。长度改为 60 时体积约 36000 mm³。执行 `--param length=-1` 应失败并返回非零状态。

核心测试包含 BREP 往返、参数校验、失败后阶段保留和无效模型检查。离屏 Qt 测试验证参数控件与选择同步；真实视窗另行验收，不能以离屏测试代替。

框架边界、六个月能力映射及初始验收记录见[框架设计](../../plan/课程代码框架设计.md)。
