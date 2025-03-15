#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_TASKS 100
#define MAX_TASK_LENGTH 50
#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 256

// Task structure
typedef struct {
    char task[MAX_TASK_LENGTH];
    int assigned;
    int completed;
    pid_t assigned_to;
} Task;

// Global variables
Task task_queue[MAX_TASKS];
int task_count = 0;
int server_fd;

// Function to handle zombie processes
void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;
    int status;
    
    // Collect all dead child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d terminated\n", pid);
        
        // Mark task as unassigned if the process terminated without completing
        for (int i = 0; i < task_count; i++) {
            if (task_queue[i].assigned && task_queue[i].assigned_to == pid && !task_queue[i].completed) {
                task_queue[i].assigned = 0;
                task_queue[i].assigned_to = 0;
                printf("Task '%s' is back in the queue\n", task_queue[i].task);
            }
        }
    }
    
    errno = saved_errno;
}

// Function to load tasks from config file
int load_tasks(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening task file");
        return -1;
    }
    
    char line[MAX_TASK_LENGTH];
    while (fgets(line, sizeof(line), file) && task_count < MAX_TASKS) {
        // Remove newline character
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        strcpy(task_queue[task_count].task, line);
        task_queue[task_count].assigned = 0;
        task_queue[task_count].completed = 0;
        task_queue[task_count].assigned_to = 0;
        task_count++;
    }
    
    fclose(file);
    printf("Loaded %d tasks\n", task_count);
    return 0;
}

// Function to get an unassigned task
int get_next_task_index() {
    for (int i = 0; i < task_count; i++) {
        if (!task_queue[i].assigned && !task_queue[i].completed) {
            return i;
        }
    }
    return -1;
}

// Function to handle client requests
void handle_client(int client_fd, pid_t child_pid) {
    char buffer[BUFFER_SIZE];
    int task_index = -1;
    
    // Make socket non-blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    while (1) {
        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Try to read from client
        int n = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, try again later
                sleep(1);
                continue;
            } else {
                perror("Error reading from socket");
                break;
            }
        } else if (n == 0) {
            // Client closed connection
            printf("Client disconnected\n");
            break;
        }
        
        // Process client message
        buffer[n] = '\0';
        printf("Client %d: %s\n", child_pid, buffer);
        
        // Handle client request
        if (strncmp(buffer, "GET_TASK", 8) == 0) {
            // Check if client already has a task
            int has_pending_task = 0;
            for (int i = 0; i < task_count; i++) {
                if (task_queue[i].assigned && task_queue[i].assigned_to == child_pid && !task_queue[i].completed) {
                    has_pending_task = 1;
                    task_index = i;
                    break;
                }
            }
            
            if (has_pending_task) {
                char response[BUFFER_SIZE];
                sprintf(response, "Task: %s", task_queue[task_index].task);
                write(client_fd, response, strlen(response));
                printf("Re-sent task to client %d: %s\n", child_pid, response);
            } else {
                // Find new task for client
                task_index = get_next_task_index();
                
                if (task_index >= 0) {
                    task_queue[task_index].assigned = 1;
                    task_queue[task_index].assigned_to = child_pid;
                    
                    char response[BUFFER_SIZE];
                    sprintf(response, "Task: %s", task_queue[task_index].task);
                    write(client_fd, response, strlen(response));
                    printf("Sent task to client %d: %s\n", child_pid, response);
                } else {
                    // No tasks available
                    write(client_fd, "No tasks available", 17);
                    printf("No tasks available for client %d\n", child_pid);
                }
            }
        } else if (strncmp(buffer, "RESULT ", 7) == 0) {
            // Process result
            if (task_index >= 0) {
                task_queue[task_index].completed = 1;
                printf("Task completed by client %d: %s = %s\n", 
                       child_pid, task_queue[task_index].task, buffer + 7);
                
                // Check if all tasks are completed
                int all_completed = 1;
                for (int i = 0; i < task_count; i++) {
                    if (!task_queue[i].completed) {
                        all_completed = 0;
                        break;
                    }
                }
                
                if (all_completed) {
                    printf("All tasks completed!\n");
                }
            }
        } else if (strncmp(buffer, "exit", 4) == 0) {
            printf("Client %d requested to exit\n", child_pid);
            break;
        }
    }
    
    close(client_fd);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <task_file>\n", argv[0]);
        return 1;
    }
    
    // Load tasks from file
    if (load_tasks(argv[1]) < 0) {
        return 1;
    }
    
    // Set up signal handler for child processes
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        return 1;
    }
    
    // Make server socket non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Prepare address structure
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    // Listen for connections
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        return 1;
    }
    
    printf("Task Queue Server started on port %d\n", PORT);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept client connection (non-blocking)
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, continue
                usleep(100000); // 100ms
                continue;
            } else {
                perror("Accept failed");
                continue;
            }
        }
        
        printf("New client connected\n");
        
        // Fork a child process to handle the client
        pid_t child_pid = fork();
        
        if (child_pid < 0) {
            perror("Fork failed");
            close(client_fd);
        } else if (child_pid == 0) {
            // Child process - handle client
            close(server_fd); // Close server socket in child
            handle_client(client_fd, getpid());
            // Child process will exit in handle_client
        } else {
            // Parent process - continue accepting connections
            close(client_fd); // Close client socket in parent
            printf("Spawned child process %d for client\n", child_pid);
        }
    }
    
    close(server_fd);
    return 0;
}