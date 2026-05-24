#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

volatile sig_atomic_t keep_running = 1;

//handler function so that the program knows what to do based on specific signals
void handler(int sig)
{
    if(sig == SIGINT) //we finish the program
        keep_running = 0;

    if(sig == SIGUSR1) //the signal for a new report that has been added
    {
        const char message[] = "\n[Monitor] A new report has been added to the system!\n";
        write(1, message, sizeof(message) - 1); //async signal safe
    }
}

int main(int argc, char **argv)
{
    //we need to check if there isn't another monitor running
    int old_pid = -1;
    int fd_check = open(".monitor_pid", O_RDONLY);

    if(fd_check != -1) 
    {
        char buff[32];
        memset(buff, 0, sizeof(buff));
        
        //we try to read the old pid
        if(read(fd_check, buff, sizeof(buff) - 1) > 0) 
        {
            old_pid = atoi(buff);
        }

        close(fd_check);

        if (old_pid > 0 && kill(old_pid, 0) == 0) //if the pid is valid and the monitor is still active, we need to notify the user and not let another one start
        {
            printf("ERROR: Another monitor is already open and running with the pid: %d!\n", old_pid);
            exit(1); 
        }
    }

    pid_t pid = getpid();

    struct stat st;

    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644); //O_TRUNC for overwriting

    if(fd == -1)
    {
        printf("Error creating the monitoring file!\n");
        exit(-1);
    }

    if(fstat(fd, &st) == -1)
    {
        printf("Error on fstat!\n");
        close(fd);
        exit(-2);
    }

    char pid_str[20];
    int len = sprintf(pid_str, "%d\n", pid); 
    
    if(write(fd, pid_str, len) != len)
    {
        printf("Error writing in the monitoring file!\n");
        exit(-3);
    }      

    close(fd);

    //Ctrl+C to close the monitor 
    struct sigaction sa_int;
    sa_int.sa_handler = handler; //sets the sa_handler field to point to the custom function named handler 
    sigemptyset(&sa_int.sa_mask); //initializes the signal mask inside the structure to be completely empty, so it doesn't block any additional signals
    sa_int.sa_flags = 0; //sets the operational flags to 0, opting fot the default POSIX signal handling, no special modifications             
    sigaction(SIGINT, &sa_int, NULL); //applies the configuartion to the SIGINT signal
    //sa_int is the address of the structure containing the new configuration rules
    //NULL means it doesn't save old behaviour

    struct sigaction sa_usr1;
    sa_usr1.sa_handler = handler; 
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    printf("Monitor running with PID: %d. Waiting for signals...\n", pid);
    fflush(stdout); //when stopping the monitor in city hub, the buffer keeps the text in there until it gets full so it also shows Monitor running with pid ... and this gets rid of it

    while(keep_running) //we keep the monitor open until SIGINT
        pause();

    printf("Monitoring stopped.\n");

    if(unlink(".monitor_pid") == -1)
    {
        printf("Error on deleting .monitor_pid!\n");
        exit(-4);
    }

    return 0;
}
