# 03 — Transactions

Tests ACID transactions including commit, rollback, and isolation levels.

## What This Covers

- Beginning and committing transactions
- Rolling back transactions
- Multi-operation atomic writes
- Isolation level comparisons (read-uncommitted through serializable)
- Conflict detection with snapshot isolation

## Build & Run

```bash
make        # from c/ directory: make 03-transactions
./03-transactions
```