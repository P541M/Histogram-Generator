#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> //WAIT WAIT WAIT
#include <ctype.h> //islower, isalpha stuff

//Constant for alphabet count
#define ALPHABET_SIZE 26

//for pipe descriptors
int **pipeFd;
//Array of all the PIDs
pid_t *childPids;
//stores the number of files to process (used for child creation)
int numFiles;

void sigchld_handler(int sig){
    //Stores the status of the child
    int termStatus;
    //stores the PID of the child
    pid_t pid;
    
    //reaps all terminated kids, no blocking
    while((pid = waitpid(-1, &termStatus, WNOHANG)) > 0){
        printf("SIGCHLD caught from PID %d\n", pid);
        
        //For loop to iterate through all kids
        for(int i = 0; i < numFiles; ++i){
            //Checks if they are equal (child PID and return waitpid)
            if(childPids[i] == pid){
                //normal exit
                if(WIFEXITED(termStatus)){
                    printf("Child %d exited with status %d\n", pid, WEXITSTATUS(termStatus));
                }
                //signal exit 
                else if(WIFSIGNALED(termStatus)){
                    printf("Child %d killed by signal %d\n", pid, WTERMSIG(termStatus));
                }

                //Checks for proper run
                if(WIFEXITED(termStatus) && WEXITSTATUS(termStatus) == 0){
                    //Holds total count for each letter
                    int counts[ALPHABET_SIZE] = {0};
                    
                    //will read hist data
                    if(read(pipeFd[i][0], counts, sizeof(counts)) > 0){
                        //filename name const
                        char filename[256];
                        //actual filename creation with PID
                        snprintf(filename, sizeof(filename), "file%d.hist", pid);
                        
                        //opens the file for writing (creation of hist)
                        FILE* fp = fopen(filename, "w");
                        if (fp){
                            //alphabet count
                            for(int j = 0; j < ALPHABET_SIZE; ++j){
                                fprintf(fp, "%c %d\n", 'a' + j, counts[j]);
                            }
                            //gates are closed
                            fclose(fp);
                        }
                    }
                }
                //Closes the read
                close(pipeFd[i][0]); // Always close the read end of the pipe
                break;
            }
        }
    }
}

void compute_histogram(const char* filename, int pipe_write_end, int child_index){
    //Opens the file for readingg
    FILE *fp = fopen(filename, "r");
    //Calculates sleep time
    int sleepTime = 10 + 3 * child_index;

    //Checks if the open failed
    if(fp == NULL){
        perror("fopen failed");
        exit(1);
    }

    //Array for storing the letter count
    int counts[ALPHABET_SIZE] = {0};
    //Stores each character while being read from file
    int character;
    
    //READING THE FILEEEEEE WOOOOO
    while((character = fgetc(fp)) != EOF){
        //Checks if it is a letter and will increment accordingly
        if(isalpha(character)){
            //Sets it to lowercase (Accounts for A/a)
            counts[tolower(character) - 'a']++;
        }
    }
    fclose(fp);

    //sends hist to parent
    if(write(pipe_write_end, counts, sizeof(counts)) == -1){
        perror("write failed");
        exit(1);
    }

    //sleepy time
    sleep(sleepTime);
    //close write
    close(pipe_write_end);
    exit(0);
}

//Helper cleanup function to free memory
void clean_up(){
    //for loop to clean each child/file process
    for(int i = 0; i < numFiles; ++i){
        if(pipeFd[i] != NULL){
            //Close read
            close(pipeFd[i][0]);
            //Close write
            close(pipeFd[i][1]);
            //FREEEEEEEEEEEEEE
            free(pipeFd[i]);
        }
    }
    //Frees the main array holding the pipe pointers
    if(pipeFd != NULL){
        free(pipeFd);
    }
    //Frees the memory for storing the PIDs
    if(childPids != NULL){
        free(childPids);
    }
}

int main(int argc, char* argv[]){
    //If statement to check if file was given after ./A1
    if(argc < 2){
        fprintf(stderr, "No input file(s)\n");
        return 1;
    }

    //Subtract 1 from the total params provided to file total amount of files
    numFiles = argc - 1;
    
    //Malloc for pipe descriptors
    pipeFd = malloc(numFiles * sizeof(int*));
    //Malloc for child PIDs
    childPids = malloc(numFiles * sizeof(pid_t));
    
    //Malloc for each file
    for(int i = 0; i < numFiles; i++){
        //Malloc for each pipe
        pipeFd[i] = malloc(2 * sizeof(int));
        //If statement to check if the pipe creation worked
        if (pipe(pipeFd[i]) < 0) {
            perror("pipe failed");
            clean_up();
            exit(1);
        }
    }

    //struct for signal handling
    struct sigaction sa;
    //Sets it to 0
    memset(&sa, 0, sizeof(sa));
    //sets the handler function
    sa.sa_handler = sigchld_handler;
    //signal mask, for blocking
    sigemptyset(&sa.sa_mask);
    //will restart if interrupted
    sa.sa_flags = SA_RESTART;

    //Applies the settings, if not will shoot error
    if(sigaction(SIGCHLD, &sa, NULL) != 0){
        perror("sigaction failed");
        clean_up();
        exit(1);
    }

    //For loop to create a child process for each file
    for(int i = 1; i < argc; ++i){
        //Creates new process
        pid_t pid = fork();
        //If statement to check if it failed
        if (pid < 0){
            perror("fork failed");
            //Will cleanup before leaving (NO LEAKS HEREEE)
            clean_up();
            exit(1);
        }
        //Child
        else if(pid == 0){
            //Closes read
            close(pipeFd[i - 1][0]);
            //Calls the histogram function to create the hist
            compute_histogram(argv[i], pipeFd[i - 1][1], i - 1);
        }
        //Parent
        else{
            //Closes write
            close(pipeFd[i - 1][1]);
            //Stores the child PID
            childPids[i - 1] = pid; 
        }
    }

    int leftChildren = numFiles;
    //waits for all kids to terminate
    while(leftChildren > 0){
        //wait for signals to show a child terminated
        pause();
        //decrement remaining children count
        leftChildren--;
    }

    //NO MEMORY LEAKS HEREEEEEE
    clean_up();
    //JE SUIS FINI
    return 0;
}
