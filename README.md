# SO2023_Projeto1

<div>
    <br>
    <b>Trabalho desenvolvido por:</b>
    <ol>
        <li><a href="https://github.com/laranjeira15">João Espada (202100660)</a></li>
        <li><a href="https://github.com/Tiago2840">Tiago Silva (202000331)</a></li>
    </ol>
</div>

## Table of Contents

1. [Base/Advanced General Functions](#general-functions)
2. [Base Version](#base-version)
3. [Advanced Version](#advanced-version)
4. [Base vs Advanced](#base-vs-advanced)

## <br> General Functions

#### initialize_shared_memory()

- Function to initialize shared memory and semaphore

#### generate_random_path(int \*path, int size)

- Initializes an array representing a path with consecutive city numbers and then shuffles the path using the Fisher-Yates algorithm to create a random ordering of cities. The resulting path is used as a starting point for the algorithm.

#### calculate_distance(int \*path)

- Calculates the total distance of a given path in the traveling salesman problem. It iterates through each city in the path, adding the distance from one city to the next. Finally, it adds the distance from the last city back to the starting city to complete the loop. The calculated total distance is then returned.

#### exchange_mutation(int \*path)

- Implements exchange mutation, by randomly selecting two positions in the path and swaping the cities at those positions. This mutation helps introduce diversity in the population of solutions during the optimization process.

#### get_elapsed_time()

- Calculates and returns the elapsed time in milliseconds since the program started. It uses the gettimeofday function to obtain the current time and then calculates the time difference in microseconds. Finally, it converts the elapsed time to milliseconds before returning the result.

#### run_algorithm(int process_id, int num_processes, int max_time)

- Represents the main logic of the algorithm for solving the traveling salesman problem. It initializes a random solution, applies mutation, evaluates the mutated solution, and updates the current solution and shared memory if improvements are found. The loop continues until the specified maximum time is reached.

#### main(int argc, char \*argv[])

- Initializes the program, reads the distance matrix from a file, initializes shared memory and a semaphore, forks processes to run the genetic algorithm, waits for processes to finish, prints the best solution found, and performs cleanup by freeing memory and removing shared memory.

## <br>Base Version

#### update_shared_memory(Solution \*solution)

- Updates the shared memory with a new best solution. It uses a semaphore to control access to the shared memory, ensuring that only one process can update it at a time. The function compares the distance of the new solution with the distance stored in shared_memory. If the new solution has a shorter distance or if shared_memory has not been initialized (distance is 0), it updates shared_memory with the new solution and accumulates the total iterations performed.

## <br>Advanced Version

#### update_shared_memory(Solution \*solution)

- Responsible for updating the shared memory with a new solution if the provided solution has a smaller distance or more iterations than the solution currently in shared memory. It uses a semaphore for synchronization and sends a signal (SIGUSR1) to the parent process to notify it about the update. The synchronized_this_iteration flag is set to indicate that synchronization occurred during this iteration.

#### synchronize_processes()

- Responsible for synchronizing child processes by sending the SIGUSR1 signal to each of them. The usleep function introduces a small delay to allow time for the signals to be processed by the child processes.

#### handle_update_signal(int signo)

- This function is a signal handler for the SIGUSR1 signal. It sets the update_signal flag when the signal is received and then checks if the synchronized_this_iteration flag is set. If set, it calls the synchronize_processes function to synchronize child processes and resets the flag afterward.

## <br> Base vs Advanced

##### Signal Handling:

The base version does not use signals for synchronization between processes. Instead, it relies on semaphores.<br>
The advanced version uses signals (SIGUSR1) to notify parent processes about updates in the shared memory. It also handles signals for synchronization.

##### Process Synchronization:

The base version relies on semaphores for process synchronization, specifically using sem_wait and sem_post.<br>
The advanced version uses both signals and semaphores for process synchronization. It uses signals to notify parent processes about updates and semaphores for other synchronization tasks.

##### Child Process Creation:

In the base version, child processes are created using the fork system call, and each process runs the genetic algorithm independently.<br>
In the advanced version, child processes are created similarly, but they are synchronized using signals, and each process runs a loop that includes synchronization steps.

##### Path Update:

In the base version, the update_shared_memory function is responsible for updating shared memory based on the current solution.<br>
In the advanced version, the update_shared_memory function is more complex and involves handling signals and synchronization.
