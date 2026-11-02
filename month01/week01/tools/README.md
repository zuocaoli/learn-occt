# 本周构建、运行与 IDE

默认课程实验 `transform-order`；`box-transform` 作为框架基础示例保留。课程代码和工具均是本周本地副本。

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

## 安装 OCCT SDK

课程使用上游 [Open-Cascade-SAS/OCCT](https://github.com/Open-Cascade-SAS/OCCT) 的提交
`3d097a0328e71b826377d4814ab05ec3c3d23871`（8.1.0 dev1-3d097a03）。
`几何引擎/occt/` 是本机安装目录，已被 Git 忽略，首次检出需要自行安装。
课程调用此提交的 API；系统 OCCT 7.9.3 无法编译本课程。

先准备上表工具链以及 FreeType、X11、OpenGL/GLX 开发文件。以下为发行版依赖命令，编译器等工具仍需满足上表版本：

```bash
# Fedora
sudo dnf install git freetype-devel libX11-devel libXext-devel libXmu-devel libXi-devel mesa-libGL-devel mesa-libGLU-devel
# Debian / Ubuntu 使用这一条替代 Fedora 命令
sudo apt install git libfreetype-dev libx11-dev libxext-dev libxmu-dev libxi-dev libgl-dev libglu1-mesa-dev
```

下面从**本周目录**执行；`../..` 对模板和各月课程均为 `几何引擎/`。
获取源码需要联网。克隆步骤适用于尚无 `occt-src/` 的首次安装；已有源码时核对远程来源和工作区，再切换到指定提交。

```bash
cd ../..
geometry_dir="$PWD"
git clone --filter=blob:none --no-checkout https://github.com/Open-Cascade-SAS/OCCT.git occt-src
git -C occt-src checkout --detach 3d097a0328e71b826377d4814ab05ec3c3d23871
git -C occt-src rev-parse HEAD

cmake -S occt-src -B occt-src/build-course -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY_TYPE=Shared \
  -DINSTALL_DIR="$geometry_dir/occt" -DINSTALL_DIR_LAYOUT=Unix \
  -DINSTALL_DIR_LIB=lib -DINSTALL_DIR_CMAKE=lib/cmake/opencascade \
  -DBUILD_USE_VCPKG=OFF -DUSE_GIT_HASH=ON \
  -DBUILD_MODULE_FoundationClasses=ON -DBUILD_MODULE_ModelingData=ON \
  -DBUILD_MODULE_ModelingAlgorithms=ON -DBUILD_MODULE_Visualization=ON \
  -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_DataExchange=OFF \
  -DBUILD_MODULE_Draw=OFF -DUSE_TK=OFF \
  -DUSE_FREETYPE=ON -DUSE_XLIB=ON -DUSE_OPENGL=ON \
  -DUSE_TBB=OFF -DUSE_FREEIMAGE=OFF -DUSE_FFMPEG=OFF \
  -DUSE_VTK=OFF -DUSE_OPENVR=OFF -DUSE_RAPIDJSON=OFF -DUSE_DRACO=OFF
cmake --build occt-src/build-course --parallel 4
cmake --install occt-src/build-course
test -f occt/lib/cmake/opencascade/OpenCASCADEConfig.cmake
rg 'OCC_VERSION_(COMPLETE|DEVELOPMENT)' occt/include/opencascade/Standard_Version.hxx
```

预期头文件输出 `8.1.0` 和 `dev1-3d097a03`。安装生成 `include/opencascade/`、
`lib/`、`bin/` 和 `share/opencascade/resources/`；课程只需上述四个 OCCT 模块，无需 Tcl/Tk、Draw、OCAF 或交换格式组件。并行度可按内存调整，安装到仓库内不需要 sudo。

回到**原本的周目录**后，显式指定 SDK 并检查其存在，避免缺失时 CMake 改用系统安装：

```bash
occt_package_dir="$PWD/../../occt/lib/cmake/opencascade"
test -f "$occt_package_dir/OpenCASCADEConfig.cmake" &&
  cmake --fresh --preset default -DOpenCASCADE_DIR="$occt_package_dir"
cmake --build --preset default
ctest --preset default --no-tests=error
```

配置输出应包含 `OCCT: 8.1.0 dev1-3d097a03`。核心配置改用 `--preset headless`，
同样传入 `-DOpenCASCADE_DIR="$occt_package_dir"`，后续构建和测试也选 `headless`。

## 配置、构建、测试

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
./build/demo
./build/demo --headless --param angle_deg=0
./build/console-demo --param angle_deg=0
```

`./run.sh [参数...]` 依次配置、构建和运行 `build/demo`。默认配置为 Debug，本地生成 `build/compile_commands.json`。即使是 GUI 可执行文件，`--headless` 也在创建 QApplication 前分流，不连接显示器。

完全不安装 Qt/X11 开发依赖时可仅构建核心：

```bash
cmake --preset headless
cmake --build --preset headless
ctest --preset headless --no-tests=error
./build/headless/demo --param angle_deg=0
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

预期输出 `Real viewer checks passed`，当前目录生成 `viewer.png` 和 `workbench.png`。测试验证基础示例的非空模型渲染、鼠标选面与树联动、坐标换算及旋转、平移、缩放、线框和显隐；随后切换到本周实际实验，逐阶段检查非空图像，生成 `course-stage-N.png` 和 `course-workbench.png`。两次截图会覆盖，可在再次执行前另存需要的图像。

手工补充检查：展开面和边、选择相邻面共享的边、切换阶段、修改尺寸重跑、保存后读取、关闭窗口。默认平台选择 `xcb`；纯 Wayland 原生视窗尚未实现。

## 左键拖动退出的排查记录

左键仅单击选择，按住拖动不改变选择、模型或相机；右键旋转，中键平移，滚轮缩放。真实视窗测试在首次右键旋转前比较左键拖动前后的图像和选择事件，并验证拖动后单击仍能选面。

`AIS_ViewController` 除主手势映射外，还有默认的左键物体拖拽映射。只清空 `ChangeMouseGestureMap()` 不会关闭该路径；未处理的物体拖动仍可进入相机旋转，引发异常相机参数和 `ZFar should be greater than ZNear` 断言。视窗通过 OCCT 提供的 `SetAllowDragging(false)` 明确关闭物体拖拽，保留单击选择和已有相机手势。

## 选中但看不到高亮

OCCT 默认选中样式为细灰色线框，容易与模型边界混淆。视窗同时设置整体和局部选中样式：黄色、着色显示、独立深度层，并放大点和加粗边；青色继续用于悬停预选。单击后将鼠标移到空白处，黄色仍应保留；单击空白处应清除。

真实视窗测试对整体、面、边、顶点分别点击并核对回传拓扑类型，再检查鼠标移开后的黄色像素与取消选择后的清除结果。生成 `selection-object.png`、`selection-face.png`、`selection-edge.png`、`selection-vertex.png` 和 `selection-cleared.png`，用于人工核对。

## clangd 与调试

将本周目录单独作为 IDE 文件夹打开；先配置构建，再启动 PATH 中的原生 clangd。`.clangd` 与 VS Code settings 均指向本周 `build/` 数据库。无需 Python 包装或跨周导航工具。

实现文件将 `import std;` 放在头文件包含之前，避免当前 clangd 对 libstdc++ 头文件预编译区与标准库 BMI 重复定义的诊断。新增实验沿用此顺序；不屏蔽诊断，也不为 IDE 换用另一套代码。

```bash
clangd --check=experiments/course.cpp --compile-commands-dir=build --tweaks=ExtractVariable --log=error
clangd --check=framework/ui/workbench.cpp --compile-commands-dir=build --tweaks=ExtractVariable --log=error
```

`--check` 除解析和导航外还会穷举重构动作；界面中的泛型 lambda 无法展开依赖的 `auto` 类型，因此该条验收只启用 `ExtractVariable` 重构检查。语法诊断不被屏蔽，IDE 中仍使用默认完整功能。

`.vscode/launch.json` 是可选 CodeLLDB 配置，启动本周 `build/demo`。使用其他 IDE 时也直接启动此文件。框架目标为 Debug，但当前 OCCT 安装是 Release，不能据此承诺内核源码单步调试。

第 6 月准备匹配源码与带调试符号的 OCCT 安装，通过 `OpenCASCADE_DIR` 切换，fresh 配置；若调试信息记录了不同源码路径，在调试器中配置 source-map。不要覆盖课程原有安装或混用其他版本头文件。
