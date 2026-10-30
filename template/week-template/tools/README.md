# 本周构建、运行与 IDE

所有命令从**本周目录**执行，无仓库根运行工具，不依赖其他周产物。

## 依赖

| 组件 | 模板使用版本 / 要求 |
|---|---|
| 语言 | C++23，`import std;` |
| Clang / clang-scan-deps / clangd | 22.1.8，使用匹配版本 |
| CMake | 4.3.x；升级时核对 `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` token |
| Ninja | ≥ 1.11 |
| 标准库 | libstdc++ 16.2.1 与模块开发文件 |
| Qt | 6.11.2 Widgets；GUI 测试另需 Qt Test |
| OCCT | `../../occt` 当前 8.1.0 dev1-3d097a03，共享库及匹配头文件 |
| 图形环境 | Linux X11/XWayland、X11 开发文件、OpenGL/GLX 驱动 |
| 格式化 | clang-format 22；模板自带 `.clang-format` |
| 可选验收工具 | Xvfb、xauth，用于虚拟 X11 视窗测试 |

```bash
clang++ --version
clang-scan-deps --version
clangd --version
cmake --version
ninja --version
pkg-config --modversion Qt6Widgets Qt6Test x11 gl
```

## 配置、构建、测试

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
./build/demo
./build/demo --headless --param length=60
./build/console-demo --param length=60
```

`./run.sh [参数...]` 依次配置、构建和运行 `build/demo`。默认配置为 Debug，本地生成 `build/compile_commands.json`。即使是 GUI 可执行文件，`--headless` 也在创建 QApplication 前分流，不连接显示器。

完全不安装 Qt/X11 开发依赖时可仅构建核心：

```bash
cmake --preset headless
cmake --build --preset headless
ctest --preset headless --no-tests=error
./build/headless/demo --param length=60
```

切换编译器、模块或 OCCT 安装后刷新缓存并重启 clangd：

```bash
cmake --fresh --preset default -DOpenCASCADE_DIR=/absolute/occt/lib/cmake/opencascade
cmake --build --preset default
```

CMake 按功能链接 OCCT imported targets；不导入全部 Draw、OCAF 或交换格式组件。Linux executable 的 DT_RPATH 解决当前 OCCT 库缺少 RUNPATH 时的间接依赖查找，资源目录由配置头文件初始化。无需全局修改 `LD_LIBRARY_PATH`，已有 `CSF_*` 环境设置会保留。

## 真实视窗验收

`ui` CTest 使用 `QT_QPA_PLATFORM=offscreen`，只验证适配逻辑，不创建 OCCT GLX 视窗。实际交互测试必须有显示环境：

```bash
cd build
xvfb-run -a -s '-screen 0 1600x1100x24' ./viewer-tests
QT_SCALE_FACTOR=2 xvfb-run -a -s '-screen 0 1600x1100x24' ./viewer-tests
```

预期输出 `Real viewer checks passed`，当前目录生成 `viewer.png` 和 `workbench.png`。测试验证非空模型渲染、鼠标选面与树联动、坐标换算，并执行旋转、平移、缩放、线框与显隐操作。两次截图会覆盖，可在再次执行前另存需要的图像。

手工补充检查：展开面和边、选择相邻面共享的边、切换阶段、修改尺寸重跑、保存后读取、关闭窗口。默认平台选择 `xcb`；纯 Wayland 原生视窗尚未实现。

## clangd 与调试

将本周目录单独作为 IDE 文件夹打开；先配置构建，再启动 PATH 中的原生 clangd。`.clangd` 与 VS Code settings 均指向本周 `build/` 数据库。无需 Python 包装或跨周导航工具。

实现文件将 `import std;` 放在头文件包含之前，避免当前 clangd 对 libstdc++ 头文件预编译区与标准库 BMI 重复定义的诊断。新增实验沿用此顺序；不屏蔽诊断，也不为 IDE 换用另一套代码。

```bash
clangd --check=experiments/box_transform.cpp --compile-commands-dir=build --log=error
clangd --check=framework/ui/workbench.cpp --compile-commands-dir=build --tweaks=ExtractVariable --log=error
```

`--check` 除解析和导航外还会穷举重构动作；界面中的泛型 lambda 无法展开依赖的 `auto` 类型，因此该条验收只启用 `ExtractVariable` 重构检查。语法诊断不被屏蔽，IDE 中仍使用默认完整功能。

`.vscode/launch.json` 是可选 CodeLLDB 配置，启动本周 `build/demo`。使用其他 IDE 时也直接启动此文件。框架目标为 Debug，但当前 OCCT 安装是 Release，不能据此承诺内核源码单步调试。

第 6 月准备匹配源码与带调试符号的 OCCT 安装，通过 `OpenCASCADE_DIR` 切换，fresh 配置；若调试信息记录了不同源码路径，在调试器中配置 source-map。不要覆盖课程原有安装或混用其他版本头文件。
