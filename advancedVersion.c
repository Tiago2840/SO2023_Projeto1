#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>

#define MAX_CITIES 20
#define MAX_ITERATIONS 1000000000
#define MAX_PROCESSES 100

// Structure that stores the best solution
typedef struct
{
    int path[MAX_CITIES];
    int distance;
    int total_iterations;
    int process_id;
} Solution;

// Global variables
int num_cities;
int *distance_matrix;
Solution *shared_memory;
sem_t *semaphore;
int shm_id;
volatile sig_atomic_t update_signal = 0;
pid_t child_processes[MAX_PROCESSES];
int num_child_processes = 0;
struct timeval start_program_time, *current_time, best_time;
Solution best_solution;
int synchronized_this_iteration = 0;

// Function to initialize shared memory and semaphore
void initialize_shared_memory()
{
    // Set protection and visibility flags for memory mapping
    int sem_protection = PROT_READ | PROT_WRITE;
    int sem_visibility = MAP_ANONYMOUS | MAP_SHARED;

    // Allocate memory for current_time using memory mapping
    current_time = mmap(NULL, sizeof(struct timeval), sem_protection, sem_visibility, 0, 0);

    // Generate a key for shared memory using ftok
    key_t key = ftok(".", 'a');

    // Create a shared memory segment
    shm_id = shmget(key, sizeof(Solution), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Error creating shared memory");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the process
    shared_memory = (Solution *)shmat(shm_id, NULL, 0);
    if (shared_memory == (Solution *)-1)
    {
        perror("Error attaching shared memory");
        exit(EXIT_FAILURE);
    }

    // Initialize distance to a large value
    shared_memory->distance = 100000;

    // Initialize semaphore for synchronization
    semaphore = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (semaphore == SEM_FAILED)
    {
        perror("Error creating semaphore");
        exit(EXIT_FAILURE);
    }

    // Unlink the semaphore to remove it when the program finishes
    sem_unlink("/my_semaphore");
}

// Function to generate a random path of cities
void generate_random_path(int *path, int size)
{
    // Initialize the path with consecutive city numbers
    for (int i = 0; i < size; ++i)
    {
        path[i] = i + 1;
    }

    // Shuffle the path using the Fisher-Yates algorithm
    for (int i = size - 1; i > 0; --i)
    {
        // Generate a random index between 0 and i (inclusive)
        int j = rand() % (i + 1);

        // Swap path[i] and path[j]
        int temp = path[i];
        path[i] = path[j];
        path[j] = temp;
    }
}

// Function to calculate the total distance of a given path
int calculate_distance(int *path)
{
    // Initialize total distance to 0
    int total_distance = 0;

    // Iterate through the cities in the path
    for (int i = 0; i < num_cities - 1; ++i)
    {
        // Add the distance from city i to city i+1
        total_distance += distance_matrix[(path[i] - 1) * num_cities + (path[i + 1] - 1)];
    }

    // Add the distance from the last city back to the starting city
    total_distance += distance_matrix[(path[num_cities - 1] - 1) * num_cities + (path[0] - 1)];

    // Return the total distance of the path
    return total_distance;
}

// Function to perform exchange mutation on a path
void exchange_mutation(int *path)
{
    // Get the size of the path (number of cities)
    int size = num_cities;

    // Choose two random positions to exchange
    int position1 = rand() % size;
    int position2;

    // Check if both positions are not the same
    do
    {
        position2 = rand() % size;
    } while (position1 == position2);

    // Swap the cities at the chosen positions
    int temp = path[position1];
    path[position1] = path[position2];
    path[position2] = temp;
}

// Function to update shared memory with a new solution
void update_shared_memory(Solution *solution)
{
    // Wait for the semaphore to access shared memory
    sem_wait(semaphore);

    // Check if the new solution has a smaller distance or more iterations
    // than the solution in shared memory
    if (solution->distance < shared_memory->distance || solution->total_iterations > shared_memory->total_iterations)
    {
        // Update the shared memory with the new solution
        *shared_memory = *solution;

        // Notify the parent process about the update using a signal
        kill(getppid(), SIGUSR1);

        // Set the flag to indicate synchronization during this iteration
        synchronized_this_iteration = 1;
    }

    // Release the semaphore
    sem_post(semaphore);
}

// Function to synchronize child processes by sending signals
void synchronize_processes()
{
    // Send SIGUSR1 signal to each child process
    for (int i = 0; i < num_child_processes; ++i)
    {
        kill(child_processes[i], SIGUSR1);
    }

    // Introduce a small delay to allow signals to be processed
    usleep(10000);
}

// Signal handler function for SIGUSR1
void handle_update_signal(int signo)
{
    // Set the update_signal flag if the signal is SIGUSR1
    if (signo == SIGUSR1)
    {
        update_signal = 1;
    }

    // Only synchronize if the synchronized_this_iteration flag is set
    if (synchronized_this_iteration)
    {
        // Synchronize child processes
        synchronize_processes();

        // Reset the flag after synchronization
        synchronized_this_iteration = 0;
    }
}

// Function to get the elapsed time in milliseconds
long get_elapsed_time()
{
    // Get the current time
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Calculate the elapsed time in microseconds
    long elapsed_time = (current_time.tv_sec - start_program_time.tv_sec) * 1000000 +
                        (current_time.tv_usec - start_program_time.tv_usec);

    // Convert elapsed time to milliseconds
    return elapsed_time / 1000;
}

// Function to run the algorithm
void run_algorithm(int process_id, int num_processes, int max_time)
{
    // Local variables for the current process
    Solution current_solution;
    int iteration = 0;
    time_t start_time = time(NULL);
    srand((unsigned int)time(NULL));

    // Initialize the current solution with a random path
    generate_random_path(current_solution.path, num_cities);
    current_solution.distance = calculate_distance(current_solution.path);

    current_solution.process_id = process_id; // Set process_id
    current_solution.total_iterations = 0;    // Initialize total iterations

    // Main loop for the genetic algorithm
    while (difftime(time(NULL), start_time) < max_time)
    {
        // If an update signal is received and not synchronized in the current iteration,
        // synchronize paths and resume activity
        if (update_signal && !synchronized_this_iteration)
        {
            // Wait for the semaphore to access shared memory
            sem_wait(semaphore);

            // Synchronize paths with the shared memory
            for (int i = 0; i < num_cities; ++i)
            {
                current_solution.path[i] = shared_memory->path[i];
            }
            current_solution.distance = shared_memory->distance;
            current_solution.total_iterations = shared_memory->total_iterations;

            // Release the semaphore
            sem_post(semaphore);

            // Print synchronization information
            printf("Process %d synchronized with updated path. New distance: %d\n", process_id, current_solution.distance);

            // Reset the update signal and set the flag
            update_signal = 0;
            synchronized_this_iteration = 1;
        }

        // Apply mutation (exchange mutation)
        exchange_mutation(current_solution.path);

        // Calculate the distance for the mutated path
        int mutated_distance = calculate_distance(current_solution.path);

        // Update the current solution if the mutated path is better
        if (mutated_distance < current_solution.distance)
        {
            current_solution.distance = mutated_distance;
        }

        // Update the shared memory if the current solution is better
        update_shared_memory(&current_solution);

        // Increment the iteration counter and update total iterations
        iteration++;
        current_solution.total_iterations = iteration;

        // Check if the current solution is the best
        if (current_solution.distance < best_solution.distance)
        {
            best_solution = current_solution;

            // Update the best time
            gettimeofday(current_time, NULL);
        }
    }
}

// Main function
int main(int argc, char *argv[])
{
    // Record the start time of the program
    gettimeofday(&start_program_time, NULL);

    // Check if the correct number of command-line arguments is provided
    if (argc != 4)
    {
        printf("Usage: %s <filename> <num_processes> <max_time>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    char *filename = argv[1];
    int num_processes = atoi(argv[2]);
    int max_time = atoi(argv[3]);

    // Read distance matrix from file
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Parse the number of cities
    fscanf(file, "%d", &num_cities);

    // Allocate memory for the distance matrix
    distance_matrix = (int *)malloc(num_cities * num_cities * sizeof(int));

    // Read distances from the file into the distance matrix
    for (int i = 0; i < num_cities; ++i)
    {
        for (int j = 0; j < num_cities; ++j)
        {
            fscanf(file, "%d", &distance_matrix[i * num_cities + j]);
        }
    }

    fclose(file);

    // Initialize best_solution
    best_solution.distance = 100000;
    best_solution.total_iterations = 0;
    best_solution.process_id = -1; // Some invalid process ID to indicate uninitialized state
    for (int i = 0; i < MAX_CITIES; ++i)
    {
        best_solution.path[i] = i + 1;
    }

    // Initialize shared memory and semaphore
    initialize_shared_memory();

    // Attach the signal handler
    if (signal(SIGUSR1, handle_update_signal) == SIG_ERR)
    {
        perror("Error attaching signal handler");
        exit(EXIT_FAILURE);
    }

    // Create processes
    for (int i = 0; i < num_processes; ++i)
    {
        pid_t pid = fork();

        if (pid == -1)
        {
            perror("Error creating process");
            exit(EXIT_FAILURE);
        }

        if (pid == 0)
        {
            // Child process
            run_algorithm(i, num_processes, max_time);
            exit(EXIT_SUCCESS);
        }
        else
        {
            // Parent process
            child_processes[num_child_processes++] = pid;
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_processes; ++i)
    {
        wait(NULL);
    }

    // Print the best solution found and the time it took
    printf("\n*** Advanced Version ***\n");
    printf("Best solution found by Process %d with %d iterations\n", shared_memory->process_id, shared_memory->total_iterations);

    // Calculate the best time
    timersub(current_time, &start_program_time, &best_time);
    long best_time_ms = best_time.tv_sec * 1000 + best_time.tv_usec / 1000; // Convert to milliseconds
    printf("Best Time = %ld ms\n", best_time_ms);

    // Print the best path
    printf("Path: ");
    for (int i = 0; i < num_cities; ++i)
    {
        printf("%d ", shared_memory->path[i]);
    }
    printf("\nDistance: %d\n", shared_memory->distance);

    // Calculate total execution time in milliseconds
    long total_execution_time = get_elapsed_time();
    printf("Total execution time: %ld ms\n", total_execution_time);
    printf("\n\n");

    // Clean up
    free(distance_matrix);
    shmdt(shared_memory);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}
