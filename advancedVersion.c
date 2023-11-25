#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define MAX_CITIES 20
#define MAX_ITERATIONS 10000

// Struct that will store the information about the best path possible
struct bestPath
{
    int path[MAX_CITIES];
    int distance;
};

// Shared memory and semaphores information
#define SHM_SIZE sizeof(struct bestPath)
#define SEM_KEY 1234
#define SHM_KEY 5678

//
void initializeRandomPath(int path[MAX_CITIES])
{
    for (int i = 0; i < MAX_CITIES; i++)
    {
        path[i] = i + 1;
    }

    // Shuffle the path
    for (int i = MAX_CITIES - 1; i > 0; --i)
    {
        int j = rand() % (i + 1);
        int temp = path[i];
        path[i] = path[j];
        path[j] = temp;
    }
}

// Function to perform exchange mutation
void exchangeMutation(int path[MAX_CITIES])
{
    int i = rand() % MAX_CITIES;
    int j = rand() % MAX_CITIES;

    // Prevent that the i and j are the same number
    while (i == j)
    {
        j = rand() % MAX_CITIES;
    }

    // Swap cities at position i and j
    int temp = path[i];
    path[i] = path[j];
    path[j] = temp;
}

int claculatePathTotalDistance(const int path[MAX_CITIES], const int distances[MAX_CITIES][MAX_CITIES])
{
    int totalDistance = 0;
    for (int i = 0; i < MAX_CITIES - 1; i++)
    {
        totalDistance += distances[path[i] - 1][path[i + 1] - 1];
    }

    // Add the distance from the last city back to the starting city
    totalDistance += distances[path[MAX_CITIES - 1] - 1][path[0] - 1];
    return totalDistance;
}

int readMatrix(int m[50][50], char *name, int *n)
{
    FILE *f = fopen(name, "r");
    if (f == NULL)
        return -1;

    fscanf(f, "%d", n);

    for (int i = 0; i < *n; i++)
        for (int j = 0; j < *n; j++)
            fscanf(f, "%d", &m[i][j]);

    return 0;
}

// Function to perform the AJ-PE Algorithm
void ajpe_algorithm(const int distances[MAX_CITIES][MAX_CITIES], struct bestPath *bestPath, int max_iterations)
{
    // Initialize a random path
    int currentPath[MAX_CITIES];
    initializeRandomPath(currentPath);

    // Calculate the initial distance
    int currentDistance = claculatePathTotalDistance(currentPath, distances);

    // Set the initial path and distance in the shared memory
    for (int i = 0; i < MAX_CITIES; i++)
    {
        bestPath->path[i] = currentPath[i];
    }
    bestPath->distance = currentDistance;

    // Algorithm loop
    for (int iteration = 0; iteration < max_iterations; iteration++)
    {
        // Apply exchange mutation
        exchangeMutation(currentPath);

        // Calculate new distance
        int newDistance = claculatePathTotalDistance(currentPath, distances);

        // Update the best path
        if (newDistance < bestPath->distance)
        {
            for (int i = 0; i < MAX_CITIES; i++)
            {
                bestPath->path[i] = currentPath[i];
            }
            bestPath->distance = newDistance;
        }
    }
}

int main(int argc, char **argv)
{
    int matrix[50][50];
    int n;

    // if (argc != 2)
    // {
    //     printf("Usage: tsp <file_name>\n");
    //     return -1;
    // }

    if (argc != 4)
    {
        printf("Usage: tsp <file_name> <num_processes> <max_execution_time>\n");
        return -1;
    }

    char filename[30];
    strcpy(filename, argv[1]);

    if (readMatrix(matrix, filename, &n) != 0)
    {
        printf("Error reading file!\n");
        return -1;
    }

    // Display the distance matrix
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
            printf("%2d ", matrix[i][j]);
        printf("\n");
    }

    // Create semaphore
    int sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    semctl(sem_id, 0, SETVAL, 1);

    // Create shared memory
    int shm_id = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    struct bestPath *shared_bestPath = (struct bestPath *)shmat(shm_id, NULL, 0);

    // Fork process
    for (int i = 0; i < atoi(argv[2]); i++)
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // Child process \\
            ajpe_algorithm(matrix, shared_bestPath, atoi(argv[3]));
            exit(EXIT_SUCCESS);
        }
    }

    // Parent proccess \\
    // Wait for child processes to finish
    for (int i = 0; i < atoi(argv[2]); ++i)
    {
        wait(NULL);
    }

    // Retrieve and print the best path info
    printf("Best path: ");
    for (int i = 0; i < n; i++)
    {
        printf("%d", shared_bestPath->path[i]);
    }
    printf("\nBest distance: %d\n", shared_bestPath->distance);

    // CleanUp
    shmdt(shared_bestPath);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    return 0;
}