/*
 * Experiment 05 — TTL (Time-to-Live)
 *
 * Demonstrates automatic key expiration using TidesDB's TTL support:
 *   1. Insert keys with different TTL values (2s, 4s, permanent)
 *   2. Verify all keys exist immediately after insertion
 *   3. Wait and observe short-lived keys expire first
 *   4. Wait more and observe medium-lived keys expire
 *   5. Scan to confirm only permanent keys survive
 *   6. Overwrite a permanent key with a TTL to make it expire
 *
 * TTL is passed as the last argument to tidesdb_txn_put (in seconds).
 * A TTL of 0 means the key never expires.  Expired keys are filtered
 * out at read time — no compaction needed to observe expiration.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <tidesdb/tidesdb.h>
#include "../common/utils.h"

#define DB_PATH "05-ttl/data"
#define CF_NAME "ttl_demo"

/* Put a single KV with a specified TTL. */
static void ttl_put(tidesdb_t *db, tidesdb_column_family_t *cf,
                    const char *key, const char *value, time_t ttl)
{
    tidesdb_txn_t *txn = NULL;
    TDB_CHECK(tidesdb_txn_begin(db, &txn));
    TDB_CHECK(tidesdb_txn_put(txn, cf,
        (const uint8_t *)key,   strlen(key),
        (const uint8_t *)value, strlen(value), ttl));
    TDB_CHECK(tidesdb_txn_commit(txn));
    tidesdb_txn_free(txn);

    if (ttl > 0)
        printf("    PUT  %-20s => %-20s  (TTL: %lds)\n",
               key, value, (long)ttl);
    else
        printf("    PUT  %-20s => %-20s  (permanent)\n", key, value);
}

/* Sleep with a visible dot-countdown. */
static void wait_seconds(int seconds)
{
    printf("\n    Waiting %d seconds", seconds);
    fflush(stdout);
    for (int i = 0; i < seconds; i++) {
        sleep(1);
        printf(" .");
        fflush(stdout);
    }
    printf(" done.\n\n");
}

/* Keys used throughout the experiment. */
static const char *all_keys[] = {
    "session:alice", "session:bob", "user:alice", "user:bob"
};
#define NUM_KEYS 4

int main(void) {
    tidesdb_t *db = NULL;

    print_header("Experiment 05: TTL (Time-to-Live)");

    /* ── 1. Setup ───────────────────────────────────────── */
    printf("\n[1] Opening database and creating column family ...\n");

    tidesdb_config_t db_cfg = tidesdb_default_config();
    db_cfg.db_path   = DB_PATH;
    db_cfg.log_level = TDB_LOG_WARN;
    TDB_CHECK(tidesdb_open(&db_cfg, &db));

    tidesdb_column_family_config_t cf_cfg = tidesdb_default_column_family_config();
    tidesdb_column_family_t *cf = tdb_ensure_cf(db, CF_NAME, &cf_cfg);
    printf("    Ready.\n");

    /* ── 2. Insert keys with different lifetimes ────────── */
    print_header("INSERT — keys with different TTLs");

    ttl_put(db, cf, "session:alice", "logged_in", 2);   /* expires in 2s */
    ttl_put(db, cf, "session:bob",   "logged_in", 4);   /* expires in 4s */
    ttl_put(db, cf, "user:alice",    "Alice",     0);   /* permanent     */
    ttl_put(db, cf, "user:bob",      "Bob",       0);   /* permanent     */

    /* ── 3. Immediate check — all four keys exist ───────── */
    print_header("CHECK t=0s — all keys should exist");
    tdb_get_kv(db, cf, NULL, all_keys, NUM_KEYS);

    /* ── 4. After 3 seconds — 2s TTL has expired ────────── */
    print_header("CHECK t=3s — 2s TTL expired, 4s still alive");
    wait_seconds(3);
    tdb_get_kv(db, cf, NULL, all_keys, NUM_KEYS);

    /* ── 5. After 5 seconds total — 4s TTL also expired ── */
    print_header("CHECK t=5s — both sessions expired");
    wait_seconds(2);
    tdb_get_kv(db, cf, NULL, all_keys, NUM_KEYS);

    /* ── 6. Scan — only permanent keys survive ──────────── */
    print_header("SCAN — remaining keys after expiration");
    tdb_scan(db, cf, NULL);

    /* ── 7. Overwrite a permanent key WITH a TTL ────────── */
    print_header("OVERWRITE — making a permanent key temporary");

    printf("    'user:bob' is currently permanent. Overwriting with 2s TTL ...\n\n");
    ttl_put(db, cf, "user:bob", "Bob (now temporary)", 2);

    printf("\n    Immediately after overwrite:\n");
    { const char *k[] = { "user:bob" }; tdb_get_kv(db, cf, NULL, k, 1); }

    wait_seconds(3);

    printf("    After expiration:\n");
    { const char *k[] = { "user:bob" }; tdb_get_kv(db, cf, NULL, k, 1); }

    /* ── 8. Final state ─────────────────────────────────── */
    print_header("FINAL SCAN — only truly permanent keys remain");
    tdb_scan(db, cf, NULL);

    /* ── 9. Close ───────────────────────────────────────── */
    print_header("CLEANUP");
    TDB_CHECK(tidesdb_close(db));
    printf("    Database closed.\n\n");

    return 0;
}