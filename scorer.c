#include <stdio.h>
#include <string.h> //strings
#include <stdlib.h>
#include <dirent.h> //working with directories
#include <sys/stat.h> //for stat
#include <unistd.h> //close & open
#include <fcntl.h> //files

typedef struct
{
    float longitude;
    float latitude;
}Coordonates;

typedef struct
{
    int report_id;
    char name[30];
    Coordonates gps_coordonates;
    char issue_category[20]; //ex. road, lighting, housing, flora, wildlife
    int severity_level; //1 = minor, 2 = moderate, 3 = critical
    time_t timestamp; //the time of the making of the report
    char description[256]; //short description about the issue
}Report;

typedef struct
{
    char name[30];
    int score;
}Inspector_score;

int argument_validation(char *district_id, char *file_path)
{
    struct stat st;
    sprintf(file_path, "%s/reports.dat", district_id);

    if(stat(district_id, &st) == -1)
    {
        printf("District folder not found!\n");
        return 0;
    }

    return 1;
}

void calculate_workload_score(char *district_id, char *file_path)
{
    struct stat st;

    int fd = open(file_path, O_RDONLY);

    if(fd == -1)
    {
        printf("Error opening the file!");
        return;
    }

    if(fstat(fd, &st) == -1)
    {
        printf("Error on fstat!\n");
        close(fd);
        return;
    }

    Report r;
    Inspector_score inspectors[st.st_size/sizeof(Report)]; //we assume that each report has a different inspector in order to make sure the vector is big enough
    int inspector_count = 0;

    while(read(fd, &r, sizeof(Report)))
    {
        int already_there = 0;

        for(int i = 0; i < inspector_count; i++)
        {
            if(strcmp(r.name, inspectors[i].name) == 0)
            {
                already_there = 1;
                break;
            }
        }

        if(already_there == 0)
        {
            strcpy(inspectors[inspector_count].name, r.name);
            inspectors[inspector_count].score = 0;
            inspector_count++;
        }

        for(int i = 0; i < inspector_count; i++)
        {
            if(strcmp(r.name, inspectors[i].name) == 0)
            {
                inspectors[i].score += r.severity_level;
                break;
            }
        }
    }

    close(fd);

    printf("Calculating the scores for district %s:\n", district_id);

    for(int i = 0; i < inspector_count; i++)
        printf("Inspector %s: %d.\n", inspectors[i].name, inspectors[i].score);
}

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        printf("Error at the number of arguments!\n");
        exit(-1);
    }

    char file_path[256];

    if(argument_validation(argv[1], file_path) == 0)
    {
        printf("The arguments are not correct!\n");
        exit(-2);
    }

    calculate_workload_score(argv[1], file_path);

    return 0;
}
