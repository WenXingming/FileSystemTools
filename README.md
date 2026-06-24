# FileSystemTools

文件系统工具集合。当前包含 `dedup`：一个基于 `ThreadPool` 的并行文件去重命令行工具，用于递归扫描目录并输出重复文件组。

`ThreadPool` 不再保留在本项目源码中，而是通过 CMake `FetchContent` 自动从 GitHub 仓库 (https://github.com/WenXingming/ThreadPool.git) 拉取。

## 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 使用方式

```bash
./build/src/dedup/dedup /path/to/dir
./build/src/dedup/dedup /path/to/dir --threads 4
```

输出示例：

```text
Duplicate group #1, size=12, count=2
  /tmp/example/a.txt
  /tmp/example/b.txt

Summary:
  scanned files: 3
  hashed files: 3
  threads: 4
  duplicate groups: 1
  errors: 0
```

## 核心流程

```text
FileWalker -> size grouping -> ThreadPool parallel hash -> DuplicateFinder report
```

- `FileWalker`：递归遍历目录，收集普通文件路径和大小，并记录无法访问的路径错误。
- `size grouping`：先按文件大小分组，跳过不可能重复的唯一大小文件。
- `ThreadPool parallel hash`：对大小相同的候选文件提交并行 hash 任务。
- `DuplicateFinder report`：按 `size + hash` 聚合结果，输出重复文件组、扫描数量、hash 数量和错误数量。

`--threads N` 用于显式控制 hash 阶段的并发度。

## Benchmark

Benchmark 默认不参与普通构建。需要显式开启：

```bash
cmake -S . -B build-bench -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench
./build-bench/benchmark/dedup/dedupBenchmark --benchmark_min_time=1.0 --benchmark_repetitions=3
```

详细基线方法和结果记录模板见 [docs/dedup_benchmark.md](docs/dedup_benchmark.md)。

## 当前边界

- 当前使用 FNV-1a 作为 MVP hash 算法，适合演示和工程流程，不是密码学 hash。
- 当前以 `size + hash` 判断重复文件，后续可以增加字节级确认来规避理论上的 hash 碰撞。
- 当前只做扫描和报告，不执行删除、移动等破坏性操作。
- 当前保持 C++11 标准，不依赖 C++17 文件系统库。
