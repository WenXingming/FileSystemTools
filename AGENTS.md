# Repository Guidelines

## Project Structure & Module Organization

Production code lives under `src/` and implements parallel duplicate-file discovery. Tests live under `test/`. Performance experiments belong in `benchmark/`; design notes and measured results belong in `docs/`. Keep images and other documentation assets in `assets/`.

When adding a feature, place reusable logic in the tool's library target (`dedup_lib`) and keep `main.cpp` limited to argument parsing and orchestration.

## Build, Test, and Development Commands

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The first command configures the C++11 project and fetches missing
dependencies. The second builds the tool and tests; the third runs GTest and
CLI integration tests.

Run a local scan with:

```bash
./build/src/dedup /path/to/data --threads 4
```

Benchmarks are opt-in:

```bash
cmake -S . -B build-bench -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench -j
```

## Coding Style & Naming Conventions

Use four-space indentation and C++11 unless a target explicitly requires a
newer standard. Match existing conventions: `PascalCase` for types and source
files (`DuplicateFinder.cpp`), `snake_case` for functions, and trailing
underscores for data members (`pool_`). Prefer RAII, const-correctness, and
small functions with explicit ownership. There is no enforced formatter, so
avoid unrelated formatting changes.

## Testing Guidelines

Unit tests use GoogleTest and are named `*Tests.cpp`. Add focused tests beside
the corresponding module and describe behavior in test names. Shell-based CLI
tests must create isolated temporary data and avoid modifying user files.
Run the complete `ctest` command before submitting. Use Google Benchmark only
for repeatable performance claims and record relevant environment details.

## Commit & Pull Request Guidelines

Recent commits use concise subjects with prefixes such as `Fix`, `Update`,
`Refactor`, and `Optimization`. Follow that pattern, for example:
`Fix dedup hardlink accounting`. Keep commits scoped to one logical change.

Pull requests should explain the problem, implementation choice, and commands
used for verification. Link related issues, include benchmark before/after
results for performance changes, and update `README.md` or `docs/` when
behavior or CLI usage changes.
