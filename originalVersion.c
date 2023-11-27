#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

int num_cities;
int *distance_matrix;
int **memo;

void read_distance_matrix(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d", &num_cities);

    distance_matrix = (int *)malloc(num_cities * num_cities * sizeof(int));
    memo = (int **)malloc(num_cities * sizeof(int *));
    for (int i = 0; i < num_cities; ++i)
    {
        memo[i] = (int *)malloc((1 << num_cities) * sizeof(int));
        for (int j = 0; j < (1 << num_cities); ++j)
        {
            memo[i][j] = -1;
        }
    }

    for (int i = 0; i < num_cities; ++i)
    {
        for (int j = 0; j < num_cities; ++j)
        {
            fscanf(file, "%d", &distance_matrix[i * num_cities + j]);
        }
    }

    fclose(file);
}

// Function to get the elapsed time in seconds
long get_elapsed_time(struct timeval *start_time)
{
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    return (current_time.tv_sec - start_time->tv_sec) * 1000000 +
           (current_time.tv_usec - start_time->tv_usec);
}

int tsp(int current, int mask)
{
    if (mask == (1 << num_cities) - 1)
    {
        return distance_matrix[current * num_cities];
    }

    if (memo[current][mask] != -1)
    {
        return memo[current][mask];
    }

    int min_distance = INT_MAX;

    for (int next = 0; next < num_cities; ++next)
    {
        if (!(mask & (1 << next)))
        {
            int new_mask = mask | (1 << next);
            int cost = distance_matrix[current * num_cities + next] + tsp(next, new_mask);

            if (cost < min_distance)
            {
                min_distance = cost;
            }
        }
    }

    memo[current][mask] = min_distance;
    return min_distance;
}

int held_karp_parallel(int process_id, int num_processes, int max_time)
{
    int result;

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("Error creating process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        // Child process
        result = tsp(0, 1);
        exit(result);
    }
    else
    {
        // Parent process
        int child_status;
        waitpid(pid, &child_status, 0);
        result = WEXITSTATUS(child_status);
    }

    return result;
}

void worker_process(int process_id, int num_processes, int max_time)
{
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    while (get_elapsed_time(&start_time) < max_time)
    {
        held_karp_parallel(process_id, num_processes, max_time);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <filename> <num_processes> <max_time>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    read_distance_matrix(argv[1]);

    int num_processes = atoi(argv[2]);
    int max_time = atoi(argv[3]);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    int result = held_karp_parallel(0, num_processes, max_time); // Pass process_id as 0 for the main process

    gettimeofday(&end_time, NULL);
    long total_execution_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                                (end_time.tv_usec - start_time.tv_usec);

    printf("\n*** Original Version ***\n");
    // Print the result
    printf("Minimum distance: %d\n", result);
    printf("Total execution time: %ld ms\n", total_execution_time);
    printf("\n\n");

    // Clean up
    free(distance_matrix);
    for (int i = 0; i < num_cities; ++i)
    {
        free(memo[i]);
    }
    free(memo);

    return 0;
}