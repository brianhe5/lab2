#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  //each process has forward and backward nodes
  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */

  u32 temp_burst_time;
  bool started;
  /* End of "Additional fields here" */
};
//struct acting as our head of linked list
  //process_list: head struct
  //process: node type of DLL
TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    //if next is not digit, return current digit
    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }


    //if digit
    else
    {

      // if digit and parsing not started, current = current digit value
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }

      // if digit and parsing started, add next to current value
      // 6 4 --> current = 6 with next = 4 --> current  = 64
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    //move to next character in the input data
    ++(*data);
  }

  //exit call
  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

//similar to first next_int
u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}
/*
the init_processes function reads process information from a file into a 
dynamically allocated array of struct process based on the provided file path. It uses memory 
mapping for efficient file reading and dynamically allocates memory for the process data. The 
next_int function is crucial for parsing integers from the mapped data.
*/
void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);

  //error if cant open
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  //prints error if retrieved info about opened file size fails
  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  //calculates the size of file
  u32 size = st.st_size;

  //attemps to read file into memory for reading
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  //error if mmap fails
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  //sets pointers to end of mapped region
  const char *data_end = data_start + size;

  //maps data to start of mapped region
  const char *data = data_start;

  //uses next_int to read in first integer (# of processes) as *process_size
  //data pointers then points to next pos after reading integer
  *process_size = next_int(&data, data_end);

  //allocates memory for an array of struct process
  *process_data = calloc(sizeof(struct process), *process_size);

  //error if allocation fails
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  //loop initializes each element of struct array by using next_int
  //reads in process ID, arrival time, and burst time

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
    //temp burst time
    (*process_data)[i].temp_burst_time = (*process_data)[i].burst_time;
    //determines start time for response time
    (*process_data)[i].started = false;
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{

  //error if # of cmd line arguments != 3
  if (argc != 3)
  {
    return EINVAL;
  }

  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  //#structure tthat connects elements in the queue
  //TAILQ_ENTRY(process) pointers;
  //#process list is structure to be defined and process is elements to be linked
  //TAILQ_HEAD(process_list, process);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;
  
  if (quantum_length == 0)
  {
    //???
    int err = errno;
    perror("invalid quantum_length");
    exit(err);
  }
  if (size == 0)
  {
    int err = errno;
    perror("invalid number of processes");
    exit(err);
  }
  u32 total_time = 0;
  u32 current_quantum_length_time = quantum_length;
  u32 finish_counter = 0;
  bool finished = false;

  while (!finished) {


    //inserts processes into queue
    for (u32 i = 0; i < size; ++i) {
      if (data[i].arrival_time == total_time)
      {
        struct process* newProcess = (struct process*)malloc(sizeof(struct process));
        *newProcess = data[i];
        TAILQ_INSERT_TAIL(&list, newProcess, pointers);
      }
    }


    // printf("=======================\n");
    // printf("head pointer ID: %u\n", TAILQ_FIRST(&list)->pid);
    // printf("remaining time: %u\n", TAILQ_FIRST(&list)-> temp_burst_time);


    //processes expired process and resets current_quantum_length_time to quantum length
    if (!TAILQ_EMPTY(&list))
    {
      if (TAILQ_FIRST(&list)->temp_burst_time == 0)
      {
        struct process *currentProcess = TAILQ_FIRST(&list);
        TAILQ_REMOVE(&list, currentProcess, pointers);
        free(currentProcess);
        finish_counter++;
        current_quantum_length_time = quantum_length;
      }
    }


    //processes expired quantum length and puts head at tail
    if (current_quantum_length_time == 0)
    {
      current_quantum_length_time = quantum_length;
      if (!TAILQ_EMPTY(&list))
      {
        struct process *head = TAILQ_FIRST(&list);
        TAILQ_REMOVE(&list, head, pointers);
        TAILQ_INSERT_TAIL(&list, head, pointers);
      }
    }


    //processes head_burst
    if ((!TAILQ_EMPTY(&list)))
    {
      struct process *currentProcess = TAILQ_FIRST(&list);
      if (!(currentProcess->started))
      {
        //calculates total response time
        total_response_time += (total_time - currentProcess->arrival_time);
        currentProcess->started = true;
      }
      currentProcess->temp_burst_time -= 1;
    }


    //adds up waiting time for every process in queue, but not active
    if ((!TAILQ_EMPTY(&list)))
    {
      struct process* currentProcess;
      TAILQ_FOREACH(currentProcess, &list, pointers) {
        if (currentProcess != TAILQ_FIRST(&list))
        {
          total_waiting_time += 1;
        }
      }
    }
    

    //after processing, increment to time after processing
    total_time += 1;
    current_quantum_length_time -= 1;

    // printf("=======================\n");
    // printf("head pointer ID: %u\n", TAILQ_FIRST(&list)->pid);
    // printf("remaining time: %u\n", TAILQ_FIRST(&list)-> temp_burst_time);
    // printf("Waiting Time Accumulated: %u\n", total_waiting_time);
    // printf("Seconds Completed: %u\n", total_time);

    //determines if finished
    
    if (finish_counter == size)
    {
      // printf("Stops Running: no more processes\n\n");
      finished = true;
    }
    // u32 counter = 0;
  //   for (u32 i = 0; i < size; ++i) {
  //     printf("hello hi\n");
  //     if (data[i].temp_burst_time == 0)
  //     {
  //       printf("hello \n");
  //       counter++;
  //     }
  //   }
  //   printf("finish counter value: %u\n", counter);
  //   printf("size value: %u\n", size);

  //   if (counter == size)
  //   {
  //     printf("mhelo11 \n");

  //     finished = true;
  //   }
  }

  
  //total waiting time is time not being contacted
  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);

  //response time is time from first arrival to first contact?
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
