# C Experiments

All experiments using the native TidesDB C API.

## Prerequisites

- **TidesDB** installed (built from source or via package manager)
  - See [TidesDB build instructions](https://github.com/tidesdb/tidesdb)
- **C compiler**: gcc or clang
- **Make**: GNU Make

## Building

Build all experiments from this directory:

```bash
make all
```

Build a specific experiment:

```bash
make 01-basic-crud
```

Clean all build artifacts:

```bash
make clean
```

## Experiments

| #  | Directory              | Description                                      |
|----|------------------------|--------------------------------------------------|
| 01 | `01-basic-crud/`       | Basic put, get, delete operations                |
| 02 | `02-column-families/`  | Creating and managing column families            |
| 03 | `03-transactions/`     | ACID transactions and isolation levels           |
| 04 | `04-benchmarks/`       | Write/read throughput benchmarks                 |
| 05 | `05-ttl/`              | Time-to-live key expiration                      |
| 06 | `06-crash-recovery/`   | WAL replay and crash recovery scenarios          |
| 07 | `07-compression/`      | Comparing Snappy, LZ4, and Zstd compression      |

## Common Utilities

Shared helpers live in `common/`. All experiments link against these:

- `common/utils.h` / `common/utils.c` — error handling, timing macros, print helpers