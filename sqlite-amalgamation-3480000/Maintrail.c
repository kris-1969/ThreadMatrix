#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3 *db;
    int result = sqlite3_open("test.db", &db);

    if (result == SQLITE_OK) {
        printf("Database opened successfully!\n");
        sqlite3_close(db);
    } else {
        printf("Failed to open database: %s\n", sqlite3_errmsg(db));
    }

    return 0;
}
