# 可更新隐私集合求交协议实现

本项目是一个可更新隐私集合求交（Updatable Private Set Intersection, UPSI）协议实验原型，围绕分桶、OKVS 编码和基于 RELIC 的密码学计算进行实现。代码适合用于论文原型复现、协议功能验证和后续工程化改造。

## 项目功能

- 初始轮 PSI：将双方集合按哈希分桶，在每个桶内执行密码学求交流程。
- 更新轮 PSI：维护协议状态，对更新后的桶重新计算，并复用未变化桶中的历史交集结果。
- OKVS 适配层：将宽字节载荷拆分为多个 128-bit block，复用第三方 OKVS 实现完成编码和解码。
- 小规模正确性测试：包含分桶、OKVS 参数策略、初始轮、更新轮和 RELIC 桥接一致性测试。
- 性能统计接口：记录分桶、OKVS 编码、验证、通信量等阶段指标，便于后续实验分析。

## 目录结构

```text
.
├── CMakeLists.txt                  # CMake 构建入口
├── include/                        # UPSI 对外头文件
├── src/                            # 协议、分桶、哈希、OKVS 适配和 RELIC 桥接实现
├── tests/                          # 单元测试和小规模协议测试
├── third_party/okvs/               # 项目内置 OKVS 依赖
├── build_and_test_okvs_adapter.ps1 # Windows/PowerShell 构建与测试脚本
├── run_exp_4_3_2.ps1               # 可选性能实验脚本
├── rebuild_relic_for_upsi.ps1      # 可选 RELIC 构建辅助脚本
└── setup_winlibs_x64_toolchain.ps1 # 可选 Windows x64 工具链准备脚本
```

## 依赖环境

- CMake 3.16 或更新版本。
- 支持 C++17 的 C/C++ 编译器。
- GMP 和 RELIC。未配置 RELIC 时，项目仍可构建 OKVS、分桶等不依赖 RELIC 的基础测试；完整 PSI 协议测试和 benchmark 需要 RELIC。

RELIC 相关路径通过 CMake 参数传入，不再依赖本机绝对路径：

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DUPSI_RELIC_MAIN_DIR=/path/to/relic-main \
  -DUPSI_RELIC_TARGET_DIR=/path/to/relic-build-or-target \
  -DUPSI_RELIC_STATIC_LIB=/path/to/librelic_s.a \
  -DUPSI_GMP_INCLUDE_DIR=/path/to/gmp/include \
  -DUPSI_GMP_LIBRARY=/path/to/libgmp.a
```

## 构建与测试

基础构建：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Windows PowerShell 下也可以使用：

```powershell
.\build_and_test_okvs_adapter.ps1 -BuildType Release
```

如需运行完整协议测试，请先配置 RELIC 和 GMP，再重新执行构建与测试。CMake 会在检测到 `UPSI_RELIC_STATIC_LIB` 后自动启用 `upsi_protocol_initial`、`upsi_protocol_update`、`upsi_relic_bridge` 以及对应测试目标。

## 性能实验

`run_exp_4_3_2.ps1` 用于运行本项目保留的性能实验入口，默认输出到 `experiment_results/`。该目录属于实验生成物，已经加入 `.gitignore`，不建议提交到开源仓库。

示例：

```powershell
.\run_exp_4_3_2.ps1 `
  -Profile quick `
  -Trials 1 `
  -RelicMainDir C:\path\to\relic-main `
  -RelicTargetDir C:\path\to\relic-target `
  -RelicStaticLib C:\path\to\librelic_s.a `
  -GmpIncludeDir C:\path\to\gmp\include `
  -GmpLibrary C:\path\to\libgmp.a
```

## 项目说明
项目名称（project name）：lcupsi
项目作者（Author）：Chao Qi
作者单位（Affiliation）：暨南大学网络空间安全学院（College of Cyber Security, Jinan University）

## 许可证

本项目采用 Apache License 2.0，详见根目录 `LICENSE` 文件。

`third_party/okvs/` 中的 OKVS 代码同样来自 Apache License 2.0 授权项目。再次分发或修改第三方代码时，请保留其原始版权与许可证声明。
