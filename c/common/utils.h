#ifndef TIDESDB_EXP_UTILS_H
#define TIDESDB_EXP_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tidesdb/tidesdb.h>

/*
 * TDB_CHECK — abort with message if a TidesDB call fails (returns non-zero).
 * Usage:  TDB_CHECK(tidesdb_open(&config, &db));
 */
#define TDB_CHECK(call)                                                    \
    do {                                                                   \
        int _rc = (call);                                                  \
        if (_rc != TDB_SUCCESS) {                                      \
            fprintf(stderr, "[ERROR] %s:%d  %s  returned %d\n",            \
                    __FILE__, __LINE__, #call, _rc);                       \
            exit(EXIT_FAILURE);                                            \
        }                                                                  \
    } while (0)

/*
 * Simple wall-clock timer.
 *
 *   timer_t t;
 *   timer_start(&t);
 *   // ... work ...
 *   double ms = timer_elapsed_ms(&t);
 */
typedef struct {
    struct timespec start;
} exp_timer_t;

static inline void timer_start(exp_timer_t *t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

static inline double timer_elapsed_ms(const exp_timer_t *t) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - t->start.tv_sec) * 1000.0 +
           (now.tv_nsec - t->start.tv_nsec) / 1e6;
}

/* Print a separator line */
static inline void print_sep(void) {
    printf("────────────────────────────────────────\n");
}

/* Print a section header */
static inline void print_header(const char *title) {
    printf("\n");
    print_sep();
    printf("  %s\n", title);
    print_sep();
}

/* ── TidesDB experiment helpers ──────────────────────────
 *
 * Higher-level wrappers that hide the txn begin/commit/free boilerplate.
 * Each function aborts on error (via TDB_CHECK), which is fine for
 * experiments.  The optional `label` parameter prefixes output lines
 * with [label] when non-NULL — handy for multi-CF experiments.
 * ──────────────────────────────────────────────────────── */

/*
 * tdb_ensure_cf — create a column family (or reuse if it already exists)
 * and return its handle.  Aborts on any error other than TDB_ERR_EXISTS.
 */
static inline tidesdb_column_family_t *
tdb_ensure_cf(tidesdb_t *db, const char *name,
              const tidesdb_column_family_config_t *cfg)
{
    int rc = tidesdb_create_column_family(db, name, cfg);
    if (rc != TDB_SUCCESS && rc != TDB_ERR_EXISTS) {
        fprintf(stderr, "[ERROR] %s:%d  create_column_family('%s') returned %d\n",
                __FILE__, __LINE__, name, rc);
        exit(EXIT_FAILURE);
    }
    tidesdb_column_family_t *cf = tidesdb_get_column_family(db, name);
    if (!cf) {
        fprintf(stderr, "[ERROR] %s:%d  get_column_family('%s') returned NULL\n",
                __FILE__, __LINE__, name);
        exit(EXIT_FAILURE);
    }
    return cf;
}

/*
 * tdb_put_kv — put string key-value pairs in a single transaction.
 */
static inline void
tdb_put_kv(tidesdb_t *db, tidesdb_column_family_t *cf, const char *label,
           const char *keys[], const char *vals[], int n)
{
    tidesdb_txn_t *txn = NULL;
    TDB_CHECK(tidesdb_txn_begin(db, &txn));

    for (int i = 0; i < n; i++) {
        TDB_CHECK(tidesdb_txn_put(
            txn, cf,
            (const uint8_t *)keys[i], strlen(keys[i]),
            (const uint8_t *)vals[i], strlen(vals[i]), 0));
        if (label)
            printf("    [%s] PUT  %s => %s\n", label, keys[i], vals[i]);
        else
            printf("    PUT  %s => %s\n", keys[i], vals[i]);
    }

    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);
}

/*
 * tdb_get_kv — read and print string keys from a column family.
 */
static inline void
tdb_get_kv(tidesdb_t *db, tidesdb_column_family_t *cf, const char *label,
           const char *keys[], int n)
{
    tidesdb_txn_t *txn = NULL;
    TDB_CHECK(tidesdb_txn_begin(db, &txn));

    for (int i = 0; i < n; i++) {
        uint8_t *value = NULL;
        size_t value_size = 0;
        int rc = tidesdb_txn_get(
            txn, cf,
            (const uint8_t *)keys[i], strlen(keys[i]),
            &value, &value_size);

        if (rc == TDB_SUCCESS) {
            if (label)
                printf("    [%s] GET  %s => %.*s\n",
                       label, keys[i], (int)value_size, value);
            else
                printf("    GET  %s => %.*s\n",
                       keys[i], (int)value_size, value);
            tidesdb_free(value);
        } else {
            if (label)
                printf("    [%s] GET  %s => [NOT FOUND]\n", label, keys[i]);
            else
                printf("    GET  %s => [NOT FOUND]\n", keys[i]);
        }
    }

    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);
}

/*
 * tdb_delete_kv — delete string keys in a single transaction.
 */
static inline void
tdb_delete_kv(tidesdb_t *db, tidesdb_column_family_t *cf, const char *label,
              const char *keys[], int n)
{
    tidesdb_txn_t *txn = NULL;
    TDB_CHECK(tidesdb_txn_begin(db, &txn));

    for (int i = 0; i < n; i++) {
        TDB_CHECK(tidesdb_txn_delete(
            txn, cf,
            (const uint8_t *)keys[i], strlen(keys[i])));
        if (label)
            printf("    [%s] DELETE  %s\n", label, keys[i]);
        else
            printf("    DELETE  %s\n", keys[i]);
    }

    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);
}

/*
 * tdb_scan — iterate and print every key-value pair in a column family.
 * Returns the total number of keys found.
 */
static inline int
tdb_scan(tidesdb_t *db, tidesdb_column_family_t *cf, const char *label)
{
    tidesdb_txn_t *txn = NULL;
    TDB_CHECK(tidesdb_txn_begin(db, &txn));

    tidesdb_iter_t *iter = NULL;
    TDB_CHECK(tidesdb_iter_new(txn, cf, &iter));
    TDB_CHECK(tidesdb_iter_seek_to_first(iter));

    int count = 0;
    while (tidesdb_iter_valid(iter)) {
        uint8_t *key = NULL, *value = NULL;
        size_t key_size = 0, value_size = 0;

        if (tidesdb_iter_key_value(iter, &key, &key_size,
                                   &value, &value_size) == TDB_SUCCESS) {
            if (label)
                printf("    [%s] [%d] %.*s => %.*s\n", label, count,
                       (int)key_size, key, (int)value_size, value);
            else
                printf("    [%d] %.*s => %.*s\n", count,
                       (int)key_size, key, (int)value_size, value);
            count++;
        }
        tidesdb_iter_next(iter);
    }

    if (label)
        printf("    [%s] Total keys: %d\n", label, count);
    else
        printf("    Total keys: %d\n", count);

    tidesdb_iter_free(iter);
    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);
    return count;
}

/*
 * tdb_list_cfs — print all column families in the database.
 */
static inline void
tdb_list_cfs(tidesdb_t *db)
{
    char **names = NULL;
    int count = 0;
    TDB_CHECK(tidesdb_list_column_families(db, &names, &count));
    printf("    Column families (%d):\n", count);
    for (int i = 0; i < count; i++) {
        printf("      [%d] %s\n", i, names[i]);
        tidesdb_free(names[i]);
    }
    tidesdb_free(names);
}

#endif /* TIDESDB_EXP_UTILS_H */