/*=====================================
Assignment 5 Submission
Name: Chandransh Singh
Roll number: 22CS30017
=====================================*/


/*
command to compile:
gcc server.c -o server

to run
./server tasks.txt
*/

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
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define MAX_TASKS 100
#define MAX_TASK_LENGTH 50
#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 256
#define SEM_KEY 12345
#define CTRL_SHM_KEY 56789

// Task structure
typedef struct {
    char task[MAX_TASK_LENGTH];
    int assigned;
    int completed;
    pid_t assigned_to;
    time_t assignment_time;  // Time when the task was assigned
} Task;

// Control structure (stored in separate shared memory segment)
typedef struct {
    int all_tasks_completed;
    int active_clients;  // Track the number of connected clients
} ServerControl;

// Global variables
Task *task_queue;         // Will be in shared memory
ServerControl *control;   // Control structure in shared memory
int task_count = 0;
int server_fd;
int shm_id;              // Shared memory ID for tasks
int ctrl_shm_id;         // Shared memory ID for control struct
int sem_id;              // Semaphore ID

// Define semaphore operations
struct sembuf sem_lock = {0, -1, 0};   // Lock operation
struct sembuf sem_unlock = {0, 1, 0};  // Unlock operation

// Function to lock the semaphore
void lock_queue() {
    if (semop(sem_id, &sem_lock, 1) == -1) {
        perror("### semop lock failed");
        exit(1);
    }
}

// Function to unlock the semaphore
void unlock_queue() {
    if (semop(sem_id, &sem_unlock, 1) == -1) {
        perror("### semop unlock failed");
        exit(1);
    }
}

// Function to check if all tasks are completed
int check_all_completed() {
    for (int i = 0; i < task_count; i++) {
        if (!task_queue[i].completed) {
            return 0;
        }
    }
    return 1;
}

// Function to check for task timeouts (for tasks that were assigned but not completed)
void check_task_timeouts() {
    time_t current_time = time(NULL);
    const int TIMEOUT_SECONDS = 30;  // Timeout after 30 seconds of inactivity
    
    lock_queue();
    
    for (int i = 0; i < task_count; i++) {
        if (task_queue[i].assigned && !task_queue[i].completed) {
            // Check if the task has timed out
            if (current_time - task_queue[i].assignment_time > TIMEOUT_SECONDS) {
                printf("*** Task '%s' timed out for client %d, putting back in queue\n", 
                       task_queue[i].task, task_queue[i].assigned_to);
                task_queue[i].assigned = 0;
                task_queue[i].assigned_to = 0;
            }
        }
    }
    
    unlock_queue();
}

// Function to handle zombie processes
void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;
    int status;
    
    // Collect all dead child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("--- Child [%d]: process terminated\n", pid);
        
        // Decrement active clients count
        lock_queue();
        control->active_clients--;
        printf("~~~ Active clients: %d\n", control->active_clients);
        
        // Mark task as unassigned if the process terminated without completing
        for (int i = 0; i < task_count; i++) {
            if (task_queue[i].assigned && task_queue[i].assigned_to == pid && !task_queue[i].completed) {
                task_queue[i].assigned = 0;
                task_queue[i].assigned_to = 0;
                printf("*** Task '%s' is back in the queue\n", task_queue[i].task);
            }
        }
        unlock_queue();
    }
    
    // Re-establish the signal handler for SIGCHLD
    signal(SIGCHLD, sigchld_handler);
    
    errno = saved_errno;
}

// Function to load tasks from config file
int load_tasks(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("### Error opening task file");
        return -1;
    }
    
    char line[MAX_TASK_LENGTH];
    int local_count = 0;
    
    // Read all tasks from file
    while (fgets(line, sizeof(line), file) && local_count < MAX_TASKS) {
        // Remove newline character
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        strcpy(task_queue[local_count].task, line);
        task_queue[local_count].assigned = 0;
        task_queue[local_count].completed = 0;
        task_queue[local_count].assigned_to = 0;
        task_queue[local_count].assignment_time = 0;
        local_count++;
    }
    
    task_count = local_count;
    fclose(file);
    printf(">>> Loaded %d tasks\n", task_count);
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
    time_t last_activity = time(NULL);
    const int CLIENT_IDLE_TIMEOUT = 60;  // Disconnect client after 60 seconds of inactivity
    
    /* Make socket non-blocking */
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    while (1) {
        // check idleness
        if(time(NULL) - last_activity > CLIENT_IDLE_TIMEOUT) {
            printf("^^^ Client [%d]: idle timeout\n", child_pid);
            break;
        }

        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Try to read from client using recv()
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, try again later
                sleep(1);
                continue;
            } else {
                perror("### Error reading from socket");
                break;
            }
        } else if (n == 0) {
            // Client closed connection
            printf("^^^ Client disconnected\n");
            
            // Release any task assigned to this client
            lock_queue();
            for (int i = 0; i < task_count; i++) {
                if (task_queue[i].assigned && task_queue[i].assigned_to == child_pid && !task_queue[i].completed) {
                    task_queue[i].assigned = 0;
                    task_queue[i].assigned_to = 0;
                    printf("*** Client [%d]: disconnected, task '%s' is back in the queue\n", 
                           child_pid, task_queue[i].task);
                }
            }
            unlock_queue();
            break;
        }

        // successfully read n bytes
        last_activity = time(NULL);
        
        // Process client message
        buffer[n] = '\0';
        printf("--> Client [%d]: %s\n", child_pid, buffer);
        
        // Handle client request
        if (strncmp(buffer, "GET_TASK", 8) == 0) {
            // Check if client already has a task
            int has_pending_task = 0;
            
            lock_queue();
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
                
                // Send response using send()
                if (send(client_fd, response, strlen(response), 0) < 0) {
                    perror("### Send failed");
                    unlock_queue();
                    break;
                }
                
                printf("<-- Client [%d]: Re-sent %s\n", child_pid, response);
                unlock_queue();
            } else {
                // Find new task for client
                task_index = get_next_task_index();
                
                if (task_index >= 0) {
                    task_queue[task_index].assigned = 1;
                    task_queue[task_index].assigned_to = child_pid;
                    task_queue[task_index].assignment_time = time(NULL);
                    
                    char response[BUFFER_SIZE];
                    sprintf(response, "Task: %s", task_queue[task_index].task);
                    
                    // Send response using send()
                    if (send(client_fd, response, strlen(response), 0) < 0) {
                        perror("### Send failed");
                        unlock_queue();
                        break;
                    }
                    
                    printf("<-- Client [%d]: Sent %s\n", child_pid, response);
                } else {
                    // No tasks available
                    if (send(client_fd, "No tasks available", 18, 0) < 0) {
                        perror("### Send failed");
                        unlock_queue();
                        break;
                    }
                    printf("<-- (^_^) No tasks available for client [%d]\n", child_pid);
                }
                unlock_queue();
            }
        } else if (strncmp(buffer, "RESULT ", 7) == 0) {
            // Process result
            lock_queue();
            // Find the task assigned to this client
            for (int i = 0; i < task_count; i++) {
                if (task_queue[i].assigned && task_queue[i].assigned_to == child_pid && !task_queue[i].completed) {
                    task_index = i;
                    break;
                }
            }
            
            if (task_index >= 0) {
                task_queue[task_index].completed = 1;
                printf("=== Client [%d]: Task completed: %s = %s\n", 
                       child_pid, task_queue[task_index].task, buffer + 7);
                
                // Check if all tasks are completed
                int all_completed = check_all_completed();
                
                if (all_completed) {
                    printf(";D All tasks completed!\n");
                    // Set the flag for the main server process to terminate
                    control->all_tasks_completed = 1;
                }
            }
            unlock_queue();
        } else if (strncmp(buffer, "exit", 4) == 0) {
            printf("--> Client [%d]: requested to exit\n", child_pid);
            
            // Release any task assigned to this client
            lock_queue();
            for (int i = 0; i < task_count; i++) {
                if (task_queue[i].assigned && task_queue[i].assigned_to == child_pid && !task_queue[i].completed) {
                    task_queue[i].assigned = 0;
                    task_queue[i].assigned_to = 0;
                    printf("*** Client [%d]: exited, task '%s' is back in the queue\n", 
                           child_pid, task_queue[i].task);
                }
            }
            unlock_queue();
            break;
        }
        
        // Periodically check for timed-out tasks
        check_task_timeouts();
    }
    
    close(client_fd);
    exit(0);
}

// Function to terminate all child processes
void terminate_all_children() {
    printf("--- Terminating all child processes with SIGKILL...\n");
    
    // First try to terminate gracefully with SIGTERM
    kill(0, SIGTERM);
    
    // Wait for a moment to give processes time to terminate
    sleep(1);
    
    // Force kill any remaining processes with SIGKILL
    kill(0, SIGKILL);
}

// Signal handler for graceful termination
void termination_handler(int sig) {
    // printf("!!! Received termination signal %d. Cleaning up...\n", sig);
    
    // Clean up resources
    close(server_fd);
    shmdt(task_queue);
    shmdt(control);
    shmctl(shm_id, IPC_RMID, NULL);
    shmctl(ctrl_shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    
    printf("$$$ Server terminated successfully $$$\n");
    exit(0);
}

int setup_server(){
    // Create shared memory segments
    if((shm_id = shmget(IPC_PRIVATE, sizeof(Task)*MAX_TASKS, IPC_CREAT | 0666)) < 0){
        perror("### shmget failed for task queue");
        return -1;
    }
    if((ctrl_shm_id = shmget(CTRL_SHM_KEY, sizeof(ServerControl), IPC_CREAT | 0666)) < 0){
        perror("### shmget failed for control structure");
        return -1;
    }
    // Attach shared memory segments
    if((task_queue = (Task *) shmat(shm_id, NULL, 0)) == (Task *) -1){
        perror("### shmat failed for task queue");
        return -1;
    }
    if((control = (ServerControl *) shmat(ctrl_shm_id, NULL, 0)) == (ServerControl *) -1){
        perror("### shmat failed for control structure");
        return -1;
    }
    control->all_tasks_completed = 0;
    control->active_clients = 0;

    // Create semaphore for task queue
    if((sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666)) < 0){
        perror("### semget failed");
        return -1;
    }
    // Initialize semaphore to 1 (unlocked)
    if(semctl(sem_id, 0, SETVAL, 1) == -1){
        perror("### semctl failed");
        return -1;
    }
    
    // Load tasks from file
    if(load_tasks("tasks.txt") < 0){
        perror("### Error loading tasks");
        return -1;
    }

    // Set up signal handlers
    if(signal(SIGCHLD, sigchld_handler) == SIG_ERR){
        perror("### signal for SIGCHLD");
        return -1;
    }
    if(signal(SIGINT, termination_handler) == SIG_ERR){
        perror("### signal for SIGINT");
        return -1;
    }
    if(signal(SIGTERM, termination_handler) == SIG_ERR){
        perror("### signal for SIGTERM");
        return -1;
    }

    // Create socket
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("### Socket creation failed");
        return -1;
    }
    // Set socket options
    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("### Setsockopt failed");
        return -1;
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
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
        perror("### Bind failed");
        return -1;
    }

    // Listen for connections
    if(listen(server_fd, BACKLOG) < 0){
        perror("### Listen failed");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <task_file>\n", argv[0]);
        return 1;
    }
    
    if(setup_server() < 0){
        fprintf(stderr, "### Server setup failed\n");
        return 1;
    }
    printf("+-+ Task Queue Server started on port %d\n", PORT);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Check for timed-out tasks in the main thread
        check_task_timeouts();
        
        // Check if all tasks are completed and there are no active clients
        if (control->all_tasks_completed && control->active_clients == 0) {
            printf("!!! All tasks completed and no active clients. Shutting down...\n");
            break;  // Exit the main server loop
        }
        
        // Accept client connection (non-blocking)
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, continue
                usleep(100000); // 100ms
                continue;
            } else {
                perror("### Accept failed");
                continue;
            }
        }
        
        printf("+++ New client connected\n");
        
        // Increment the active clients counter
        lock_queue();
        control->active_clients++;
        printf("~~~ Active clients: %d\n", control->active_clients);
        unlock_queue();
        
        // Fork a child process to handle the client
        pid_t child_pid = fork();
        
        if (child_pid < 0) {
            perror("### Fork failed");
            close(client_fd);
            
            // Decrement the active clients counter on failure
            lock_queue();
            control->active_clients--;
            unlock_queue();
        } else if (child_pid == 0) {
            // Child process - handle client
            close(server_fd); // Close server socket in child
            handle_client(client_fd, getpid());
            // Child process will exit in handle_client
        } else {
            // Parent process - continue accepting connections
            close(client_fd); // Close client socket in parent
            printf("++> Forked child process [%d] for client\n", child_pid);
        }
    }
    
    // Send termination signal to all child processes using SIGKILL
    terminate_all_children(); //and cleanup


    return 0;
}