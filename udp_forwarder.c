#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#ifndef VERSION
#define VERSION "current-" __DATE__
#endif

#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096
#define CONFIG_FILE_DEFAULT "config.ini"
#define HISTORY_SECONDS 60  // Track logs per second over the last 60 seconds

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int messages_last_second[HISTORY_SECONDS];  // Array to track logs per second for the last 60 seconds
    int total_messages;  // Total number of messages received from the client
} ClientStats;

ClientStats client_stats[MAX_CLIENTS];
int num_clients = 0;
int listen_socket;
int tcp_socket;
int daemon_mode = 0;  // Daemon mode flag
int silent_mode = 0;  // Silent mode flag

// Structure to hold server arguments (IP and ports)
typedef struct {
    int http_port;
    int listen_port;
    int remote_port;
    char forward_ip[INET_ADDRSTRLEN];
} ServerArgs;

// Function to trim whitespace from a string
char* trim_whitespace(char* str) {
    // Remove leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == '\0') return str;

    // Remove trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

// Function to parse the config file
int parse_config_file(const char* config_file, ServerArgs* args) {
    FILE* file = fopen(config_file, "r");
    if (file == NULL) {
        perror("Failed to open config file");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "=");

        if (key && value) {
            key = trim_whitespace(key);
            value = trim_whitespace(value);

            // Set the appropriate field in the args struct based on the key
            if (strcmp(key, "listen_port") == 0) {
                args->listen_port = atoi(value);
            } else if (strcmp(key, "remote_port") == 0) {
                args->remote_port = atoi(value);
            } else if (strcmp(key, "http_port") == 0) {
                args->http_port = atoi(value);
            } else if (strcmp(key, "forward_ip") == 0) {
                strncpy(args->forward_ip, value, INET_ADDRSTRLEN - 1);
                args->forward_ip[INET_ADDRSTRLEN - 1] = '\0';
            }
        }
    }

    fclose(file);
    return 0;
}

// Signal handler to gracefully shut down the server
void handle_interrupt(int sig) {
    if (!daemon_mode) {
        printf("\nStopping the forwarder.\n");
    }
    close(listen_socket);
    close(tcp_socket);
    exit(0);
}

// Update statistics for a specific client IP
void update_stats(const char* ip) {
    for (int i = 0; i < num_clients; ++i) {
        if (strcmp(client_stats[i].ip, ip) == 0) {
            client_stats[i].messages_last_second[HISTORY_SECONDS - 1]++;
            client_stats[i].total_messages++;
            return;
        }
    }

    // If it's a new client, add it to the list
    if (num_clients < MAX_CLIENTS) {
        strcpy(client_stats[num_clients].ip, ip);
        memset(client_stats[num_clients].messages_last_second, 0, sizeof(client_stats[num_clients].messages_last_second));
        client_stats[num_clients].messages_last_second[HISTORY_SECONDS - 1] = 1;
        client_stats[num_clients].total_messages = 1;
        num_clients++;
    }
}

// Shift and reset the logs per second for each client
void shift_and_reset_messages_per_second() {
    for (int i = 0; i < num_clients; ++i) {
        // Shift the log counts to the left to make room for the next second's count
        memmove(client_stats[i].messages_last_second, client_stats[i].messages_last_second + 1, 
                (HISTORY_SECONDS - 1) * sizeof(int));
        // Reset the last second's count to zero
        client_stats[i].messages_last_second[HISTORY_SECONDS - 1] = 0;
    }
}

// Calculate the average number of logs per second for a client over the last minute
int calculate_average_logs_per_second(ClientStats* client) {
    int sum = 0;
    for (int i = 0; i < HISTORY_SECONDS; ++i) {
        sum += client->messages_last_second[i];
    }
    return sum / HISTORY_SECONDS;
}

// Print statistics for all connected clients
void print_stats() {
    if (silent_mode) return;

    printf("--- Stats ---\n");

    int total_logs_per_second = 0;
    int total_logs_per_hour = 0;

    // Iterate over each client and print their stats
    for (int i = 0; i < num_clients; ++i) {
        int avg_logs_per_second = calculate_average_logs_per_second(&client_stats[i]);
        printf("Source IP: %s, Average logs per second (last minute): %d, Total logs per hour: %d\n",
               client_stats[i].ip, avg_logs_per_second, client_stats[i].total_messages);
        total_logs_per_second += avg_logs_per_second;
        total_logs_per_hour += client_stats[i].total_messages;
    }

    printf("\nTotal connected clients: %d\n", num_clients);
    printf("Average forwarded logs per second (last minute): %d\n", total_logs_per_second);
    printf("Total forwarded logs per hour: %d\n", total_logs_per_hour);
}

// Thread function to periodically print statistics
void* stats_printer(void* arg) {
    while (1) {
        sleep(30);
        if (!daemon_mode) {
            //system("clear");
            print_stats();
        }
        shift_and_reset_messages_per_second();
    }
}

// Handle HTTP requests from clients
void serve_http(int client_sock, const char* forward_ip, int listen_port, int remote_port, int http_port) {
    char request[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_sock, request, sizeof(request) - 1, 0);
    if (bytes_received < 0) {
        perror("Failed to receive data");
        close(client_sock);
        return;
    }
    request[bytes_received] = '\0';

    if (strstr(request, "GET /api") != NULL) {
        // Serve JSON response for /api path
        char response[8192];
        char json[4096] = "{\"clients\": [";

        int total_logs_per_second = 0;
        int total_logs_per_hour = 0;

        // Construct JSON response with client statistics
        for (int i = 0; i < num_clients; ++i) {
            int avg_logs_per_second = calculate_average_logs_per_second(&client_stats[i]);

            char client_info[256];
            snprintf(client_info, sizeof(client_info), "{\"ip\": \"%s\", \"average_logs_per_second\": %d, \"total_messages\": %d}",
                     client_stats[i].ip, avg_logs_per_second, client_stats[i].total_messages);
            if (strlen(json) + strlen(client_info) < sizeof(json) - 2) {
                strcat(json, client_info);
                if (i < num_clients - 1) {
                    strcat(json, ", ");
                }
            }

            total_logs_per_second += avg_logs_per_second;
            total_logs_per_hour += client_stats[i].total_messages;
        }

        strcat(json, "]");

        // Add overall statistics to the JSON response
        char total_info[256];
        snprintf(total_info, sizeof(total_info), ", \"total_logs_per_second\": %d, \"total_logs_per_hour\": %d, \"forward_ip\": \"%s\", \"listen_port\": %d, \"remote_port\": %d, \"http_port\": %d, \"num_clients\": %d}",
                 total_logs_per_second, total_logs_per_hour, forward_ip, listen_port, remote_port, http_port, num_clients);
        if (strlen(json) + strlen(total_info) < sizeof(json)) {
            strcat(json, total_info);
        }

        int header_len = snprintf(response, sizeof(response),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  strlen(json));

        if (header_len + strlen(json) < sizeof(response)) {
            strcat(response, json);
        }

        send(client_sock, response, strlen(response), 0);
    } else {
        // Serve HTML response for the root path
        char response[8192];
        char html[4096] = "<html><head><title>UDP Forwarder Stats</title><script>\n";

        strcat(html, "async function loadStats() {\n");
        strcat(html, "  const response = await fetch('/api');\n");
        strcat(html, "  const data = await response.json();\n");
        strcat(html, "  let table = \"<table border=\\\"1\\\"><tr style=\\\"background-color: lightblue;\\\"><th>IP Address</th><th>Average Logs per Second (Last Minute)</th><th>Total Messages Last Hour</th></tr>\";\n");
        strcat(html, "  if (data.clients.length > 0) {\n");
        strcat(html, "    data.clients.forEach(client => {\n");
        strcat(html, "      table += `<tr><td>${client.ip}</td><td>${client.average_logs_per_second}</td><td>${client.total_messages}</td></tr>`;\n");
        strcat(html, "    });\n");
        strcat(html, "  } else {\n");
        strcat(html, "    table += '<tr><td colspan=\\\"3\\\">No data available</td></tr>';\n");
        strcat(html, "  }\n");
        strcat(html, "  table += '</table><br><br>';\n");
        strcat(html, "  document.getElementById('stats').innerHTML = table;\n");
        strcat(html, "}\n");

        strcat(html, "async function loadGlobalStats() {\n");
        strcat(html, "  const response = await fetch('/api');\n");
        strcat(html, "  const data = await response.json();\n");
        strcat(html, "  let globalTable = \"<table border=\\\"1\\\"><tr style=\\\"background-color: lightblue;\\\"><th>Total Logs per Second</th><th>Total Logs per Hour</th><th>Forward IP</th><th>Listen Port</th><th>Remote Port</th><th>HTTP Port</th><th>Number of Clients</th></tr>\";\n");
        strcat(html, "  globalTable += `<tr><td>${data.total_logs_per_second}</td><td>${data.total_logs_per_hour}</td><td>${data.forward_ip}</td><td>${data.listen_port}</td><td>${data.remote_port}</td><td>${data.http_port}</td><td>${data.num_clients}</td></tr>`;\n");
        strcat(html, "  globalTable += '</table>';\n");
        strcat(html, "  document.getElementById('global_stats').innerHTML = globalTable;\n");
        strcat(html, "}\n");

        strcat(html, "setInterval(loadStats, 5000);\n");
        strcat(html, "setInterval(loadGlobalStats, 5000);\n");
        strcat(html, "window.onload = function() { loadStats(); loadGlobalStats(); };\n");
        strcat(html, "</script></head><body><h1>UDP Forwarder Stats</h1><div id='stats'></div><div id='global_stats'></div></body></html>");

        int header_len = snprintf(response, sizeof(response),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  strlen(html));

        if (header_len + strlen(html) < sizeof(response)) {
            strcat(response, html);
        }

        send(client_sock, response, strlen(response), 0);
    }

    close(client_sock);
}

// Thread function to run the HTTP server
void* tcp_server(void* arg) {
    ServerArgs* args = (ServerArgs*)arg;
    int http_port = args->http_port;

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("Failed to create TCP socket");
        return NULL;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(http_port);

    if (bind(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind TCP socket");
        close(tcp_socket);
        return NULL;
    }

    if (listen(tcp_socket, 5) < 0) {
        perror("Failed to listen on TCP socket");
        close(tcp_socket);
        return NULL;
    }

    if (!daemon_mode) {
        printf("TCP server listening on port %d\n", http_port);
    }

    while (1) {
        int client_sock = accept(tcp_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Failed to accept TCP connection");
            continue;
        }
        serve_http(client_sock, args->forward_ip, args->listen_port, args->remote_port, args->http_port);
    }

    close(tcp_socket);
    return NULL;
}

// Daemonize the process
#ifdef _WIN32
#else
void daemonize() {
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(0);
    }

    if (setsid() < 0) {
        perror("Failed to setsid");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("Failed to fork");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(0);
    }

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    chdir("/");
    umask(0);
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
#endif

    int listen_port = 514;
    int remote_port = 514;
    int http_port = 8514;

    int opt;
    char config_file[256] = CONFIG_FILE_DEFAULT;  // Default config file

    while ((opt = getopt(argc, argv, "l:r:w:c:dsv")) != -1) {
        switch (opt) {
            case 'l':
                listen_port = atoi(optarg);
                break;
            case 'r':
                remote_port = atoi(optarg);
                break;
            case 'w':
                http_port = atoi(optarg);
                break;
            case 'c':
                strncpy(config_file, optarg, sizeof(config_file) - 1);
                config_file[sizeof(config_file) - 1] = '\0';
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 's':
                silent_mode = 1;
                break;
            case 'v':
                printf("UDP Forwarder Version: %s\n", VERSION);
                exit(0);      
            default:
                fprintf(stderr, "Usage: %s [-l listen_port] [-r remote_port] [-w http_port] [-c config_file] [-s] [-d] <forward_ip>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Parse config file (if exists)
    ServerArgs args = {http_port, listen_port, remote_port, ""};
    parse_config_file(config_file, &args);  // Read the config file

    // Command-line forward_ip overrides config file
    if (optind < argc) {
        strncpy(args.forward_ip, argv[optind], INET_ADDRSTRLEN - 1);
        args.forward_ip[INET_ADDRSTRLEN - 1] = '\0';
    } else if (strlen(args.forward_ip) == 0) {
        fprintf(stderr, "No forward IP provided. Either use a config file or pass the IP as an argument.\n");
        exit(EXIT_FAILURE);
    }

    // Daemon mode only available on Unix liks OS
    #ifdef _WIN32
    #else
    if (daemon_mode) {
        daemonize();
    }
    #endif
    
    struct sockaddr_in listen_addr, forward_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];

    listen_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_socket < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_port = htons(args.listen_port);

    if (bind(listen_socket, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) {
        perror("Failed to bind socket");
        close(listen_socket);
        exit(EXIT_FAILURE);
    }

    memset(&forward_addr, 0, sizeof(forward_addr));
    forward_addr.sin_family = AF_INET;
    forward_addr.sin_port = htons(args.remote_port);
    if (inet_pton(AF_INET, args.forward_ip, &forward_addr.sin_addr) <= 0) {
        perror("Invalid forward IP address");
        close(listen_socket);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_interrupt);

    pthread_t stats_thread;
    if (pthread_create(&stats_thread, NULL, stats_printer, NULL) != 0) {
        perror("Failed to create stats thread");
        close(listen_socket);
        exit(EXIT_FAILURE);
    }

    pthread_t tcp_thread;
    if (pthread_create(&tcp_thread, NULL, tcp_server, &args) != 0) {
        perror("Failed to create TCP server thread");
        close(listen_socket);
        exit(EXIT_FAILURE);
    }

    if (!daemon_mode) {
        printf("Listening for UDP packets on port %d and forwarding to %s:%d\n", args.listen_port, args.forward_ip, args.remote_port);
    }

    while (1) {
        ssize_t received_bytes = recvfrom(listen_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if (received_bytes < 0) {
            perror("Failed to receive data");
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        update_stats(client_ip);

        sendto(listen_socket, buffer, received_bytes, 0, (struct sockaddr*)&forward_addr, sizeof(forward_addr));
    }

    close(listen_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
