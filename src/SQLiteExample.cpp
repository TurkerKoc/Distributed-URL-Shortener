#include <sqlite3.h>

#define NULL 0


int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    // Open or create the SQLite database
    rc = sqlite3_open("raft.db", &db);
    if (rc) {
        // Handle error
    }

    // Create the table to store logs
    const char *sql = "CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY, term INTEGER, key TEXT, value TEXT);";
    rc = sqlite3_exec(db, sql, NULL, 0, NULL);
    if (rc) {
        // Handle error
    }

    // Insert a new log
    sql = "INSERT INTO logs (term, key, value) VALUES (?, ?, ?);";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        // Handle error
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, 1); // term
    sqlite3_bind_text(stmt, 2, "key1", -1, SQLITE_STATIC); // key
    sqlite3_bind_text(stmt, 3, "value1", -1, SQLITE_STATIC); // value

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        // Handle error
    }

    // Close the statement
    sqlite3_finalize(stmt);

    // Close the database
    sqlite3_close(db);

    return 0;
}






/*
 *
 * // read data from table and save in vector
    std::vector<KeyValuePair> data;
    std::string select_query = "SELECT key, value FROM logs ORDER BY id;";
    rc = sqlite3_prepare_v2(db, select_query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Error preparing select statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        KeyValuePair pair;
        pair.key = sqlite3_column_int(stmt, 0);
        pair.value = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        data.push_back(pair);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // print the key-value pairs
    for (auto const& pair : data) {
        std::cout << pair.key << ": " << pair.value << std::endl;
    }
    return 0;
}
 *
 */
