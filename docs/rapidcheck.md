# RapidCheck：超大文件 SHA-256 校验器的性能分析与优化决策

## 项目要解决什么问题

RapidCheck 是一个面向超大文件的完整性校验工具。典型场景是：用户下载了一个 20 GB 以上的 ISO、模型权重或归档文件，希望计算 SHA-256，并与官网提供的摘要比较，确认文件没有损坏或被篡改。

命令行支持直接输出摘要，也支持传入期望值完成校验：

```bash
rapidcheck <FILE_PATH>
rapidcheck <FILE_PATH> --expect <SHA256>
```

当前核心流程非常直接：

1. 打开目标文件并获取大小。
2. 分配一次 1 MiB 用户态缓冲区。
3. 顺序执行 `read()`。
4. 将每次读到的数据交给 OpenSSL EVP SHA-256 上下文。
5. 更新进度，文件结束后输出摘要或校验结果。

这个场景有三个重要约束：

- 摘要必须与官网标准 SHA-256 完全一致。
- 文件可能远大于物理内存，不能一次性载入。
- 校验首先必须正确，然后才讨论性能。

因此，本项目的性能目标不是追求一个自定义的“快速摘要”，而是在保持标准 SHA-256 兼容性的前提下，判断还能否缩短单个大文件的首次校验时间。

## 多线程分块 Hash ❌

当处理数量很多的文件时，可以通过多线程并行校验不同文件来充分利用 CPU，这也是 Dedup 的第一步中采用的多线程性能优化方案。

对于单个大文件，我们的初始设想也是能否利用多线程并行加速。最初的设想是多线程分块 Hash，即把单个大文件切成 N 个（线程池大小）数据块，每个线程独立计算一个块的 SHA-256，然后在主线程合并结果。

```text
线程 0 -> Hash(chunk 0)
线程 1 -> Hash(chunk 1)
线程 2 -> Hash(chunk 2)
...
主线程 -> 合并各块摘要
```

这个方案可以实现并行计算，但不能得到标准 SHA-256，因为 SHA-256 的内部状态严格**依赖**前一个压缩块：

```text
State(n) = Compress(State(n - 1), Block(n))
```

线程独立计算 `SHA256(chunk n)` 时缺少前一块产生的内部状态。之后无论拼接摘要还是再次 Hash，得到的都是自定义分块摘要，无法与官网 SHA-256 比较。支持树形 Hash 的 BLAKE3 或自定义 Merkle Tree 可以并行分块，但它们改变了摘要定义，不符合本项目的兼容性要求。

这一步得到的第一个重要结论是：对于各自独立的场景，我们可以使用多线程并行加速计算来充分利用 CPU。对于各个计算阶段依赖前一个阶段结果的场景，多线程并行计算就不适用了。单个文件的标准 SHA-256 不能通过多个独立 Hash 线程并行计算，因为每个压缩块的计算依赖前一个块的内部状态。独立分块再合并会改变算法定义，无法匹配标准 SHA-256。

## I/O 与 Hash 流水线

虽然 SHA-256 本身必须顺序执行，但文件读取与 Hash 属于不同阶段：

```text
I/O 线程   -> 准备下一个缓冲区
Hash 线程  -> 顺序处理当前缓冲区
```

只要 Hash 线程按文件顺序调用 `SHA256_Update()`，最终摘要仍然正确。使用两个预分配缓冲区，就可以构造生产者和消费者流水线：

```text
时间 ------------------------------------------------------------>

I/O:    [读取块 0][读取块 1][读取块 2][读取块 3]
Hash:              [Hash 0]   [Hash 1]   [Hash 2]
```

流水线的理论收益解释：设每块读取耗时为 `R`，哈希耗时为 `H`，一共 `N` 块。

无流水线情况下理论上完全串行的耗时是：

```text
Tserial = Tread + Thash = N × (R + H)
```

理想流水线的耗时下限是：

```text
Tpipeline_min ≈ R + H + (N - 1) × max(R, H) ≈ N × max(R, H)
```

文件足够大时，加速比接近：

```
Speedup = (R + H) / max(R, H)
```

因此流水线收益最大的条件是：读取耗时 ≈ 哈希耗时。如果一个阶段明显更慢，另一个阶段即使完全隐藏，也改变不了最终吞吐。

但这只是理论模型。Linux 本身存在 Page Cache、readahead、异步块设备请求和 DMA，单线程程序也可能已经获得部分 I/O 与计算重叠。因此，我们没有直接实现线程和队列，而是先建立 benchmark，回答“如果使用流水线，现在到底还剩多少可优化空间”。

## 先设计 Benchmark 分析优化潜力

### BenchMark 设计

我们把性能问题拆成四个可验证的问题：

1. 当前 CPU 的纯 SHA-256 上限是多少？
2. HDD 和 SSD 的冷缓存顺序读取速度是多少？
3. 当前“读取 + Hash”的端到端速度是多少？
4. 端到端时间距离理想流水线下限还有多远？

为此建立三个 benchmark：

| Benchmark              | 测量内容                            |
| ---------------------- | ----------------------------------- |
| `BM_Sha256Hash_Memory` | 纯内存数据的 SHA-256 吞吐           |
| `BM_FileRead_Only`     | 文件读取、Page Cache 和用户态拷贝   |
| `BM_Sha256Hash_Disk`   | 文件读取加标准 SHA-256 的端到端吞吐 |

这三个指标不能混为一谈。尤其是 `BM_FileRead_Only` 并非“纯物理磁盘时间”，它还包含文件系统处理以及 Page Cache 到用户缓冲区的复制。

> [!important]
>
> 控制 Page Cache。如果先运行 `ReadOnly`，同一个文件会进入 Page Cache，后面的 `Read + Hash` 就可能变成热缓存测试。因此缓存敏感的 benchmark 必须：
>
> - 通过 `--benchmark_filter` 在独立进程中运行。
> - 在冷缓存测试前请求驱逐目标文件页面。
> - 使用 `fincore` 观察文件驻留情况。
>
> RapidCheck benchmark 提供：
>
> ```bash
> --evict-file-cache
> ```
>
> 它在计时前调用：
>
> ```cpp
> posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
> ```
>
> 该调用只请求内核丢弃目标文件的干净缓存页，不会清理整个系统缓存，也不会永久关闭 Page Cache。它是 advisory，因此文中使用“冷缓存启动”，而不是“禁用缓存”。

### 测试环境

| 项目         | 配置                                   |
| ------------ | -------------------------------------- |
| CPU          | Intel Xeon Silver 4214R，12 核 24 线程 |
| CPU 频率     | 2.40 GHz，最高 3.50 GHz                |
| 内存         | 64 GiB                                 |
| SHA-256 实现 | OpenSSL 3.0.2 EVP                      |
| 构建类型     | Release                                |
| 文件大小     | 29,837,778,925 bytes，约 27.789 GiB    |
| HDD          | TOSHIBA DT01ACA200，NTFS3              |
| SSD          | SAMSUNG MZ7LN1T0HAJQ，ext4             |
| 应用缓冲区   | 1 MiB                                  |

Google Benchmark 提示 CPU scaling enabled，因此个位数差异需要谨慎解释。本文关注瓶颈、数量级和理论上限，不把单次小幅波动当成确定收益。

### 建立性能模型

定义：

```text
Tread    = 读取整个文件的时间
Thash    = 按纯 Hash 吞吐处理相同字节数的时间
Tcurrent = 当前读取加 Hash 的端到端时间
```

理想流水线下限：

```text
Tpipeline_min = max(Tread, Thash)
```

相对于当前实现的乐观剩余空间：

```text
PipelineHeadroom = Tcurrent - Tpipeline_min
```

乐观最大加速比：

```text
Speedup_max = Tcurrent / Tpipeline_min
```

### 更准确的三阶段模型

普通 buffered I/O 实际包含三个成本：

```text
D = 块设备把文件送入 Page Cache 的时间
C = 文件系统、read() 和 Page Cache 到用户缓冲区的 CPU 工作
H = SHA-256 的 CPU 工作
```

数据路径是：

```text
块设备 --DMA/驱动--> Page Cache --copy_to_user--> 1 MiB 用户缓冲区
                                              |
                                              +--> SHA-256
```

Linux 检测到顺序读取后，会提前向块设备提交后续页面请求。因此，即使用户代码写的是：

```text
read(chunk 0) -> hash(chunk 0) -> read(chunk 1) -> hash(chunk 1)
```

磁盘仍然可以在 CPU 处理当前块时，把后续块读入 Page Cache。

当前单线程实现的稳态时间更接近：

```text
Tread    ~= max(D, C)
Tcurrent ~= max(D, C + H)
```

`C + H` 仍然相加，因为同一个用户线程必须等 `read()` 完成拷贝，随后才能 Hash 当前缓冲区。内核 readahead 重叠的是设备阶段 `D` 与 CPU 阶段 `C + H`。

#### 与 `BM_FileRead_Only` 的区别

`BM_FileRead_Only` 只执行读取，不执行 Hash：

```text
BM_FileRead_Only    = D + C 路径
BM_Sha256Hash_Disk = D + C + H 路径
```

这里的 `Tread` 不是单独的物理设备时间 `D`。它同时包含 Page Cache、文件系统、`read()` 系统调用以及 `copy_to_user` 的成本 `C`。由于 readahead 已经让 `D` 与 `C` 部分重叠，纯读取的 Wall Time 更接近：

```text
Tread ~= max(D, C)
```

加入 Hash 后，readahead 仍可让设备阶段 `D` 在后台运行，但当前用户线程必须依次完成 `C` 和 `H`：

```text
Tcurrent ~= max(D, C + H)
```

所以，`BM_FileRead_Only` 的作用是测量完整的 buffered read 路径，而不是直接给出一个能够被 readahead 全部隐藏的纯磁盘时间。仅凭 `Tread < Thash`，不能推导出当前单线程必然达到 `Thash`。

理想双线程流水线会进一步尝试把 `C` 与 `H` 放到不同线程：

```text
Tpipeline_min ~= max(D, C, H)
              ~= max(Tread, Thash)
```

这个下限忽略线程同步、缓冲区迁移、内存带宽竞争以及流水线填充和排空，因此只能用来判断“值不值得实验”，不能当成预期结果。

> [!tip]
>
> 决策门槛。为了避免为了几个百分点引入并发复杂度，我们预先设置门槛：
>
> - 理论剩余收益低于 5%：不实现流水线原型。
> - 理论剩余收益达到 5%：最多先做 benchmark-only 双缓冲原型。
> - 原型摘要必须与顺序 SHA-256 完全一致。
> - 原型稳定提升超过 5%：才考虑修改生产端 `FileVerifier`。

## 实测结果

### 汇总

纯内存 SHA-256 吞吐为：

```text
386.618 MiB/s
```

处理 27.789 GiB 文件的理想纯 Hash 时间为：

```text
Thash = 27.789 GiB / 386.618 MiB/s = 73.601 s
```

冷缓存测试汇总：

| 设备 |    纯读取 |      读取吞吐 | 读取 + Hash |    端到端吞吐 | 主要约束 |
| ---- | --------: | ------------: | ----------: | ------------: | -------- |
| HDD  | 215.535 s | 132.023 MiB/s |   214.512 s | 132.652 MiB/s | I/O      |
| SSD  |  55.072 s | 516.701 MiB/s |    83.597 s | 340.391 MiB/s | CPU/Hash |

CPU Time：

| 设备 | `BM_FileRead_Only` | `BM_Sha256Hash_Disk` |
| ---- | -----------------: | -------------------: |
| HDD  |           27.563 s |            158.501 s |
| SSD  |           25.370 s |             83.567 s |

### HDD：I/O 约束且没有剩余流水线空间

HDD 数据：

```text
Tread    = 215.535 s
Thash    = 73.601 s
Tcurrent = 214.512 s
```

如果设备读取与 Hash 完全不重叠，理论时间应为：

```text
Tfully_serialized = 215.535 + 73.601
                  = 289.136 s
```

但实测端到端时间约为 214.5 秒，已经接近纯读取时间。原因不是 Hash 没有成本：

- `BM_FileRead_Only` 的 CPU Time 为 27.563 秒。
- `BM_Sha256Hash_Disk` 的 CPU Time 增加到 158.501 秒。
- 增加的 CPU 工作落在更长的物理磁盘时间窗口内。

HDD 每秒只能生产约 132 MiB 数据，而 CPU 理想情况下每秒可 Hash 约 387 MiB。CPU 处理当前块时，readahead 和磁盘继续准备后续块，最终吞吐由 HDD 决定。

`Tcurrent` 比独立测得的 `Tread` 还少 1.023 秒，差异约 0.48%。流水线不可能比纯读取更早完成，因此这是机械盘和系统状态波动。对 HDD 来说：

```text
PipelineHeadroom ~= 0
```

额外增加用户态 I/O 线程无法突破磁盘吞吐，也只会重复内核已经完成的重叠。

### SSD：Hash 约束，但理论收益仍然有限

SSD 数据：

```text
Tread    = 55.072 s
Thash    = 73.601 s
Tcurrent = 83.597 s
```

完全串行模型：

```text
Tfully_serialized = 55.072 + 73.601
                  = 128.673 s
```

实际端到端只用了 83.597 秒，说明 Linux 已经隐藏了：

```text
128.673 - 83.597 = 45.076 s
```

理想流水线下限由 Hash 决定：

```text
Tpipeline_min = max(55.072, 73.601)
              = 73.601 s
```

#### 为什么启用 readahead 后仍然是 83.597 秒

SSD 的每 MiB 耗时约为：

```text
冷缓存读取：     1 / 516.701 s = 1.94 ms/MiB
理想纯 Hash：    1 / 386.618 s = 2.59 ms/MiB
实际读取 + Hash：1 / 340.391 s = 2.94 ms/MiB
```

readahead 已经把大部分物理设备读取 `D` 隐藏在 Hash 期间，但它只负责提前把数据送入 Page Cache。当前线程仍需同步执行：

```text
read()/copy_to_user(C) -> SHA-256(H)
```

因此，当前单线程不是单独执行 `H`，而是执行 `C + H`。累计整个文件后，每 MiB 约 `0.35 ms` 的 CPU 路径差异形成了约 10 秒额外时间。

这组数据也支持该判断：

```text
Wall Time = 83.597 s
CPU Time  = 83.567 s
```

二者几乎相同，说明 readahead 已经避免了明显的 SSD 等待，当前线程基本一直在执行 CPU 工作。83.597 秒与 73.601 秒的差距并不等同于 10 秒物理 I/O 等待。

显式双缓冲流水线可能让 I/O 线程在另一个 CPU 核执行 `C`，同时让 Hash 线程执行 `H`，所以理论上可以尝试接近 `max(D, C, H)`。但 73.601 秒来自短时间、重复 16 MiB 缓冲区的理想纯内存 benchmark；如果长期流式 SHA-256 本身就比这个结果慢，流水线无法回收这部分差距。

因此，在实现流水线前，更严谨的下一项测量应是增加一个与 `FileVerifier` 相同 1 MiB 更新粒度、运行时间足够长的纯内存分块 Hash benchmark。它可以先验证 73.601 秒是否是可靠的 Hash 下限。

当前实现距离该乐观下限约：

```text
83.597 - 73.601 = 9.996 s
```

对应：

```text
乐观最大加速比 = 83.597 / 73.601 = 1.136
乐观最大加速   = 13.6%
当前耗时最多减少约 12.0%
```

这 10 秒不能全部归因于“缺少用户态流水线”，它还可能来自：

- Page Cache 到用户缓冲区的复制。
- 文件系统和系统调用的 CPU 工作。
- 长时间运行时的 CPU 频率变化。
- 读取与 Hash 竞争缓存和内存带宽。
- 纯内存 benchmark 重复处理 16 MiB 缓冲区，`Thash` 偏理想化。

双缓冲可以尝试让 I/O 线程准备下一块，同时让 Hash 线程处理当前块，但同步和缓冲区跨核迁移也会产生新成本。因此 13.6% 是上限，不是收益承诺。

### 热缓存：进一步确认 SHA-256 接近上限

文件完全驻留 Page Cache 时，实测：

```text
纯 SHA-256：       385.620 MiB/s
读取 + SHA-256：   363.534 MiB/s
Wall Time：         78.275 s
CPU Time：          78.271 s
```

Wall Time 与 CPU Time 几乎相同，说明热缓存场景没有明显 I/O 等待。端到端已经达到纯 Hash 吞吐的约 94%，理想情况下最多再节省约 4.7 秒，理论加速约 6%。这进一步说明，单个文件标准 SHA-256 的主要限制已经是算法和当前 CPU，而不是文件读取代码。

## 最终优化决策

### 没有把线程池和内存池加入生产代码

测试结果不支持为当前单文件场景直接引入完整并发架构：

- HDD 冷缓存几乎没有剩余空间。
- SSD 冷缓存的乐观耗时下降约 12%，实际收益只会更低。
- 热缓存的理论加速约 6%。
- 标准 SHA-256 仍然只能由一个 Hash 线程顺序更新。
- 当前实现没有每块动态分配，内存池解决不了真实瓶颈。
- 并发实现还要处理顺序、背压、异常传播、停止和线程同步。

按照实验门槛，SSD 数据足以支持一个隔离的 benchmark-only 双缓冲原型；但它尚不足以证明应该修改生产实现。当前阶段选择保留简单、正确、可维护的顺序代码。

### “没有大幅优化”也是有效结果

这次工作的价值不是成功加上两个线程，而是完成了一次可证伪的性能分析：

1. 识别并否定了错误的 SHA-256 分块并行假设。
2. 提出不改变摘要语义的 I/O + Hash 流水线。
3. 在编码前建立可分解瓶颈的 benchmark。
4. 控制 Page Cache，分别测试 HDD、SSD 和热缓存。
5. 建立 `D/C/H` 模型解释内核隐式流水线。
6. 用数据判断收益上限，避免为很小的收益增加生产复杂度。

性能优化并不要求每次都改代码。证明当前实现已经接近设备或算法上限，并据此停止错误方向，同样是工程成果。

## 7. 更有价值的后续方向

如果目标是显著改善 RapidCheck 的整体体验，应该改变优化层次，而不是继续挤压当前循环。

### 7.1 整文件结果缓存

对重复校验，可以按文件身份和元数据保存上次 SHA-256：

```text
device + inode + size + mtime + ctime -> SHA-256
```

缓存命中时可以把几十秒降低到毫秒级。需要明确缓存适用的是可信本地工作流；安全敏感校验仍应允许强制重新读取。

### 7.2 多文件并行

单个 SHA-256 流不能并行，但多个文件互相独立。线程池更适合：

```text
线程 0 -> file A
线程 1 -> file B
线程 2 -> file C
```

这能让批量校验真正使用多核。对于机械盘还要限制并发，避免随机寻道降低吞吐。

### 7.3 可选 BLAKE3

如果业务不要求与官网 SHA-256 兼容，可以提供 BLAKE3 模式。BLAKE3 原生使用树形结构，能够并行处理一个大文件。

它应作为另一种明确命名的算法，而不是伪装成 SHA-256 优化。

### 7.4 小范围底层实验

仍可通过 benchmark 独立比较：

- `read()` 与 `mmap()`。
- 1、4、8、16 MiB 缓冲区。
- 双缓冲 benchmark-only 原型。
- 不同 OpenSSL 或 SHA-256 后端。

这些实验预期是个位数到十几个百分点，必须先设收益门槛，再决定是否进入生产代码。

## 8. 面试表达参考

### 8.1 一分钟版本

> 我实现了一个超大文件 SHA-256 校验器，最初想通过线程池把文件分块并行 Hash。但分析算法后发现标准 SHA-256 的内部状态严格顺序依赖，独立分块再合并无法得到官网摘要，所以我把优化方向调整为 I/O 与 Hash 双缓冲流水线。
>
> 我没有马上写并发代码，而是先建立三组 benchmark，分别测纯 Hash、纯读取和读取加 Hash，并通过 `POSIX_FADV_DONTNEED` 与独立进程控制 Page Cache。结果显示，29.8 GB 文件在 HDD 上纯读取约 215 秒，端到端也是约 215 秒，说明 Linux readahead 已经把 Hash 隐藏在磁盘读取期间；SSD 上端到端 83.6 秒，理论下限约 73.6 秒，最多只有约 12% 的耗时下降空间。
>
> 最终我没有为了小收益直接引入线程池和内存池，而是保留了简单正确的实现，并把后续重点转向整文件缓存、多文件并行和可选 BLAKE3。这个过程让我完整实践了从假设、测量、建模到技术决策的性能优化闭环。

### 8.2 面试官可能追问

**为什么不能多线程计算一个标准 SHA-256？**

因为每个压缩块依赖前一个块的内部状态。独立计算每块摘要再合并会改变算法定义，无法匹配标准 SHA-256。

**为什么单线程代码的时间不是 `Tread + Thash`？**

Linux readahead 会异步读取后续页面。用户线程 Hash 当前缓冲区时，块设备仍在向 Page Cache 填充后续数据，因此设备 I/O 与 CPU 工作已经重叠。

**`read()` 不还是要从内核拷贝到用户态吗？**

需要。这个成本记作 `C`，并且已经包含在 `BM_FileRead_Only` 中。当前单线程更接近 `max(D, C + H)`；双线程只能尝试把 `C` 与 `H` 进一步重叠。

**为什么没有实现理论上可行的流水线？**

HDD 场景没有剩余空间，SSD 和热缓存场景只有有限的乐观上限。并发还会增加同步、顺序和异常处理复杂度，所以生产实现需要实测稳定收益，而不是只凭理论可行。

**这个项目最终优化了什么？**

首先优化了决策质量：排除了错误方向，证明当前单文件首次校验接近系统上限。产品层面更值得做的是重复校验缓存和多文件并行，它们比挤压当前核心循环更可能获得数量级收益。

## 9. Benchmark 复现

构建：

```bash
cmake -S . -B build \
  -DBUILD_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --target rapidcheckBenchmark -j4
```

以下命令假设当前目录是 `build/benchmark/rapidcheck`。

纯 Hash：

```bash
./rapidcheckBenchmark \
  --benchmark_filter=BM_Sha256Hash_Memory \
  <FILE_PATH>
```

冷缓存纯读取：

```bash
./rapidcheckBenchmark \
  --evict-file-cache \
  --benchmark_filter=BM_FileRead_Only \
  <FILE_PATH>
```

冷缓存读取加 Hash：

```bash
./rapidcheckBenchmark \
  --evict-file-cache \
  --benchmark_filter=BM_Sha256Hash_Disk \
  <FILE_PATH>
```

热缓存读取加 Hash：

```bash
fincore --bytes <FILE_PATH>

./rapidcheckBenchmark \
  --benchmark_filter=BM_Sha256Hash_Disk \
  <FILE_PATH>
```

## 10. 测试限制

- `POSIX_FADV_DONTNEED` 是 advisory，成功返回不代表所有页面必然立即驱逐。
- 单次 29.8 GB 测试成本较高，目前没有为每个设备执行多次统计重复。
- CPU scaling enabled 会带来频率和耗时波动。
- `BM_Sha256Hash_Memory` 重复使用 16 MiB 缓冲区，代表理想 Hash 上限，不完全等同于长期流式文件负载。
- `Time` 是端到端判断的主要指标；`CPU` 不能直接等同于纯 Hash 时间。
- SSD 的约 12% 是乐观时间上限，不能替代流水线原型的实测。
