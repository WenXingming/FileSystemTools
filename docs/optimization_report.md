# 性能优化演进报告 (Optimization Report)

本文档持续记录 `dedup` 工具的性能基线与优化过程。目标不是追求某一次漂亮的数字，而是让性能讨论可复现、可解释、可对比。

---

## 阶段 0：从串行到并行 Hash 的优化基线

### 1. 测试环境与原始数据

**运行时间**: 2026-06-24T14:01:15+08:00
**运行环境**: 24 X 3500 MHz CPUs
**缓存结构**:
- L1 Data: 32 KiB (x12)
- L1 Instruction: 32 KiB (x12)
- L2 Unified: 1024 KiB (x12)
- L3 Unified: 16896 KiB (x1)

#### 原始 Benchmark 结果

```text
---------------------------------------------------------------------------------------------------------------------------
Benchmark                                                                 Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------------------------------------
DuplicateFinder/100_files_4KiB/threads_1/100/4096/1_mean               11.0 ms         2.93 ms            3 bytes_per_second=133.514M/s items_per_second=34.1795k/s
DuplicateFinder/100_files_4KiB/threads_2/100/4096/2_mean               6.54 ms         2.52 ms            3 bytes_per_second=155.253M/s items_per_second=39.7448k/s
DuplicateFinder/100_files_4KiB/threads_4/100/4096/4_mean               3.94 ms         1.95 ms            3 bytes_per_second=200.338M/s items_per_second=51.2865k/s
DuplicateFinder/100_files_4KiB/threads_8/100/4096/8_mean               2.90 ms         1.64 ms            3 bytes_per_second=239.598M/s items_per_second=61.3372k/s
DuplicateFinder/1000_files_4KiB/threads_1/1000/4096/1_mean              119 ms         29.4 ms            3 bytes_per_second=133.253M/s items_per_second=34.1129k/s
DuplicateFinder/1000_files_4KiB/threads_2/1000/4096/2_mean             69.8 ms         24.2 ms            3 bytes_per_second=162.176M/s items_per_second=41.5169k/s
DuplicateFinder/1000_files_4KiB/threads_4/1000/4096/4_mean             42.2 ms         21.4 ms            3 bytes_per_second=183.174M/s items_per_second=46.8925k/s
DuplicateFinder/1000_files_4KiB/threads_8/1000/4096/8_mean             23.8 ms         14.0 ms            3 bytes_per_second=280.15M/s items_per_second=71.7183k/s
DuplicateFinder/10000_files_4KiB/threads_1/10000/4096/1_mean           1027 ms          242 ms            3 bytes_per_second=161.357M/s items_per_second=41.3075k/s
DuplicateFinder/10000_files_4KiB/threads_2/10000/4096/2_mean            577 ms          195 ms            3 bytes_per_second=201.468M/s items_per_second=51.5757k/s
DuplicateFinder/10000_files_4KiB/threads_4/10000/4096/4_mean            355 ms          169 ms            3 bytes_per_second=231.565M/s items_per_second=59.2806k/s
DuplicateFinder/10000_files_4KiB/threads_8/10000/4096/8_mean            217 ms          133 ms            3 bytes_per_second=293.102M/s items_per_second=75.034k/s
DuplicateFinder/1000_files_256KiB/threads_1/1000/262144/1_mean          868 ms         30.5 ms            3 bytes_per_second=8.00046G/s items_per_second=32.7699k/s
DuplicateFinder/1000_files_256KiB/threads_2/1000/262144/2_mean          446 ms         29.1 ms            3 bytes_per_second=8.3974G/s items_per_second=34.3958k/s
DuplicateFinder/1000_files_256KiB/threads_4/1000/262144/4_mean          234 ms         27.3 ms            3 bytes_per_second=8.955G/s items_per_second=36.6797k/s
DuplicateFinder/1000_files_256KiB/threads_8/1000/262144/8_mean          124 ms         21.3 ms            3 bytes_per_second=11.4722G/s items_per_second=46.9901k/s
```
*(注：为保证可读性，上表仅提取了每组测试的 `_mean` 统计平均行，完整的 3 次 iterations 数据请参考原始终端输出)*

### 2. 性能指标参数解析

*   **Time (Wall Clock Time / 真实流逝时间)**：相当于“挂钟时间”。表示从代码执行开始到结束，现实世界中流逝的总时间。
*   **CPU (CPU Time / CPU 运算时间)**：表示 CPU 真正处于忙碌状态，用来执行测试指令的总时间。如果线程因为读取硬盘而休眠挂起，这部分等待时间是不计入 CPU Time 的。
*   **Iterations**：为了让测量结果具备统计学意义，Benchmark 框架自动执行测试核心代码的迭代次数。
*   **UserCounters (自定义计数器)**：
    *   `bytes_per_second`：吞吐量，即每秒处理的字节数。
    *   `items_per_second`：每秒处理的文件个数。

### 3. 核心分析与结论

#### 3.1 巨大的“Time - CPU”差值印证 I/O 密集型特征

对比 `Time` 和 `CPU` 时间可以发现极端的差异，这证明了文件去重扫描是一个**典型的 I/O 密集型（I/O Bound）任务**：

*   **小文件场景（10000 个 4KiB 文件，单线程）**：真实流逝时间为 `1027 ms`，而 CPU 真实计算时间仅 `242 ms`。这说明单线程下约 **76%** 的时间里，CPU 都处于闲置状态，在等待操作系统将文件从磁盘读入内存。
*   **大文件场景（1000 个 256KiB 文件，单线程）**：真实流逝时间为 `868 ms`，CPU 计算时间仅为 `30.5 ms`。因为单文件尺寸增大，磁盘读取开销占主导，此时高达 **96.5%** 的时间都在等待 I/O，CPU 算 Hash 仅是一瞬间的事。

#### 3.2 线程池带来了显著的线性并发加速

由于单线程状态下 CPU 极度空闲，为多线程并发留下了巨大的优化空间。

1.  **I/O 重叠掩盖延迟 (I/O Overlapping)**：当使用多个线程时，一个线程在等待读盘阻塞，操作系统可以立刻切换到另一个线程执行读盘或 Hash 操作。在 8 线程配置下，系统实际上在向底层并发提交多个 I/O 请求。
2.  **硬件与 Page Cache 性能被榨干**：现代 NVMe 固态硬盘和操作系统的 Page Cache 极其擅长处理高并发的读取队列，从而将原本线性的 I/O 耗时“折叠”了。

**数据佐证：**
在 1000 个 256KiB 文件的场景中，开启 8 线程 (`threads_8`) 后，真实耗时 `Time` 从单线程的 `868 ms` 断崖式下降到了 `124 ms`。获得了 **7 倍的真实时间加速**，十分接近 8 线程理论上的 8 倍上限。程序的有效处理吞吐量也达到了 11.4 GB/s。

### 总结

这组基线数据证实了 `dedup` 工具初步架构设计的正确性：**通过引入 `ThreadPool` 实现并发架构，极大地隐藏了文件读取带来的底层磁盘 I/O 延迟，将文件系统扫描任务的吞吐率和执行速度提升了近乎线性倍数。**

基于这组结果，暴露出了我们下一步需要攻克的瓶颈，例如 **many-small-files 的任务粒度过细导致调度开销占比过大** 等问题。这也直接促成了后续微步开发计划的实施。

---

## Benchmark 运行与解读指南

### 构建测试环境

Benchmark 默认不参与普通构建。需要显式开启，并建议使用 Release 模式：

```bash
cmake -S . -B build-bench -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release -DTHREADPOOL_SOURCE_DIR=/home/wxm/ThreadPool
cmake --build build-bench
```

### 正式运行命令

用于人工阅读基线结果时，使用：

```bash
./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true
```

需要机器可读结果时，使用：

```bash
./build-bench/benchmark/dedup/dedupBenchmark \
  --benchmark_min_time=1.0 \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_format=json \
  > benchmark/dedup/results/latest.json
```

### 如何解读结果

- **优先比较 real time**。它最接近用户实际感受到的运行时间。
- **再比较 CPU time**。如果 real time 下降但 CPU time 上升，说明更多 worker 可能在帮助重叠 I/O 等待，同时也意味着更多的线程调度开销。
- **重点观察 `4 -> 8` 的变化**。如果 real time 几乎不再下降，甚至变慢，说明 workload 可能已经碰到 I/O、调度或任务管理瓶颈。
- **小文件场景可能扩展性较差**，因为 open/close、目录遍历、任务提交和 future 收集开销可能超过实际 hash 工作。
- **中等文件场景通常更能体现 hash 吞吐**，但最佳线程数仍取决于存储设备和缓存状态。

### 优化决策参考

使用 benchmark 基线决定下一步工程优化方向：

- 如果 many-small-files 表现较弱：考虑将多个文件合并为一个任务，减少任务提交和 future 管理开销。
- 如果 medium-file-hash 表现较弱：比较不同 Hasher buffer size 或验证分配开销（例如改为复用 Buffer）。
- 如果所有 workload 在不同线程数下耗时接近：优先检查 FileWalker 和结果聚合逻辑，而不是直接修改线程池。

---

## 下一步阶段计划预告

我们目前的进展已进入 **阶段 1：全面补全高级 Benchmark 和指标体系**。
我们已经成功向基准测试框架注册了更复杂的针对性场景（如 `many_small_duplicates`, `large_same_size_same_prefix_different_tail`, `mixed_realistic`），接下来本报告将会持续记录针对上述瓶颈进行的系统化算法升级。

## 阶段 2：Hasher 低风险优化（消除内存强行清零开销）

### 1. 优化背景

在小文件基准测试中（特别是 1 万个文件的场景），原本单线程耗时达到 ~1400 毫秒。通过源码审查，发现一个隐藏极深的底层瓶颈：
在 `Hasher::compute_md5` 中，每次计算哈希前都会执行 `std::vector<char> buffer(bufferSize_);`。
由于默认 `bufferSize_` 是 8 MiB，这就意味着对于每一个要计算哈希的文件，不管它有多小（哪怕只有 4KiB），程序都会申请 8 MiB 内存并**由 C++ `std::vector` 机制强制进行全量清零（Zero-initialization）**。
处理 1 万个小文件时，仅仅因为这行代码，系统就被迫执行了惊人的 **80 GiB 内存的无意义清零操作**！

### 2. 实施改动

- **干掉强行清零**：将 `std::vector<char>` 替换为更轻量级的现代 C++ 智能指针 `std::unique_ptr<char[]> buffer(new char[bufferSize_]);`。`new char[]` 会执行默认初始化，而不会隐式清零。
- **错误信息强化**：利用 `std::strerror(errno)` 将底层文件读写失败的具体系统错误抛给上层应用，提升了错误排查能力。

### 3. 性能突破（微步验证）

重跑基准测试（`many_small_unique_same_size`，单线程，处理 1 万个 4KiB 文件）：
* **优化前（阶段 1 基准）**：1402 ms
* **优化后（取消清零）**：820 ms

**结论**：仅仅通过改变一行缓冲区的内存分配方式，我们就节省了惊人的 **~580 毫秒（提速 41%）**。并且它一并砍掉了海量无意义的内存带宽占用，为后续真正的并发优化扫清了隐藏的地雷。
