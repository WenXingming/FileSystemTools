# Dedup ⚡

<p align="center">
  <strong>一个基于 ThreadPool 的高性能、支持多线程并行的文件去重命令行工具</strong><br />
  针对 SSD/NVMe 的并发优势与 HDD 的寻道惩罚，并为硬链接、大文件、小文件设计了多层过滤漏斗，在尽可能减少磁盘 I/O、降低磁盘压力的前提下极速找出重复文件。
</p>

<p align="center">
  <img src="https://img.shields.io/badge/core-C%2B%2B11-00599C?style=flat-square" alt="C++11" />
  <img src="https://img.shields.io/badge/build-CMake%203.16%2B-064F8C?style=flat-square" alt="CMake" />
  <img src="https://img.shields.io/badge/library-dedup__lib-0A7F5A?style=flat-square" alt="dedup_lib" />
  <img src="https://img.shields.io/badge/tests-GoogleTest-7B42BC?style=flat-square" alt="GoogleTest" />
  <img src="https://img.shields.io/badge/benchmark-Google%20Benchmark-E91E63?style=flat-square" alt="Google Benchmark" />
</p>

<p align="center">
  <a href="#核心特性">✨ 核心特性</a> ·
  <a href="#快速开始">🚀 快速开始</a> ·
  <a href="#构建与测试">🛠️ 构建与测试</a> ·
  <a href="#性能基准">📊 性能基准</a> ·
  <a href="#快速集成">🧩 快速集成</a>
</p>

## 核心特性 ✨

* **🚀 极致并发设计**：底层接入轻量级 [ThreadPool](https://github.com/WenXingming/ThreadPool)。哈希计算与文件分块读取紧密流水线化，在大文件并发扫描场景下能够榨干多核 CPU 与 SSD 的并发读写性能。
* **🛡️ Inode 硬链接折叠**：自动识别相同物理 Inode 的硬链接文件，对其进行折叠记录，避免同一文件被反复读取多次，天然免疫硬链接导致的 I/O 放大。
* **⚡ Partial Hash 过滤漏斗**：对于大文件，首先计算头部和尾部少量数据（例如 64KB）的 Partial Hash 进行快速比对，仅在 Partial Hash 完全相同时，才对文件进行全盘 Full Hash 计算，避开 99% 的大文件全盘读取开销。
* **🎯 高性能 BLAKE3 算法**：哈希模块采用非密码学安全但极速、且自带 AVX2/AVX512/SSE 等 SIMD 汇编优化的 BLAKE3 算法，保障吞吐速度的同时杜绝了哈希碰撞风险。
* **🔌 纯净 C++11 构建**：不依赖任何复杂的第三方库（除 GTest 和 Google Benchmark 外部拉取外），且对其他 CMake 项目提供开箱即用的库链接支持。

---

## 快速开始 🚀

### 1. 编译构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 2. 运行命令行去重

```bash
# 默认单线程运行（安全且保护 HDD 机械硬盘）
./build/src/dedup /path/to/directory

# 多线程并发运行（推荐在现代 SSD 上使用，通常开启 CPU 核数的线程）
./build/src/dedup /path/to/directory --threads 8
```

> [!IMPORTANT]
> **物理介质优化建议**：
>
> * **机械硬盘 (HDD)**：请使用默认的单线程模型（不要指定 `--threads` 或设置为 `1`）。多线程的并发随机读取可能会导致磁头剧烈寻道振荡（Disk Thrashing），性能反而可能会受损。
> * **固态硬盘 (SSD/NVMe)**：强烈建议根据你的硬件情况开启并发线程（如 `--threads 8`），这能带来数倍的性能提升。

---

## 构建与测试 🛠️

```bash
# 开启测试并构建
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j

# 运行 GTest 单元测试及 CLI 综合集成测试
ctest --test-dir build --output-on-failure
```

---

## 性能基准 📊

我们编写了用于和业界知名去重工具 `jdupes` 进行宏观性能比对的 benchmark 脚本。详细比对数据和分析可参考：[开源方法对比：dedup_vs_jdupes](docs/开源方法对比：dedup_vs_jdupes.md)。

### 1. 编译 Benchmark 目标

```bash
cmake -S . -B build-bench -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench -j
```

### 2. 运行系统级对比测试 (需提前生成测试集)

```bash
# 生成 2.5 GB 混合测试集，并对比运行 jdupes 和 dedup 性能
# 该脚本会自动下载、构建 jdupes 并输出直观的执行时间与内存 RSS 占用对比表格
./build-bench/benchmark/compare_jdupes
```

### 3. 运行微观性能基准

```bash
./build-bench/benchmark/dedupBenchmark --benchmark_min_time=1.0 --benchmark_repetitions=3
```

---

## 快速集成 🧩

你可以在你的 CMake 项目中，通过 `FetchContent` 极简引入 `dedup_lib` 作为一个去重组件库：

```cmake
include(FetchContent)

FetchContent_Declare(
    dedup
    GIT_REPOSITORY https://github.com/WenXingming/FileSystemTools.git
    GIT_TAG        develop # 生产环境建议指定具体 Commit 或 Release Tag
    SOURCE_SUBDIR  src
)
FetchContent_MakeAvailable(dedup)

# 链接到你的 Target 即可直接使用 DuplicateFinder, FileWalker 与 Hasher
target_link_libraries(your_project PRIVATE dedup_lib)
```
