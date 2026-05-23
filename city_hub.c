#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
    while(1)
    {
        printf(">hub ");

        char buffer[256];
        char district_id[256][256];
        int district_count = 0;
        int command = 0;

        fgets(buffer, sizeof(buffer), stdin); //we read the input from the standard input (keyboard)

        buffer[strcspn(buffer, "\n")] = 0; //fgets also reads \n so we need to eliminate it

        char *p = strtok(buffer, " ");

        if (p == NULL) 
        {
            continue; 
        }

        if(strcmp(p, "start_monitor") == 0)
        {
            pid_t pid_hub_mon = fork();

            if(pid_hub_mon < 0)
            {
                printf("Error on fork!\n");
                exit(-2);
            }
            else if(pid_hub_mon == 0) //hub_mon
            {
                //child
                int fd[2];
                pipe(fd); //fd[0] read, fd[1] write

                pid_t pid_hub_mon_child = fork();

                if(pid_hub_mon_child < 0)
                {
                    printf("Error on fork!\n");
                    exit(-2);
                }
                else if(pid_hub_mon_child == 0)
                {
                    close(fd[0]); //we just need to write, so we close the reading end of the pipe

                    dup2(fd[1], 1); //we redirect the screen to the writing pipe fd[1]

                    close(fd[1]); //stdout points to the pipe so we don't need fd[1] anymore

                    execlp("./monitor_reports", "./monitor_reports", NULL); //anything that monitor_reports outputs 

                    //in case execlp fails
                    perror("Error on execlp monitor_reports");
                    exit(-1);
                }
                else if(pid_hub_mon_child > 0)
                {
                    //parent
                    close(fd[1]); //we need to just read, so we close the writing end of the pipe

                    char pipe_buffer[256];
                    int bytes_read;

                    while((bytes_read = read(fd[0], pipe_buffer, sizeof(pipe_buffer) - 1)) > 0) 
                    {
                        pipe_buffer[bytes_read] = '\0'; //putting the terminator in the string
                        printf("[HUB_MON] has captured: %s", pipe_buffer);
                    }

                    printf("[HUB_MON]: The monitor has stopped.\n"); //aici apare dubios daca se termina ca mai e alt program
                    
                    //so that after the monitor stops, we still get the >hub so prompt the user to input something
                    printf(">hub "); 
                    fflush(stdout);

                    close(fd[0]);

                    exit(0); //hub_mon is done
                }
            }
            else if(pid_hub_mon > 0)
            {
                //parent
                continue;
            }
        }
        else if(strcmp(p, "calculate_scores") == 0)
        {
            while(p)
            {
                if(command == 0) //to avoid putting the command in the district list
                    command = 1;
                else
                {
                    strcpy(district_id[district_count], p);
                    district_count++;
                }

                p = strtok(NULL, " ");
            }

            for(int i = 0; i < district_count; i++)
            {
                int fd[2];
                pipe(fd); //fd[0] read, fd[1] write

                pid_t pid_hub_mon = fork();

                if(pid_hub_mon < 0)
                {
                    printf("Error on fork!\n");
                    exit(-1);
                }
                else if(pid_hub_mon == 0)
                {
                    close(fd[0]); //we just need to write, so we close the reading end of the pipe

                    dup2(fd[1], 1); //we redirect the screen to the writing pipe fd[1]

                    close(fd[1]); //stdout points to the pipe so we don't need fd[1] anymore

                    execlp("./scorer", "./scorer", district_id[i], NULL); //anything that monitor_reports outputs 

                    //in case execlp fails
                    perror("Error on execlp scorer!");
                    exit(-1);
                }
                else if(pid_hub_mon > 0)
                {
                    //parent
                    close(fd[1]); //we need to just read, so we close the writing end of the pipe

                    char pipe_buffer[256];
                    int bytes_read;

                    while((bytes_read = read(fd[0], pipe_buffer, sizeof(pipe_buffer) - 1)) > 0) 
                    {
                        pipe_buffer[bytes_read] = '\0'; //putting the terminator in the string
                        printf("[HUB_MON]: %s", pipe_buffer);
                    }

                    close(fd[0]);

                    int status;
                    waitpid(pid_hub_mon, &status, 0);
                }
            }

            if(district_count == 0)
                printf("No district provided!\n");
        }
        else if(strcmp(p, "stop_monitor") == 0)
        {
            // Cautam fisierul ca sa aflam PID-ul
            int fm = open(".monitor_pid", O_RDONLY);
            if(fm != -1) 
            {
                char buff[32]; 
                memset(buff, 0, sizeof(buff));
                
                if(read(fm, buff, sizeof(buff) - 1) > 0) 
                {
                    pid_t m_pid = atoi(buff);
                    // Trimitem semnalul de oprire (Ctrl+C fals) direct catre nepot!
                    kill(m_pid, SIGINT); 
                    printf("Stop signal sent to monitor.\n");
                }
                close(fm);
            } 
            else 
            {
                printf("No monitor is currently running.\n");
            }

            continue;
        }
        else if(strcmp(p, "stop_program") == 0)
        {
            printf("Shutting down city_hub...\n");
            
            // Verificam daca am uitat un monitor pornit si il oprim
            int fm = open(".monitor_pid", O_RDONLY);

            if(fm != -1) 
            {
                char buff[32]; 
                memset(buff, 0, sizeof(buff));

                if(read(fm, buff, sizeof(buff) - 1) > 0) 
                    kill(atoi(buff), SIGINT); 
                
                close(fm);
            }
        
            exit(0);
        }
        else
        {
            printf("Wrong command!\n");
            continue;
        }
    }

    return 0;
}
