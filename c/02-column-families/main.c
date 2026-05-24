/*
 * Experiment 02 — Column Families
 *
 * Explores creating, configuring, and managing multiple column families.
 *
 *   1. Open a database
 *   2. Create three column families with different configurations:
 *      - "users"    — LZ4 compression, bloom filters enabled
 *      - "logs"     — Zstd compression (high ratio), no bloom filter
 *      - "cache"    — No compression (speed over space)
 *   3. Write distinct data to each family
 *   4. Read back from each family and verify isolation
 *   5. List all column families
 *   6. Drop a column family and confirm removal
 *   7. Close the database
 *
 * Key takeaway: column families are independent namespaces within a single
 * database.  Each family carries its own LSM tree, compression, bloom
 * filter, and write-buffer settings.
 */

#include <stdio.h>
#include <string.h>
#include <tidesdb/tidesdb.h>
#include "../common/utils.h"

#define DB_PATH "02-column-families/data"

int main(void) {
    tidesdb_t *db = NULL;

    print_header("Experiment 02: Column Families");

    /* ── 1. Open the database ───────────────────────────── */
    printf("\n[1] Opening database at '%s' ...\n", DB_PATH);

    tidesdb_config_t config = tidesdb_default_config();
    config.db_path = DB_PATH;
    config.log_level = TDB_LOG_WARN;

    TDB_CHECK(tidesdb_open(&config, &db));
    printf("    Database opened.\n");

    /* ── 2. Create column families with different configs ─ */
    print_header("CREATE — three column families");

    tidesdb_column_family_config_t cf_cfg;

    /* "users" — LZ4, bloom filter ON */
    cf_cfg = tidesdb_default_column_family_config();
    cf_cfg.compression_algorithm = TDB_COMPRESS_LZ4;
    cf_cfg.enable_bloom_filter   = 1;
    cf_cfg.bloom_fpr             = 0.01;
    tidesdb_column_family_t *cf_users = tdb_ensure_cf(db, "users", &cf_cfg);
    printf("    'users'  ready  [LZ4, bloom ON, fpr=0.01]\n");

    /* "logs" — Zstd, bloom filter OFF */
    cf_cfg = tidesdb_default_column_family_config();
    cf_cfg.compression_algorithm = TDB_COMPRESS_ZSTD;
    cf_cfg.enable_bloom_filter   = 0;
    tidesdb_column_family_t *cf_logs = tdb_ensure_cf(db, "logs", &cf_cfg);
    printf("    'logs'   ready  [Zstd, bloom OFF]\n");

    /* "cache" — no compression, bloom filter ON */
    cf_cfg = tidesdb_default_column_family_config();
    cf_cfg.compression_algorithm = TDB_COMPRESS_NONE;
    cf_cfg.enable_bloom_filter   = 1;
    cf_cfg.bloom_fpr             = 0.001;
    tidesdb_column_family_t *cf_cache = tdb_ensure_cf(db, "cache", &cf_cfg);
    printf("    'cache'  ready  [no compression, bloom ON, fpr=0.001]\n");

    /* ── 3. Write data to each family ───────────────────── */
    print_header("PUT — writing data to each family");

    {
        const char *k[] = { "user:1:name", "user:1:email", "user:2:name", "user:2:email" };
        const char *v[] = { "Alice", "alice@example.com", "Bob", "bob@example.com" };
        tdb_put_kv(db, cf_users, "users", k, v, 4);
    }
    {
        const char *k[] = { "log:001", "log:002", "log:003" };
        const char *v[] = {
            "2025-06-01 INFO  Server started",
            "2025-06-01 WARN  High memory usage",
            "2025-06-01 ERROR Connection timeout"
        };
        tdb_put_kv(db, cf_logs, "logs", k, v, 3);
    }
    {
        const char *k[] = { "session:abc123", "session:def456" };
        const char *v[] = { "{\"user\":1,\"ttl\":3600}", "{\"user\":2,\"ttl\":7200}" };
        tdb_put_kv(db, cf_cache, "cache", k, v, 2);
    }

    /* ── 4. Read back from each family (verify isolation) ─ */
    print_header("GET — reading back from each family");

    printf("\n  -- users --\n");
    {
        const char *k[] = { "user:1:name", "user:1:email", "user:2:name", "user:2:email" };
        tdb_get_kv(db, cf_users, "users", k, 4);
    }
    printf("\n  -- logs --\n");
    {
        const char *k[] = { "log:001", "log:002", "log:003" };
        tdb_get_kv(db, cf_logs, "logs", k, 3);
    }
    printf("\n  -- cache --\n");
    {
        const char *k[] = { "session:abc123", "session:def456" };
        tdb_get_kv(db, cf_cache, "cache", k, 2);
    }

    /* Cross-family isolation: a "users" key must NOT appear in "logs" */
    printf("\n  -- cross-family isolation check --\n");
    {
        const char *k[] = { "user:1:name" };
        tdb_get_kv(db, cf_logs, "logs", k, 1);  /* expect NOT FOUND */
    }

    /* ── 5. List column families ────────────────────────── */
    print_header("LIST — enumerating all column families");
    tdb_list_cfs(db);

    /* ── 6. Scan a family, then drop it ─────────────────── */
    print_header("SCAN + DROP — scanning 'cache', then dropping it");

    printf("\n  Scanning 'cache' before drop:\n");
    tdb_scan(db, cf_cache, "cache");

    printf("\n  Dropping 'cache' ...\n");
    TDB_CHECK(tidesdb_drop_column_family(db, "cache"));
    printf("    'cache' dropped.\n");

    printf("    Remaining:\n");
    tdb_list_cfs(db);

    /* ── 7. Final scan of surviving families ─────────────── */
    print_header("ITERATE — scanning surviving families");

    printf("\n  -- users --\n");
    tdb_scan(db, cf_users, "users");
    printf("\n  -- logs --\n");
    tdb_scan(db, cf_logs, "logs");

    /* ── 8. Close ────────────────────────────────────────── */
    print_header("CLEANUP");
    TDB_CHECK(tidesdb_close(db));
    printf("    Database closed.\n\n");

    return 0;
}