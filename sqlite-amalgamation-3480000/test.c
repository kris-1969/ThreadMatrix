#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <sqlite3.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define DB_POOL_SIZE 3

// Mutex for database access
CRITICAL_SECTION db_mutex; // Use CRITICAL_SECTION for Windows

sqlite3 *db_pool[DB_POOL_SIZE];

// Initialize the database connection pool
void init_db_pool() {
    for (int i = 0; i < DB_POOL_SIZE; i++) {
        if (sqlite3_open("database.db", &db_pool[i]) != SQLITE_OK) {
            fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_pool[i]));
            exit(1);
        }
    }
}

// Close database connections
void close_db_pool() {
    for (int i = 0; i < DB_POOL_SIZE; i++) {
        sqlite3_close(db_pool[i]);
    }
}

// Get a database connection from the pool
sqlite3 *get_db_connection() {
    static int index = 0;
    sqlite3 *db;

    EnterCriticalSection(&db_mutex); // Lock
    db = db_pool[index];
    index = (index + 1) % DB_POOL_SIZE;
    LeaveCriticalSection(&db_mutex);  // Unlock

    return db;
}

// Handle client requests
DWORD WINAPI handle_client(LPVOID arg) { // Correct thread function signature
    int client_socket = *(int *)arg;
    char buffer[1024];
    int bytes_received;

    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Received: %s\n", buffer);
        send(client_socket, "Message received", 16, 0);
    } else if (bytes_received == SOCKET_ERROR) {
        perror("recv failed");
    }


    closesocket(client_socket);
    free(arg);
    return 0; // Correct return for thread function
}

int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr); // Correct type for client_len

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed with error %d\n", WSAGetLastError());
        return 1;
    }

    InitializeCriticalSection(&db_mutex); // Initialize the mutex

    init_db_pool();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) { // Check for INVALID_SOCKET
        perror("Socket creation failed");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) { // Check for SOCKET_ERROR
        perror("Binding failed");
        closesocket(server_socket); // Close socket before cleanup
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, MAX_CLIENTS) == SOCKET_ERROR) { // Check for SOCKET_ERROR
        perror("Listening failed");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        client_socket = malloc(sizeof(int));
        if (client_socket == NULL) {
            perror("malloc failed");
            continue; // Or exit, depending on how you want to handle this.
        }

        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (*client_socket == INVALID_SOCKET) { // Correct check for accept failure
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        HANDLE thread_handle;
        DWORD thread_id;

        thread_handle = CreateThread(NULL, 0, handle_client, client_socket, 0, &thread_id); // Use CreateThread
        if (thread_handle == NULL) {
            perror("Thread creation failed");
            closesocket(*client_socket);
            free(client_socket);
        } else {
            CloseHandle(thread_handle); // Close the thread handle if you don't need it later
        }

    }

    close_db_pool();
    closesocket(server_socket);
    WSACleanup();
    DeleteCriticalSection(&db_mutex); // Delete the critical section
    return 0;
}