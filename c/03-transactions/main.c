/*
 * Experiment 03 — Transactions
 *
 * Demonstrates explicit transaction control with the TidesDB C API:
 *   1. Basic commit — writes become durable only after commit
 *   2. Rollback — all writes discarded, database unchanged
 *   3. Savepoints — partial undo within a single transaction
 *   4. Isolation levels — compare conflict behaviour across levels
 *
 * Experiments 01 and 02 used helpers (tdb_put_kv, etc.) that manage
 * transactions internally.  This experiment works with the raw
 * begin → operate → commit / rollback API so you can see the full
 * lifecycle.
 */

#include <stdio.h>
#include <string.h>
#include <tidesdb/tidesdb.h>
#include "../common/utils.h"

#define DB_PATH "03-transactions/data"
#define CF_NAME "txn_demo"

/* ── tiny helpers (reduce casting noise, txn is caller-managed) ── */

static void txn_put(tidesdb_txn_t *txn, tidesdb_column_family_t *cf,
                    const char *key, const char *value)
{
    TDB_CHECK(tidesdb_txn_put(
        txn, cf,
        (const uint8_t *)key,   strlen(key),
        (const uint8_t *)value, strlen(value), 0));
    printf("      PUT  %s => %s\n", key, value);
}

/* ── main ────────────────────────────────────────────────── */

int main(void) {
    tidesdb_t *db = NULL;
    tidesdb_txn_t *txn = NULL;
    int rc;

    print_header("Experiment 03: Transactions");

    /* ── 1. Setup ───────────────────────────────────────── */
    printf("\n[1] Opening database and creating column family ...\n");

    tidesdb_config_t db_cfg = tidesdb_default_config();
    db_cfg.db_path   = DB_PATH;
    db_cfg.log_level = TDB_LOG_WARN;
    TDB_CHECK(tidesdb_open(&db_cfg, &db));

    tidesdb_column_family_config_t cf_cfg = tidesdb_default_column_family_config();
    tidesdb_column_family_t *cf = tdb_ensure_cf(db, CF_NAME, &cf_cfg);
    printf("    Ready.\n");

    /* ── 2. Basic Commit ────────────────────────────────── */
    print_header("COMMIT — writes become durable after commit");

    TDB_CHECK(tidesdb_txn_begin(db, &txn));
    printf("    Transaction started (default: READ_COMMITTED).\n\n");

    txn_put(txn, cf, "city",       "Tokyo");
    txn_put(txn, cf, "country",    "Japan");
    txn_put(txn, cf, "population", "14 million");

    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);
    txn = NULL;
    printf("\n    Transaction committed.\n");

    printf("\n    Verification (fresh read):\n");
    {
        const char *k[] = { "city", "country", "population" };
        tdb_get_kv(db, cf, NULL, k, 3);
    }

    /* ── 3. Rollback ────────────────────────────────────── */
    print_header("ROLLBACK — all writes discarded");

    TDB_CHECK(tidesdb_txn_begin(db, &txn));
    printf("    Transaction started.\n\n");

    txn_put(txn, cf, "city",     "Paris");        /* would overwrite */
    txn_put(txn, cf, "temp_key", "temp_value");   /* new key */

    TDB_CHECK(tidesdb_txn_rollback(txn));
    tidesdb_txn_free(txn);
    txn = NULL;
    printf("\n    Transaction rolled back.\n");

    printf("\n    Verification (nothing changed):\n");
    {
        const char *k[] = { "city", "temp_key" };
        tdb_get_kv(db, cf, NULL, k, 2);
    }

    /* ── 4. Savepoints ──────────────────────────────────── */
    print_header("SAVEPOINTS — partial undo within a transaction");

    TDB_CHECK(tidesdb_txn_begin(db, &txn));
    printf("    Transaction started.\n\n");

    /* Phase 1 — safe writes */
    printf("    Phase 1 (safe writes):\n");
    txn_put(txn, cf, "lang",     "Japanese");
    txn_put(txn, cf, "currency", "Yen");

    TDB_CHECK(tidesdb_txn_savepoint(txn, "before_risky"));
    printf("\n    ✓ Savepoint 'before_risky' created.\n\n");

    /* Phase 2 — risky writes */
    printf("    Phase 2 (risky writes):\n");
    txn_put(txn, cf, "city",    "Osaka");      /* wrong city */
    txn_put(txn, cf, "mistake", "bad data");

    TDB_CHECK(tidesdb_txn_rollback_to_savepoint(txn, "before_risky"));
    printf("\n    ↩ Rolled back to 'before_risky' — phase 2 undone.\n\n");

    /* Phase 3 — correct writes after undo */
    printf("    Phase 3 (corrected writes):\n");
    txn_put(txn, cf, "capital", "Tokyo");

    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);
    txn = NULL;
    printf("\n    Transaction committed.\n");

    printf("\n    Verification:\n");
    printf("    (lang, currency, capital exist; city still Tokyo; mistake absent)\n");
    {
        const char *k[] = { "lang", "currency", "capital", "city", "mistake" };
        tdb_get_kv(db, cf, NULL, k, 5);
    }

    /* ── 5. Isolation Levels — conflict behaviour ───────── */
    print_header("ISOLATION — comparing conflict behaviour");

    /*
     * Scenario:  two transactions both write the same key.
     *
     * [A] SNAPSHOT isolation  — write-write conflict detected;
     *     first committer wins, second gets TDB_ERR_CONFLICT.
     *
     * [B] READ_COMMITTED      — no conflict detection;
     *     both commits succeed, last writer wins.
     */

    /* ── A. Snapshot ── */
    printf("\n  [A] Snapshot isolation (write-write conflict ON)\n\n");
    {
        tidesdb_txn_t *t1 = NULL, *t2 = NULL;

        TDB_CHECK(tidesdb_txn_begin_with_isolation(
            db, TDB_ISOLATION_SNAPSHOT, &t1));
        TDB_CHECK(tidesdb_txn_begin_with_isolation(
            db, TDB_ISOLATION_SNAPSHOT, &t2));
        printf("      txn1 & txn2 started (SNAPSHOT).\n\n");

        txn_put(t1, cf, "color", "blue");
        txn_put(t2, cf, "color", "green");

        rc = tidesdb_txn_commit(t1);
        printf("\n      txn1 commit → %s  (rc=%d)\n",
               rc == TDB_SUCCESS ? "OK" : "FAILED", rc);

        rc = tidesdb_txn_commit(t2);
        printf("      txn2 commit → %s  (rc=%d)\n",
               rc == TDB_ERR_CONFLICT ? "CONFLICT (expected)" : "OK", rc);

        tidesdb_txn_free(t1);
        tidesdb_txn_free(t2);
    }

    printf("\n      Result (first committer wins):\n");
    { const char *k[] = { "color" }; tdb_get_kv(db, cf, NULL, k, 1); }

    /* ── B. Read Committed ── */
    printf("\n  [B] Read Committed (no conflict detection)\n\n");
    {
        tidesdb_txn_t *t1 = NULL, *t2 = NULL;

        TDB_CHECK(tidesdb_txn_begin_with_isolation(
            db, TDB_ISOLATION_READ_COMMITTED, &t1));
        TDB_CHECK(tidesdb_txn_begin_with_isolation(
            db, TDB_ISOLATION_READ_COMMITTED, &t2));
        printf("      txn1 & txn2 started (READ_COMMITTED).\n\n");

        txn_put(t1, cf, "fruit", "apple");
        txn_put(t2, cf, "fruit", "banana");

        rc = tidesdb_txn_commit(t1);
        printf("\n      txn1 commit → %s  (rc=%d)\n",
               rc == TDB_SUCCESS ? "OK" : "FAILED", rc);

        rc = tidesdb_txn_commit(t2);
        printf("      txn2 commit → %s  (rc=%d)\n",
               rc == TDB_SUCCESS ? "OK (no conflict check)" : "FAILED", rc);

        tidesdb_txn_free(t1);
        tidesdb_txn_free(t2);
    }

    printf("\n      Result (last writer wins):\n");
    { const char *k[] = { "fruit" }; tdb_get_kv(db, cf, NULL, k, 1); }

    /* ── 6. Close ───────────────────────────────────────── */
    print_header("CLEANUP");
    TDB_CHECK(tidesdb_close(db));
    printf("    Database closed.\n\n");

    return 0;
}