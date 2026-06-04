cpp-lru-cache
=============

基于哈希表和双向链表的 C++ LRU（最近最少使用）缓存实现，简单可靠。该库为 header-only 设计，包含简单的测试和示例。
使用标准组件和少量自定义逻辑，保证可靠性。

示例:
--------

### LRU 缓存（header-only）

```
/** 创建一个最大容量为 3 的缓存。当达到容量上限时，
    新元素将替换最近最少使用的元素。 */
cache::lru_cache<std::string, std::string> cache(3);

cache.put("one", "one");
cache.put("two", "two");

const std::string& from_cache = cache.get("two");
```

### 页面置换模拟器

模拟四种页面置换算法（LRU 计数器、LRU 栈、额外引用位、二次机会），
在多种页面引用序列上运行，并以表格和 ASCII 条形图输出结果。

支持三种运行模式：

**模式 1 — 单个 trace 文件**（支持 `.gz` 压缩文件和十六进制地址 trace）：
```
cd build
make page_simulator
./page_simulator --file traces/gcc.log
./page_simulator --file traces/emacs.gz --frames 4,8,16,32 --pagesize 4096
```

**模式 2 — 内置合成 trace**（Classic、TightLoop、Program、Locality、Sequential、Random）：
```
./page_simulator                        # 快速子集
./page_simulator --benchmark            # 完整基准测试
./page_simulator --benchmark --pages 5000 --space 50
```

**模式 3 — 批量 trace 目录**（文件夹内所有 `.gz` 文件）：
```
./page_simulator --tracedir /path/to/traces
./page_simulator --tracedir /path/to/traces --limit 100000  # 快速测试
```

全部选项：`--file`、`--tracedir`、`--pagesize`、`--frames`、`--benchmark`、`--pages`、`--space`、`--limit`、`--help`。

如何运行测试:
--------

### 原始 LRU 缓存单元测试

```
mkdir build
cd build
cmake ..
make check
```

### 页面置换模拟器

构建模拟器：

```
cd build
make page_simulator
```

#### 模式 1：单个 Trace 文件

从文件中读取页面 trace（支持 `.gz` 压缩文件和十六进制地址 trace）：

```
./page_simulator --file traces/gcc.log
./page_simulator --file traces/emacs.gz
./page_simulator --file traces/gpp.gz --frames 4,8,16,32 --pagesize 4096
```

#### 模式 2：内置合成 Trace

运行所有内置 trace（Classic1/2、TightLoop、Program、Locality、Sequential、Random）：

```
./page_simulator --benchmark
./page_simulator --benchmark --pages 5000 --space 50
```

不加 `--benchmark` 时，运行一个快速内置子集：

```
./page_simulator
./page_simulator --pages 10000 --space 100
```

#### 模式 3：批量 Trace 目录

运行目录中的所有 `.gz` trace 文件：

```
./page_simulator --tracedir /path/to/traces
./page_simulator --tracedir /path/to/traces --pagesize 4096 --frames 8,16,32,64,128
./page_simulator --tracedir /path/to/traces --limit 100000    # 限制引用数，快速测试
```

#### 全部选项

```
--file <path>        从文件读取页面 trace
--tracedir <path>    从目录读取所有 .gz trace 文件
--pagesize <n>       页大小（字节），默认 4096
--frames <n1,...>    帧数列表（默认自动缩放）
--benchmark          运行所有内置 trace
--pages <n>          合成 trace 的引用数（默认 1000）
--space <n>          合成 trace 的最大页面号（默认 30）
--limit <n>          限制每个 trace 的引用数（默认无限制）
--help               显示帮助信息
```


