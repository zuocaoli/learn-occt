# 第一月第三周：拓扑层级、共享对象与邻接关系

## 本周训练

- **承接与前置**：[第二周](../week02/README.md)已能读取支撑几何；会使用容器、遍历与去重。
- **目标**：解释拓扑层级、身份与使用实例，建立边到不同面的邻接表。
- **独立练习**：统计长方体按面使用的边与唯一边，比较同一模型的普通赋值、独立同位构造、反向和带 Location 的实例。
- **验收**：单体 8 顶点/12 边/6 面，按面边界 24 次使用；每边关联两个不同面。同位独立两体合计 16/24/12，不自动共享。

## 1. Vertex、Edge、Wire、Face、Shell、Solid 分别承担什么职责？

Vertex 记录拓扑端点及几何位置；Edge 表达受限曲线及其端点；Wire 组织边界中的边；Face 由支撑曲面与边界限定；Shell 组织面；Solid 以定向壳限定实体区域。Compound 可以容纳互不连接的形状，并不保证其中内容已连接成一个有效实体。层级不是每层都有固定数量的树：相邻面可以共享同一个 Edge。

## 2. 为什么遍历出现次数与去重后的对象数量不同？

从每个面分别遍历边，同一条共享边会出现多次。长方体 6×4=24 次边使用，但只有 12 条唯一边。实验用 `TopExp::MapShapes` 建立基于身份的索引，再单独遍历每个 Face 记录使用实例，不能用一次去重表解释全部方向。框架统计已封装便利接口，本周代码重新显式展示计数过程。

## 3. 共享拓扑对象与坐标重合有什么区别？

普通 TopoDS 赋值共享底层 TShape，不是深复制；两次 MakeBox 即使尺寸和位置一样，也会生成不同拓扑。第二、三阶段分别用 Compound 容纳两个引用或两个独立模型，唯一边分别为 12 和 24。坐标误差小只能说明几何接近，不证明连接或身份相同；这个对照 Compound 也不等于布尔并集。

## 4. IsPartner、IsSame、IsEqual 分别比较什么，方向和位置怎样影响判断？

| 判断 | 比较内容 | 原对象与反向对象 | 原对象与非零平移实例 |
|---|---|---|---|
| IsPartner | TShape | true | true |
| IsSame | TShape、Location | true | false |
| IsEqual | TShape、Location、Orientation | false | false |

默认去重采用 IsSame 的口径；Location 参与判断，Orientation 由使用实例另外保存。把 offset 改为 0 后几何位置重合，但新建零平移 Location 不保证与原来的空 Location 身份相同；实验另列 `empty_location.same=1` 作对照。相同矩阵效果不能直接替代 Location 身份比较，更不能把 IsSame 当成任意几何等价判断。

## 5. 怎样建立边到面的关联查询，并区分使用次数与不同相邻面的数量？

给边和面各建本次运行的索引，遍历每个面的边，将面编号放入该边的集合；另累计使用次数并记录方向。输出的边面表和诊断条目可定位到视窗中的边。测试用 `MapShapesAndUniqueAncestors` 独立复核每边两个不同祖先面。这个“两面”结论限于当前封闭长方体；开放边、非流形模型或周期接缝不保证成立。

## 代码与执行轨迹

[experiments/course.cpp](experiments/course.cpp) 保留四阶段：[tests/course_tests.cpp](tests/course_tests.cpp) 验证它们。

```text
01 单一长方体：8 / 12 / 6，按面使用 24 次，每边两个不同面
02 普通赋值：8 / 12 / 6，IsSame=true
03 同位独立构造：16 / 24 / 12，IsPartner=false
04 反向：partner/same/equal=1/1/0；非零平移实例=1/0/0
```

第一阶段底部列出边面邻接及方向，点选诊断条目突出显示对应边；阶段树保留完整拓扑结构。编号只在本次运行有效，不用于长期特征命名，也不保证跨运行排序不变。

## 独立构建与运行

使用 **C++23、import std、Clang + CMake + Ninja + clangd**。本周有完整框架、构建与 IDE 配置，只共用第三方 `../../occt`；详见 [tools/README.md](tools/README.md)。在本周目录执行：

```bash
./run.sh
./run.sh --headless --param length=60 --param offset=0
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
./build/demo --headless --output build/results/topology
```

默认实验 `topology-sharing`。改变长宽高不改变长方体计数；把 offset 改为 0，对照几何重合、显式 Location 身份与空 Location。默认大小依旧是 40×30×20 mm。输入与生成都在本周，不需要上一周模型文件。

下一周研究[边界、方向与 pcurve](../week04/README.md)，观察一条边在同一个周期面中使用两次的例外。返回[课程目录](../../README.md)。
