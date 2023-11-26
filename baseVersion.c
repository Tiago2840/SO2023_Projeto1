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

#define MAX_CITIES 20
#define MAX_ITERATIONS 10000

typedef struct
{
    int path[MAX_CITIES];
    int distance;
    int total_iterations; // Added iteration counter
} Solution;

// Global variables
int num_cities;
int *distance_matrix;
Solution *shared_memory;
sem_t *semaphore;
int shm_id;
volatile sig_atomic_t update_signal = 0;
pid_t child_processes[MAX_ITERATIONS];
int num_child_processes = 0;

// Function declarations
void initialize_shared_memory();
void generate_random_path(int *path, int size);
int calculate_distance(int *path);
void exchange_mutation(int *path);
void update_shared_memory(Solution *solution);
void run_algorithm(int process_id, int num_processes, int max_time);

int main(int argc, char *argv[])
{
    struct timeval start_time, end_time;

    // Record the start time
    gettimeofday(&start_time, NULL);

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

    fscanf(file, "%d", &num_cities);

    distance_matrix = (int *)malloc(num_cities * num_cities * sizeof(int));
    for (int i = 0; i < num_cities; ++i)
    {
        for (int j = 0; j < num_cities; ++j)
        {
            fscanf(file, "%d", &distance_matrix[i * num_cities + j]);
        }
    }

    fclose(file);

    // Initialize shared memory and semaphore
    initialize_shared_memory();

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
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_processes; ++i)
    {
        wait(NULL);
    }

    // Record the end time
    gettimeofday(&end_time, NULL);

    // Calculate total execution time in microseconds
    long total_execution_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                                (end_time.tv_usec - start_time.tv_usec);

    // Convert to milliseconds
    total_execution_time /= 1000;

    // Print the best solution found
    printf("Best solution found: ");
    for (int i = 0; i < num_cities; ++i)
    {
        printf("%d ", shared_memory->path[i]);
    }
    printf("\nDistance: %d\n", shared_memory->distance);
    printf("Total iterations across all processes: %d\n", shared_memory->total_iterations);
    printf("Total execution time: %ld ms\n", total_execution_time);

    // Clean up
    free(distance_matrix);
    shmdt(shared_memory);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}

void initialize_shared_memory()
{
    key_t key = ftok(".", 'a');
    shm_id = shmget(key, sizeof(Solution), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Error creating shared memory");
        exit(EXIT_FAILURE);
    }

    shared_memory = (Solution *)shmat(shm_id, NULL, 0);
    if (shared_memory == (Solution *)-1)
    {
        perror("Error attaching shared memory");
        exit(EXIT_FAILURE);
    }

    // Initialize distance to a large value
    shared_memory->distance = 100000;
    shared_memory->total_iterations = 0; // Initialize iteration counter

    // Initialize semaphore
    semaphore = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (semaphore == SEM_FAILED)
    {
        perror("Error creating semaphore");
        exit(EXIT_FAILURE);
    }

    sem_unlink("/my_semaphore");
}

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

int calculate_distance(int *path)
{
    int total_distance = 0;

    for (int i = 0; i < num_cities - 1; ++i)
    {
        // Add the distance from city i to city i+1
        total_distance += distance_matrix[(path[i] - 1) * num_cities + (path[i + 1] - 1)];
    }

    // Add the distance from the last city back to the starting city
    total_distance += distance_matrix[(path[num_cities - 1] - 1) * num_cities + (path[0] - 1)];

    return total_distance;
}

void exchange_mutation(int *path)
{
    int size = num_cities;

    // Choose two random positions to exchange
    int position1 = rand() % size;
    int position2;
    do
    {
        position2 = rand() % size;
    } while (position1 == position2);

    // Swap the cities at the chosen positions
    int temp = path[position1];
    path[position1] = path[position2];
    path[position2] = temp;
}

void update_shared_memory(Solution *solution)
{
    sem_wait(semaphore);

    if (solution->distance < shared_memory->distance || shared_memory->distance == 0)
    {
        *shared_memory = *solution;
        shared_memory->total_iterations += solution->total_iterations;
        printf("Updated shared memory. New distance: %d\n", shared_memory->distance);
    }

    sem_post(semaphore);
}

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
    current_solution.total_iterations = 0; // Initialize local iteration counter
    printf("Initial shared memory distance: %d\n", shared_memory->distance);

    while (iteration < MAX_ITERATIONS && difftime(time(NULL), start_time) < max_time)
    {
        // Apply mutation (exchange mutation)
        exchange_mutation(current_solution.path);

        // Calculate the distance for the mutated path
        int mutated_distance = calculate_distance(current_solution.path);

        // Update the current solution if the mutated path is better
        if (mutated_distance < current_solution.distance)
        {
            current_solution.distance = mutated_distance;
            // You may want to update other information in the solution as well
        }

        // Increment the local iteration counter
        iteration++;
        current_solution.total_iterations = iteration;

        // Update the shared memory if the current solution is better
        update_shared_memory(&current_solution);
    }
}