#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h> //lucru cu directoare
#include <time.h> //timestamp
#include <sys/stat.h> //pt stat
#include <unistd.h> //close si open
#include <fcntl.h>

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
    char issue_category[20];
    int severity_level; //1 = minor, 2 = moderate, 3 = critical
    time_t timestamp;
    char description[256];
}Report;

int argument_validation(char **string, char *user_flag, char *role_flag, char *command_flag) //validarea argumentelor in linia de comanda ca structura
{
    //tre sa fie si asta cu city-manager sau e doar numele prog?
    if(strcmp(string[1], "city_manager") != 0)
        return 0;

    char role_title[100], role[100];
    strcpy(role_title, string[2]);
    strcpy(role, string[3]);

    char user_title[100], user[100];
    strcpy(user_title, string[4]);
    strcpy(user, string[5]);

    char command[100];
    strcpy(command, string[6]);

    if(strcmp(role_title, "--role") != 0)
        return 0;

    if(strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0)
        return 0;

    if(strstr(user_title, "--user") == NULL) 
        return 0;

    if(strstr(command, "--add") == NULL && strstr(command, "--list") == NULL && strstr(command, "--view") == NULL && strstr(command, "--remove_report") == NULL && strstr(command, "--update_threshold") == NULL && strstr(command, "--filter") == NULL) 
        return 0;

    strcpy(user_flag, user);
    strcpy(role_flag, role);
    strcpy(command_flag, command);

    return 1;
}

int generate_id(int fd)
{
    struct stat st;
    
    if (fstat(fd, &st) == -1) 
    {
        perror("Error on fstat");
        return 0;
    }

    int nr_rapoarte = st.st_size / sizeof(Report);

    return nr_rapoarte + 1;
}

int is_manager(char *role)
{
    if(strcmp(role, "manager") == 0)
        return 1;

    return 0;
}

int is_inspector(char *role)
{
    if(strcmp(role, "inspector") == 0)
        return 1;

    return 0;
}

void write_in_log(char *log_path, char *user, char *role, char *command, time_t timestamp)
{
    if(is_manager(role) == 0)
    {
        perror("Only the manager has the permission to write in the log!");
        return;
    }

    int log_fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if(log_fd != -1)
    {
        char log_buffer[512];
        int len = sprintf(log_buffer, "%s %s %s %s\n", ctime(&timestamp), user, role, command); //AICI trb alta comanda in loc de ctime ca pune si \n

        if(write(log_fd, log_buffer, len) == -1)
            perror("Error when writing in log file!");

        close(log_fd);
    }
}

void add(char *district_id, char *user, char *role)
{
    struct stat st;
    char file_path[256];
    char log_path[256];
    char severity_log_path[256];

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id);
    sprintf(severity_log_path, "%s/district.cfg", district_id);

    if(stat(district_id, &st) == 0) 
    {
        if(S_ISDIR(st.st_mode))
            printf("District folder found\n");
    }
    else
        mkdir(district_id, 0750);

    int fd = open(file_path, O_APPEND | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH); //0664
    //chmod(file_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH); //0664

    if(fd == -1)
    {
        perror("Error opening the report file!");
        return;
    }

    int fs = open(severity_log_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);

    if(fs == -1)
    {
        perror("Error opening the district severity level file!");
        return;
    }

    close(fs);

    Report report;
    memset(&report, 0, sizeof(Report));

    printf("X:");
    scanf("%f", &report.gps_coordonates.longitude);

    printf("Y:");
    scanf("%f", &report.gps_coordonates.latitude);

    printf("Category:");
    scanf("%s", report.issue_category);

    printf("Severity level(1|2|3):"); //eventuala verificare daca e 1 2 sau 3
    scanf("%d", &report.severity_level);

    printf("Description:");
    getchar(); 
    scanf("%[^\n]", report.description); //citire pana la enter

    strcpy(report.name, user);

    report.timestamp = time(NULL);

    report.report_id = generate_id(fd);

    if (write(fd, &report, sizeof(Report)) == -1) 
        perror("Error when writing in report file!");

    close(fd);

    write_in_log(log_path, user, role, "add", time(NULL));
}

void list_report_structure(Report r)
{
    printf("Report ID:%d\n", r.report_id);
    printf("Inspector name:%s\n", r.name);
    printf("X:%f\n", r.gps_coordonates.longitude);
    printf("Y:%f\n", r.gps_coordonates.latitude);
    printf("Category:%s\n", r.issue_category);
    printf("Severity level:%d\n", r.severity_level);
    printf("Timestamp: %s", ctime(&r.timestamp));
    printf("Description:%s\n", r.description);
}

void view(char *district_id, char *report_id_char, char *user, char *role)
{
    int report_id = atoi(report_id_char);
    int count = 1;

    struct stat st;
    char file_path[256];
    char log_path[256];

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id);

    if(stat(district_id, &st) == -1) 
    {
        printf("District folder not found\n");
        return;
    }

    int fd = open(file_path, O_RDONLY); 

    if(fd == -1)
    {
        perror("Error opening the file!");
        return;
    }

    write_in_log(log_path, user, role, "view", time(NULL));

    Report r;

    while(read(fd, &r, sizeof(Report)))
    {
        if(count == report_id)
        {
            list_report_structure(r);
            return;
        }

        count++;
    }
    
    close(fd);
}

void print_permission(mode_t mode)
{
    //user
    if(mode & S_IRUSR)
        printf("r");
    else
        printf("-");

    if(mode & S_IWUSR)
        printf("w");
    else
        printf("-");

    if(mode & S_IXUSR)
        printf("x");
    else
        printf("-");

    //group
    if(mode & S_IRGRP)
        printf("r");
    else
        printf("-");

    if(mode & S_IWGRP)
        printf("w");
    else
        printf("-");

    if(mode & S_IXGRP)
        printf("x");
    else
        printf("-");

    //other
    if(mode & S_IROTH)
        printf("r");
    else
        printf("-");

    if(mode & S_IWOTH)
        printf("w");
    else
        printf("-");

    if(mode & S_IXOTH)
        printf("x");
    else
        printf("-");

    printf("\n");
}

void list(char district_id[30], char *user, char *role) //trebuie verificat daca exista districtul inainte sau nu?
{
    struct stat st_dir, st_file;
    char file_path[256];
    char log_path[256];

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id);

    if(stat(district_id, &st_dir) == -1) 
    {
        printf("District folder not found!\n");
        return;
    }

    if(stat(file_path, &st_file) == -1) 
    {
        printf("Reports file not found!\n");
        return;
    }

    int fd = open(file_path, O_RDONLY); 

    if(fd == -1)
    {
        perror("Error opening the file!");
        return;
    }
    
    write_in_log(log_path, user, role, "list", time(NULL));

    printf("Permission: ");
    print_permission(st_file.st_mode);

    printf("File size:%ld\n", st_file.st_size);
    printf("Last modification: %s\n", ctime(&st_file.st_mtime));

    Report r;

    while(read(fd, &r, sizeof(Report)))
    {
       list_report_structure(r);
       printf("\n");
    }
    
    close(fd);
}

void update_threshold(char *district_id, char *value, char *user, char *role)
{
    struct stat st_dir, st_file;
    char file_path[256];
    char log_path[256];

    sprintf(file_path, "%s/district.cfg", district_id);
    sprintf(log_path, "%s/logged_district", district_id);

    if(stat(district_id, &st_dir) == -1)
    {
        printf("District folder not found!\n");
        return;
    }

    if(stat(file_path, &st_file) == -1)
    {
        printf("District file not found!\n");
        return;
    }

    if(is_manager(role) == 0)
    {
        perror("You do not have permission to write in this file!");
        return;
    }

    if(!(st_file.st_mode & S_IWUSR & S_IRUSR))
    {
        perror("The system has not given permission for the manager to write in this file!");
        printf("Current permissions for this file:");
        print_permission(st_file.st_mode);
        return;
    }

    int fd = open(file_path, O_RDONLY | O_WRONLY);

    if(fd == -1)
    {
        perror("Error opening the file!");
        return;
    }

    write_in_log(log_path, user, role, "update_threshold", time(NULL));

    char write_buff[512];
    int len = sprintf("%s", value);

    if(write(fd, write_buff, len) == -1)
        perror("Error when writing in district severity level file!");

    close(fd);
}

void which_command(char *command, char **string, char *user, char *role)
{
    if(strcmp(command, "--add") == 0)
        add(string[7], user, role);

    if(strcmp(command, "--list") == 0)
        list(string[7], user, role);

    if(strcmp(command, "--view") == 0)
        view(string[7], string[8], user, role);

    if(strcmp(command, "--remove_report") == 0)
        return;

    if(strcmp(command, "--update_threshold") == 0)
        update_threshold(string[7], string[8], user, role);

    if(strcmp(command, "--filter") == 0)
        return;
}

int main(int argc, char **argv)
{
    if(argc < 7)
    {
        printf("Eroare la numarul de argumente\n");
        exit(-1);
    }

    char role[100];
    char user[100];
    char command[100];

    if(argument_validation(argv, user, role, command) == 0)
    {
        printf("Argumentele introduse nu sunt corecte\n");
        exit(-2);
    }

    which_command(command, argv, user, role);

    return 0;
}
