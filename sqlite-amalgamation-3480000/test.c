'#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sqlite3.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define DB_POOL_SIZE 3

pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
sqlite3 *db_pool[DB_POOL_SIZE];

// Function to initialize the database connection pool
void init_db_pool() {
    for (int i = 0; i < DB_POOL_SIZE; i++) {
        if (sqlite3_open("database.db", &db_pool[i]) != SQLITE_OK) {
            fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_pool[i]));
            exit(1);
        }
    }
}

// Function to close database connections
void close_db_pool() {
    for (int i = 0; i < DB_POOL_SIZE; i++) {
        sqlite3_close(db_pool[i]);
    }
}

// Function to get a database connection from the pool
sqlite3 *get_db_connection() {
    static int index = 0;
    sqlite3 *db;
    pthread_mutex_lock(&db_mutex);
    db = db_pool[index];
    index = (index + 1) % DB_POOL_SIZE;
    pthread_mutex_unlock(&db_mutex);
    return db;
}

// Function to handle client requests
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[1024];
    int bytes_read;

    bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);
        write(client_socket, "Message received", 16);
    }

    close(client_socket);
    free(arg);
    pthread_exit(NULL);
}

int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    // Initialize the database pool
    init_db_pool();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Binding failed");
        exit(1);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listening failed");
        exit(1);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (*client_socket == -1) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }
        pthread_create(&thread_id, NULL, handle_client, client_socket);
        pthread_detach(thread_id);
    }

    close_db_pool();
    close(server_socket);
    return 0;
}' 