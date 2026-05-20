#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    /*while(1)
    {
        printf(">hub");
    }
        */

    char buffer[256];
    char district_id[256][256];
    int district_count = 0;
    int command = 0;

    gets(buffer);

    char *p = strtok(buffer, " ");

    if(strcmp(p, "start_monitor") == 0)
    {
        pid_t pid = fork();

        if(pid < 0)
        {
            printf("Error on fork!\n");
            return;
        }
        else if(pid == 0)
        {
            //child
        }
        else if(pid > 0)
        {
            //parent
        }
    }
    if(strcmp(p, "calculate_scores") == 0)
    {
        while(p)
        {
            if(command == 0) //to avoid putting the command in the district list
            {
                command = 1;
            }
            else
            {
                strcpy(district_id[district_count], p);
                district_count++;
            }

            p = strtok(NULL, " ");
        }
    }
    else
    {
        printf("Wrong command!\n");
        exit(-1);
    }

    return 0;
}
