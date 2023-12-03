#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

int num_cities;       // Number of cities in the problem
int *distance_matrix; // Matrix containing distances between cities
int **memo;           // Memoization table for dynamic programming

// Function to read the distance matrix from a file
void read_distance_matrix(const char *filename)
{
    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read the number of cities from the file
    fscanf(file, "%d", &num_cities);

    // Allocate memory for the distance matrix and memoization table
    distance_matrix = (int *)malloc(num_cities * num_cities * sizeof(int));
    memo = (int **)malloc(num_cities * sizeof(int *));
    for (int i = 0; i < num_cities; ++i)
    {
        memo[i] = (int *)malloc((1 << num_cities) * sizeof(int));

        // Initialize memoization table with -1
        for (int j = 0; j < (1 << num_cities); ++j)
        {
            memo[i][j] = -1;
        }
    }

    // Read the distance matrix from the file
    for (int i = 0; i < num_cities; ++i)
    {
        for (int j = 0; j < num_cities; ++j)
        {
            fscanf(file, "%d", &distance_matrix[i * num_cities + j]);
        }
    }

    // Close the file
    fclose(file);
}

// Function to get the elapsed time
long get_elapsed_time(struct timeval *start_time)
{
    // Get the current time
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Calculate the elapsed time
    return (current_time.tv_sec - start_time->tv_sec) * 1000000 +
           (current_time.tv_usec - start_time->tv_usec);
}

// Function to solve the Traveling Salesman Problem (TSP) using dynamic programming
// Returns the minimum distance for a given current city and mask of visited cities
int tsp(int current, int mask)
{
    // Base case: All cities visited
    if (mask == (1 << num_cities) - 1)
    {
        // Return the distance from the current city back to the starting city
        return distance_matrix[current * num_cities];
    }

    // Check if the solution for the current state is already memoized
    if (memo[current][mask] != -1)
    {
        // Return the memoized value
        return memo[current][mask];
    }

    int min_distance = INT_MAX;

    // Iterate over all cities
    for (int next = 0; next < num_cities; ++next)
    {
        // Check if the next city has not been visited
        if (!(mask & (1 << next)))
        {
            // Update the mask to mark the next city as visited
            int new_mask = mask | (1 << next);

            // Recursively calculate the cost of visiting the next city
            int cost = distance_matrix[current * num_cities + next] + tsp(next, new_mask);

            // Update the minimum distance if the new cost is smaller
            if (cost < min_distance)
            {
                min_distance = cost;
            }
        }
    }

    // Memoize the result for the current state
    memo[current][mask] = min_distance;

    // Return the minimum distance for the current state
    return min_distance;
}

// Function to solve the Traveling Salesman Problem (TSP) in parallel using the Held-Karp algorithm
// Returns the minimum distance for the TSP
int held_karp_parallel(int process_id, int num_processes, int max_time)
{
    int result; // Variable to store the result of the TSP

    // Create a child process
    pid_t pid = fork();

    if (pid == -1)
    {
        // Error handling: Print an error message and exit
        perror("Error creating process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        // Child process
        // Perform TSP computation starting from the first city (0)
        // with the initial mask indicating that only the first city is visited (1)
        result = tsp(0, 1);

        // Exit with the result to be collected by the parent process
        exit(result);
    }
    else
    {
        // Parent process
        int child_status;

        // Wait for the child process to finish and collect its exit status
        waitpid(pid, &child_status, 0);

        // Extract the result from the child process's exit status
        result = WEXITSTATUS(child_status);
    }

    // Return the result of the TSP computation
    return result;
}

// Worker process function that repeatedly performs Held-Karp parallel computations
void worker_process(int process_id, int num_processes, int max_time)
{
    // Variables to track time
    struct timeval start_time, current_time;

    // Get the start time for measuring elapsed time
    gettimeofday(&start_time, NULL);

    // Continue processing until the elapsed time reaches the maximum time limit
    while (get_elapsed_time(&start_time) < max_time)
    {
        // Perform Held-Karp parallel computation
        held_karp_parallel(process_id, num_processes, max_time);
    }
}

// Main function for the Traveling Salesman Problem (TSP) program
int main(int argc, char *argv[])
{
    // Check if the correct number of command-line arguments is provided
    if (argc != 4)
    {
        printf("Usage: %s <filename> <num_processes> <max_time>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Read the distance matrix from the specified file
    read_distance_matrix(argv[1]);

    // Parse command line arguments for the number of processes and maximum time
    int num_processes = atoi(argv[2]);
    int max_time = atoi(argv[3]);

    // Variables to track program execution time
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    // Perform Held-Karp parallel computation in the main process
    int result = held_karp_parallel(0, num_processes, max_time);

    // Get the end time for calculating total execution time
    gettimeofday(&end_time, NULL);
    long total_execution_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                                (end_time.tv_usec - start_time.tv_usec);

    // Print results
    printf("\n*** Original Version ***\n");
    printf("Minimum distance: %d\n", result);
    printf("Total execution time: %ld ms\n", total_execution_time);
    printf("\n\n");

    // Clean up allocated memory
    free(distance_matrix);
    for (int i = 0; i < num_cities; ++i)
    {
        free(memo[i]);
    }
    free(memo);

    return 0;
}
