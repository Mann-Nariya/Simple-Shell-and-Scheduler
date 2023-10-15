#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <errno.h>


//all the commands get stored in this array
char input[100];
char *command[20];
int size_cmd = 0;
char *history[1000];
int proc_id[1000];
int hist = 0;
struct timeval start_time[1000];
struct timeval end_time[1000];
int num_commands = 0;
// max commands using pipe "|" can be 10
char *commands_[10];
volatile int timerexpiredflag=0;

int NCPU;
int TSLICE;
struct Queue priority_queue[5];
struct Queue ready_queue;
struct Queue running_queue;


// //declaring a typedef for the advanced implementation
// typedef struct {
//     pid_t pid_proc;
//     int priority;
// }queue_pair;

// //struct of priority queue which has a array of queue_pair instead of pids
// struct Priority_Queue{
//     int size;
//     int front;
//     int rear;
//     // int lock_status; // 1 means that the Priority_Queue is locked and 0 means that the process is not locked
//     queue_pair *pair_arr;
// };


// void create_priorityqueue(struct Priority_Queue *q, int size_a){
//     q->size = size_a;
//     q->front= q->rear = -1;
//     q->pair_arr = (pid_t *)malloc(size_a*sizeof(queue_pair));
// };

// void enqueue_priority_queue(struct Priority_Queue *q, pid_t pid_1, int priority1){

//     queue_pair temp_pair;

//     temp_pair.pid_proc = pid_1;
//     temp_pair.priority = priority1;

//     if((q->rear+1)%q->size==q->front){
//         printf("Error in adding to the Priority_Queue because queue is full");
//     }
//     else{
//         q->rear=(q->rear+1)%q->size;
//         q->pair_arr[q->rear]=temp_pair;
//     }

// }

// queue_pair dequeue_priority_queue(struct Priority_Queue *q){
    
//     queue_pair temp_result;

//     if(q->front==q->rear){
//         printf("Error in removing from Priority_Queue because queue is empty\n");
//     }
//     else{
//         q->front=(q->front+1)%q->size;

//         temp_result.pid_proc = q->pair_arr->pid_proc;
//         temp_result.priority = q->pair_arr->priority;
//         //pid_2 = q->pair_arr[q->front];
//     }
//     return temp_result;
// }

// int isEmpty_priorityqueue(struct Priority_Queue* q){
//     if(q->front== q->rear){
//         return 1;
//     }
//     return 0;
// }

// int isFull_priorityqueue(struct Priority_Queue* q){
//     if((q ->rear+1)%q->size == q->front){
//         return 1;
//     }
//     return 0;
// }


//definitions for the regular queue
struct Queue{
    int size;
    int front;
    int rear;
    int lock_status; // 1 means that the queue is locked and 0 means that the process is not locked
    pid_t *pid_arr;
};

void create_queue(struct Queue *q, int size_a){
    q->size = size_a;
    q->front= q->rear = -1;
    q->pid_arr = (pid_t *)malloc(size_a*sizeof(pid_t));
}

void enqueue(struct Queue *q, pid_t pid_1){

    if((q->rear+1)%q->size==q->front){
        printf("Error in adding to the queue because full");
    }
    else{
        q->rear=(q->rear+1)%q->size;
        q->pid_arr[q->rear]=pid_1;
    }

}

pid_t dequeue(struct Queue *q){
    pid_t pid_2=-1;

    if(q->front==q->rear){
        printf("Error in removing from queue because empty\n");
    }
    else{
        q->front=(q->front+1)%q->size;
        pid_2 = q->pid_arr[q->front];
    }
    return pid_2;
}

int isEmpty_queue(struct Queue* q){
    if(q->front== q->rear){
        return 1;
    }
    return 0;
}

int isFull_queue(struct Queue* q){
    if((q ->rear+1)%q->size == q->front){
        return 1;
    }
    return 0;
}



//this function can be used for implementing the history command as told in 'g'
void print_history_cmd(){
    printf("%5s  %60s\t%17s\t%17s\t%9s\n","PID","Command","Start_time(ms)","End_time(ms)","Exec_time(ms)");
    for(int i=0; i<hist;i++){
        printf("%5d. %60s\t%10ld:%6ld\t%10ld:%6ld\t%9ld\n", proc_id[i], history[i],start_time[i].tv_sec,start_time[i].tv_usec,end_time[i].tv_sec,end_time[i].tv_usec,(end_time[i].tv_sec - start_time[i].tv_sec) * 1000000 + (end_time[i].tv_usec - start_time[i].tv_usec));
    }
    return;
}

void print_history_only(){
    printf("%7s %40s\n","PID","Command");
    for(int i=0; i<hist;i++){
        printf("%7d %40s\n", proc_id[i], history[i]);
    }
    return;
}

void add_command_to_history(char * command)
{
    history[hist++] = strdup(command);
}


// Used for seperating commands and arguments
char** seperate_user_input(char* usr_input){
    // char* output[10];
    // int j=0;
    // for(int i=0;i<usr_input[i]!="\0";i++){
    //     char* temp_string="";
    //     if(usr_input[i]==" "){
    //         output[j]=temp_string;
    //         temp_string=""; 
    //         j++;
    //         i++;
    //     }
    // }
    // return output;

    //skipping the initial spaces of the usr input
    while(*usr_input == ' '){
        usr_input++;
    }

    char *sep;
    size_cmd = 0;
    sep = strtok(input," ");
    while (sep != NULL)
    {
        command[size_cmd] = sep;
        size_cmd++;
        sep = strtok(NULL," ");
    }
    command[size_cmd] = NULL;
    return command;
    
}


// function for running the process which don't require scheduling
int create_process_to_run(char** enter_the_ptr_to_Array){
    char **arr_of_args = enter_the_ptr_to_Array;
    pid_t processes_status_1=fork();

    if(processes_status_1<0){
        printf("Error in creating the child...");
        return 1;
    }
    else if (processes_status_1==0)
    {
        // printf("I'm the child process ID = %d",getpid());
        // if (strcmp(arr_of_args[0],"history") == 0){print_history_cmd();printf("\n");    }
        execvp(arr_of_args[0],arr_of_args);
        // printf("%d\n",ret);
        // printf("-1\n");
        // processes_status_1 = -1;
        perror("execvp");
        exit(EXIT_FAILURE);
        // printf("This is child process");
    }
    else{
        // printf("This is parent process");
        waitpid(processes_status_1,NULL,0);
    }
    
    return processes_status_1;
}

//function for running the process which need scheduling
int create_process_for_scheduling(char** enter_ptr_to_arr){
    char **arr_of_args1 = enter_ptr_to_arr;

    pid_t process_status_2 = fork();

    if(process_status_2 <0){
        printf("Error in creating the child process...");
        return 1;
    }

    else if (process_status_2==0){
        //suspending the execution of the child process when it starts execution until the scheduler sends the sigcont signal
        int result_of_kill_here=kill(getpid(),SIGSTOP);
        
        //this code will not be reached until  a sigcont command is not sent to this process.
        if(result_of_kill_here == 0){
            //need to add the commands that will execute the binary that has been uploaded

            char* args_to_run = {arr_of_args1[1],NULL};
            execv(args_to_run[0],args_to_run);
            perror("execv");
            exit(EXIT_FAILURE);
        }
        //case when the sigstop failed.
        else{
            perror("SIGSTOP in process for scheduler");
            //need to add something else?
        }
    }
    else{
        pid_t pid_to_enqueue= getpid();

        //enqueuing the process id to the ready queue
        if(arr_of_args1[2] != NULL){
            int priority= arr_of_args1[2];
            enqueue(&priority_queue[priority],pid_to_enqueue);
        }
        else{
            int priority=1;
            enqueue(&priority_queue[priority],pid_to_enqueue); 
        }
        //enqueue(*ready_queue, pid_to_enqueue);


        //pause the child process here or in the child process itself
        //printf("It is the parent process");
    }

    return process_status_2;
}


//this command accepts the user input of commands and the arguments and returns the execution status of the commands it gets executed
int launch(char* entered_text){
    int process_status;
    char** array_of_args = seperate_user_input(entered_text);

    //checking if the command submitted is submit command or not
    //printf("%s\n", array_of_args[0]);

    if(strcmp(array_of_args[0],"submit")==0){
        process_status = create_process_for_scheduling(array_of_args);
    }
    else{
        process_status = create_process_to_run(array_of_args);
    }

    //to print the history when the history command is entered
    // if(strcmp(entered_text,"history")==0){
    //     print_history_cmd();
    //     return 1;
    // }
    return process_status;

}


//This function is used to check the status of the function and print the status of the execution of the command inserted.
void process_status_fn(int status){
    if(status==1){
        printf("Successful execution of process");
    }
    else{
        printf("Process execution failed");
    }
}


// setting up the signal handler
void timer_signal_handler(int signo){
    timerexpiredflag=1;
}


//priority scheduling implementation
int Process_control_block(){
    int no_proc=0;
    // for(int i=0; i<NCPU;i++){
    //             if(isEmpty_queue(ready_queue)==0){
    //                 process_arr[i]=dequeue(ready_queue);
    //                 no_proc++;
    //             }
    //         }

    for(int j=4; j>=1 ; j--){
        if(no_proc == NCPU){
            break;
        }
        if(isEmpty_queue(&priority_queue[j])==1){
            continue;
        }
        else{
            while(isEmpty_queue(&priority_queue[j])==0 || no_proc != NCPU){
                pid_t temp_pid;
                temp_pid= dequeue(&priority_queue[j]);
                enqueue(&ready_queue,temp_pid);
                no_proc++;
            }
        }
    }
    return no_proc;
}

void scheduler(){

    //continual execution of the scheduler with the while loop
    while(true){
        if(isEmpty_queue(&ready_queue)== 1){
            continue;
        }
        else{
            //int arr_len=NCPU;
            //pid_t process_arr[NCPU]={-1};
            int no_proc=Process_control_block();

            //setting up out signal handler for the SIGALRM signal
            signal(SIGALRM,timer_signal_handler);

            int start_signal_sent=0; //0 means not sent and 1 means sent
            int iterator = running_queue.front;
            int max_size_running = running_queue.size;
            //setting the alarm for the execution of the while loop for only the tslice
            alarm(TSLICE);
            

            //continues execution till timer is not expired
            while(!timer_signal_handler){
                if(start_signal_sent==0){
                    
                    //using the for loop for sending the SIGCONT signal to all the processes that have been dequeued
                    while(iterator != running_queue.rear){
                        kill(running_queue.pid_arr[iterator],SIGCONT);
                        iterator = (iterator+1)%max_size_running;
                    }
                    //for ensuring that the sigcont signal is send only once
                    start_signal_sent=1;
                }
                else{
                    continue;
                }
            }
            start_signal_sent=0;

            //needs to get work done here

            int iterator1= running_queue.front;
            //timer expired the program exits from the loop now sending the signal to the process to pause execution using SIGSTOP
            while(iterator1 != running_queue.rear){
                pid_t temp_store = dequeue(&running_queue);

                int kill_result=kill(temp_store,SIGSTOP);
                if(kill_result == 0){
                    //normal functioning add code here

                    //enqueue the process to the ready queue
                    enqueue(&ready_queue,process_arr[i]);
                    //printf("the process is not terminated and it exists");
                }
                else if(kill_result == ESRCH){
                    //process_arr[i]=-1;
                    //not enqueueing back to the qeueue because it is termainated
                }
                else{
                    //error in killing the process in which case print the error
                    perror("Killing in scheduler");
                }

                iterator1 = (iterator1+1)%max_size_running;
            }
        }
    }
}

//int dummy_main(int argc, char* argv[]);


//defining the static variables



//program execution starts here
int main(int argc, char* argv[]){

     //checking if the number of cli arguments are correct
     if(argc != 3){
        printf("Incorrect number of cli arguments, ending program execution with error code 1");
        return 1;
    }

    //using atoi function to convert arguments from strings to integers
    NCPU= atoi(argv[1]);
    TSLICE = atoi(argv[2]);

    //making the ready queue that will be used in the scheduler for scheduling processes
    //global struct Queue *ready_queue and running queue;
    create_queue(&ready_queue, 100);
    create_queue(&running_queue,50);

    //indivisially creating the priority queue for the priority queue
    create_queue(&priority_queue[1],100);
    create_queue(&priority_queue[2],100);
    create_queue(&priority_queue[3],100);
    create_queue(&priority_queue[4],100);

    
    //flag to check if the scheduler is launched or not
    int scheduler_launched_flag=0;
    do{

        //displaying the command prompt of out choice as asked in part 'b'
        if(scheduler_launched_flag==0){
            //initiating the scheduler
            scheduler();
            scheduler_launched_flag=1;
        }
        memset(input,0,sizeof(input));
        printf("SimpleShell@Grp-52: > ");
        fgets(input,100,stdin);

        //skipping the execution if the space entered is blank
        if(strlen(input) == 0){
            continue;
        }

        if ((strlen(input) > 0) && (input[strlen (input) - 1] == '\n'))
        {
        	input[strlen (input) - 1] = '\0';
        }
        // printf("input was = %s\n",input);
        // history[hist] = input;
        // hist++;
        add_command_to_history(input);
        // printf(" ");
        // printf("%s", input);
        if (strcmp(input,"exit") == 0) {print_history_cmd();break;}
        if (strcmp(input,"history") == 0){print_history_only();continue;}

        // note the start time
        gettimeofday(&start_time[hist-1],NULL);
        int process_status = launch(input);
        // note the end time
        gettimeofday(&end_time[hist-1],NULL);
        // printf("%d\n",process_status);
        proc_id[hist-1] = process_status;
        // if (process_status == -1){continue;}
        // process_status_fn(process_status);

        // i++;
    }while(1);


    return 0;
}