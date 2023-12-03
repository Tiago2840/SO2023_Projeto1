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
#include <sys/time.h>
#include <sys/mman.h>

#define MAX_CITIES 20
#define MAX_ITERATIONS 1000000000

// Structure to represent a solution to the traveling salesman problem
typedef struct
{
    int path[MAX_CITIES]; // Array to store the best path found by algorithm
    int distance;         // Total distance of the stored path
    int total_iterations; // Counter for total iterations performed until best solution is found
} Solution;

// Global variables
int num_cities;                                              // Number of cities parsed
int *distance_matrix;                                        // Matrix to store distances between cities
Solution *shared_memory;                                     // Shared memory to store the best solution
sem_t *semaphore;                                            // Semaphore used for synchronization
int shm_id;                                                  // Shared memory identifier
struct timeval start_program_time, *current_time, best_time; // Sturct used to calculate time until best iteration

// Function to initialize shared memory and semaphore
void initialize_shared_memory()
{
    // Define protection and visibility flags for shared memory
    int sem_protection = PROT_READ | PROT_WRITE;
    int sem_visibility = MAP_ANONYMOUS | MAP_SHARED;

    // Map the current_time variable to shared memory
    current_time = mmap(NULL, sizeof(struct timeval), sem_protection, sem_visibility, 0, 0);

    // Generate a key for the shared memory
    key_t key = ftok(".", 'a');

    // Create a shared memory segment for the Solution structure
    shm_id = shmget(key, sizeof(Solution), IPC_CREAT | 0666);
    if (shm_id == -1)
    {
        perror("Error creating shared memory");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the address space
    shared_memory = (Solution *)shmat(shm_id, NULL, 0);
    if (shared_memory == (Solution *)-1)
    {
        perror("Error attaching shared memory");
        exit(EXIT_FAILURE);
    }

    // Initialize distance to a large value and total_iterations to zero
    shared_memory->distance = 100000;
    shared_memory->total_iterations = 0;

    // Initialize semaphore for synchronization
    semaphore = sem_open("/my_semaphore", O_CREAT | O_EXCL, 0666, 1);
    if (semaphore == SEM_FAILED)
    {
        perror("Error creating semaphore");
        exit(EXIT_FAILURE);
    }

    // Unlink the semaphore to remove it after the program ends
    sem_unlink("/my_semaphore");
}

// Function to generate a random path that will be used as a starting point in the algorithm
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
    int total_distance = 0;

    // Iterate through each city in the path
    for (int i = 0; i < num_cities - 1; ++i)
    {
        // Add the distance from city i to city i+1 to the total distance
        total_distance += distance_matrix[(path[i] - 1) * num_cities + (path[i + 1] - 1)];
    }

    // Add the distance from the last city back to the starting city
    total_distance += distance_matrix[(path[num_cities - 1] - 1) * num_cities + (path[0] - 1)];

    // Return the total distance of the entire path
    return total_distance;
}

// Function to perform exchange mutation on a given path in the traveling salesman problem
void exchange_mutation(int *path)
{
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

// Function to update shared memory with a new solution in the traveling salesman problem
void update_shared_memory(Solution *solution)
{
    // Wait for the semaphore to control access to shared memory
    sem_wait(semaphore);

    // Check if the new solution has a shorter distance or if shared_memory has not been initialized
    if (solution->distance < shared_memory->distance || shared_memory->distance == 0)
    {
        // Update the shared memory with the new solution
        *shared_memory = *solution;

        // Accumulate the total iterations performed across all solutions
        shared_memory->total_iterations += solution->total_iterations;
    }

    // Release the semaphore to allow other processes access to shared memory
    sem_post(semaphore);
}

// Function to get the elapsed time in milliseconds since the program started
long get_elapsed_time()
{
    // Get the current time
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Calculate the elapsed time in microseconds
    long elapsed_time = (current_time.tv_sec - start_program_time.tv_sec) * 1000000 +
                        (current_time.tv_usec - start_program_time.tv_usec);

    // Convert the elapsed time to milliseconds
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

    // Record the start time of the programm
    gettimeofday(&start_program_time, NULL);

    // Initialize the current solution with a random path
    generate_random_path(current_solution.path, num_cities);
    current_solution.distance = calculate_distance(current_solution.path);
    current_solution.total_iterations = 0;

    // If the mutated_distance > current_solution.distance then the current time will be the first process time
    gettimeofday(current_time, NULL);

    // While loop continues until the specified maximum time is reached
    while (difftime(time(NULL), start_time) < max_time)
    {
        // Apply mutation (exchange mutation)
        exchange_mutation(current_solution.path);

        // Calculate the distance for the mutated path
        int mutated_distance = calculate_distance(current_solution.path);

        // Update the current solution if the mutated path is better
        if (mutated_distance < current_solution.distance)
        {
            current_solution.distance = mutated_distance;
            gettimeofday(current_time, NULL);
        }

        // Increment the local iteration counter
        iteration++;
        current_solution.total_iterations = iteration;

        // Update the shared memory if the current solution is better
        update_shared_memory(&current_solution);
    }
}

int main(int argc, char *argv[])
{
    struct timeval start_time, end_time;

    // Record the start time of the program
    gettimeofday(&start_program_time, NULL);

    // Check if the correct number of params are provided in the command line
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

    // Parse the number of cities and allocate memory for the distance matrix
    fscanf(file, "%d", &num_cities);
    distance_matrix = (int *)malloc(num_cities * num_cities * sizeof(int));

    // Populate the distance matrix from the file
    for (int i = 0; i < num_cities; ++i)
    {
        for (int j = 0; j < num_cities; ++j)
        {
            fscanf(file, "%d", &distance_matrix[i * num_cities + j]);
        }
    }

    // Close the file after reading
    fclose(file);

    // Initialize shared memory and semaphore
    initialize_shared_memory();

    // Create processes
    for (int i = 0; i < num_processes; ++i)
    {
        // Fork each process to run the algorithm
        pid_t pid = fork();

        // Handle fork errors
        if (pid == -1)
        {
            perror("Error creating process");
            exit(EXIT_FAILURE);
        }

        // Code executed by child processes
        if (pid == 0)
        {
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

    // Calculate total execution time
    long total_execution_time = (end_time.tv_sec - start_program_time.tv_sec) * 1000000 +
                                (end_time.tv_usec - start_program_time.tv_usec);

    // Convert total execution time to ms
    total_execution_time /= 1000;

    // Print the best solution found
    printf("\n*** Base Version ***\n");
    printf("Best solution found: ");
    for (int i = 0; i < num_cities; ++i)
    {
        printf("%d ", shared_memory->path[i]);
    }
    printf("\nDistance: %d\n", shared_memory->distance);
    printf("Total iterations across all processes: %d\n", shared_memory->total_iterations);
    printf("Total execution time: %ld ms\n", total_execution_time);

    // Calculate best_time as the difference between current_time and start_program_time
    timersub(current_time, &start_program_time, &best_time);
    // Convert best_time to milliseconds
    long best_time_ms = best_time.tv_sec * 1000 + best_time.tv_usec / 1000;
    // Print the correct best_time
    printf("Best Time = %ld ms\n", best_time_ms);
    printf("\n\n");

    // Clean up - free allocated memory and remove shared memory
    free(distance_matrix);
    shmdt(shared_memory);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}
