/*
 * Experiment 01 — Basic CRUD Operations
 *
 * Demonstrates the fundamental TidesDB workflow:
 *   1. Open a database
 *   2. Create a column family
 *   3. Put key-value pairs
 *   4. Get and verify values
 *   5. Delete a key and confirm removal
 *   6. Iterate over all remaining keys
 *   7. Close the database
 *
 * All reads and writes in TidesDB go through transactions.
 * The helpers in common/utils.h manage transaction lifecycle
 * automatically — see experiment 03 for explicit transaction control.
 */

#include <stdio.h>
#include <string.h>
#include <tidesdb/tidesdb.h>
#include "../common/utils.h"

#define DB_PATH  "01-basic-crud/data"
#define CF_NAME  "my_first_cf"

int main(void) {
    tidesdb_t *db = NULL;

    print_header("Experiment 01: Basic CRUD");

    /* ── 1. Open the database ───────────────────────────── */
    printf("\n[1] Opening database at '%s' ...\n", DB_PATH);

    tidesdb_config_t config = tidesdb_default_config();
    config.db_path = DB_PATH;
    config.log_level = TDB_LOG_WARN;

    TDB_CHECK(tidesdb_open(&config, &db));
    printf("    Database opened successfully.\n");

    /* ── 2. Create a column family ──────────────────────── */
    printf("\n[2] Creating column family '%s' ...\n", CF_NAME);

    tidesdb_column_family_config_t cf_cfg = tidesdb_default_column_family_config();
    tidesdb_column_family_t *cf = tdb_ensure_cf(db, CF_NAME, &cf_cfg);
    printf("    Column family ready.\n");

    /* ── 3. Put key-value pairs ─────────────────────────── */
    print_header("PUT — inserting 5 key-value pairs");

    const char *keys[] = { "name", "type", "language", "license", "version" };
    const char *vals[] = {
        "TidesDB", "LSM-tree storage engine", "C", "MPL-2.0", "9.2.5"
    };
    tdb_put_kv(db, cf, NULL, keys, vals, 5);

    /* ── 4. Get and verify values ───────────────────────── */
    print_header("GET — reading back all keys");
    tdb_get_kv(db, cf, NULL, keys, 5);

    /* ── 5. Delete a key and confirm removal ────────────── */
    print_header("DELETE — removing 'version' key");

    const char *del_keys[] = { "version" };
    tdb_delete_kv(db, cf, NULL, del_keys, 1);

    printf("    Verifying deletion:\n");
    tdb_get_kv(db, cf, NULL, del_keys, 1);  /* expect NOT FOUND */

    /* ── 6. Iterate over remaining keys ─────────────────── */
    print_header("ITERATE — scanning all remaining keys");
    tdb_scan(db, cf, NULL);

    /* ── 7. Close ───────────────────────────────────────── */
    print_header("CLEANUP");
    TDB_CHECK(tidesdb_close(db));
    printf("    Database closed.\n\n");

    return 0;
}