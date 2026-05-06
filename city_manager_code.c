#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h> //working with directories
#include <time.h> //timestamp
#include <sys/stat.h> //for stat
#include <sys/types.h> //for fork
#include <sys/wait.h>
#include <unistd.h> //close & open
#include <fcntl.h>
#include <signal.h>

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

int monitor_notification = 0;

int argument_validation(char **string, char *user_flag, char *role_flag, char *command_flag) 
{ 
    char role_title[100], role[100];
    strcpy(role_title, string[1]); 
    strcpy(role, string[2]);     

    char user_title[100], user[100];
    strcpy(user_title, string[3]); 
    strcpy(user, string[4]);       

    char command[100];
    strcpy(command, string[5]);   

    if(strcmp(role_title, "--role") != 0)
        return 0;

    if(strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0)
        return 0;

    if(strstr(user_title, "--user") == NULL) 
        return 0;

    if(strstr(command, "--add") == NULL && strstr(command, "--list") == NULL && strstr(command, "--view") == NULL && strstr(command, "--remove_report") == NULL && strstr(command, "--update_threshold") == NULL && strstr(command, "--filter") == NULL  && strstr(command, "--remove_district") == NULL)
        return 0;

    strcpy(user_flag, user);
    strcpy(role_flag, role);
    strcpy(command_flag, command);

    return 1;
}

int generate_id(int fd)
{
    struct stat st;
    
    if(fstat(fd, &st) == -1) 
    {
        printf("Error on fstat!\n");
        return -1; 
    }

    if(st.st_size == 0)
        return 1;

    Report r;
   
    lseek(fd, -sizeof(Report), SEEK_END);
 
    if(read(fd, &r, sizeof(Report)) == -1) 
    {
        printf("Error reading last report!\n");
        return -1;
    }

    return r.report_id + 1;
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

int get_district_paths(char *district_id, char *file_path, char *log_path)
{
    struct stat st;
    sprintf(file_path, "%s/reports.dat", district_id);

    if(log_path != NULL)
        sprintf(log_path, "%s/logged_district", district_id);

    if(stat(district_id, &st) == -1) 
    {
        printf("District folder not found!\n");
        return 0; 
    }
    
    return 1; 
}

void write_in_log(char *log_path, char *user, char *role, char *command, time_t timestamp)
{
    struct stat st;

    if(is_manager(role) == 0)
    {
        printf("Only the manager has the permission to write in the log!\n");
        return;
    }

    int log_fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if(log_fd != -1)
    {
        chmod(log_path, 0644); 

        if(fstat(log_fd, &st) == -1)
        {
            close(log_fd);
            return;
        }

        char log_buffer[512];
        int len;

        if(strcmp(command, "add") == 0)
        {
            if(monitor_notification)
                len = sprintf(log_buffer, "%s %s %s (monitor notified) %s", user, role, command, ctime(&timestamp)); 
            else
                len = sprintf(log_buffer, "%s %s %s (monitor not notified) %s", user, role, command, ctime(&timestamp));
        } 
        else
            len = sprintf(log_buffer, "%s %s %s %s", user, role, command, ctime(&timestamp)); 

        if(write(log_fd, log_buffer, len) == -1)
            printf("Error when writing in log file!\n");

        close(log_fd);
    }
}

void check_active_links() 
{
    DIR *dir = opendir("."); 

    if(dir == NULL)
    {
        printf("Could not open directory!\n");
        return;
    }
    
    struct dirent *entry;
    struct stat lst, st;

    while((entry = readdir(dir)) != NULL)
    {
        if(lstat(entry->d_name, &lst) == -1) 
            continue;

        if(S_ISLNK(lst.st_mode)) 
        {
            if(strncmp(entry->d_name, "active_reports-", 15) == 0) 
            {
                if(stat(entry->d_name, &st) == -1)
                {
                    printf("Warning, dangling link detected: %s\n", entry->d_name);
                    unlink(entry->d_name);
                }
            }
        }
    }

    closedir(dir);
}

void add(char *district_id, char *user, char *role)
{
    int fm = open(".monitor_pid", O_RDONLY);

    if(fm == -1)
        printf("Error opening the monitoring file!\n");
        
    int pid = -1;
    char buffer[32];

    memset(buffer, 0, sizeof(buffer));

    if(read(fm, buffer, sizeof(buffer) - 1) > 0)
        pid = atoi(buffer);
    
    close(fm);

    if(pid > 0 && kill(pid, SIGUSR1) == 0)
        monitor_notification = 1;
    else 
        monitor_notification = 0;

    struct stat st;
    char file_path[256];
    char log_path[256];
    char severity_log_path[256];

    char target[256];
    char link_name[256];

    sprintf(target, "%s/reports.dat", district_id);
    sprintf(link_name, "active_reports-%s", district_id);

    if(symlink(target, link_name) == -1) 
    {
        if (errno != EEXIST)
            printf("Warning when creating the symlink: %s\n", strerror(errno));
    }

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id); 
    sprintf(severity_log_path, "%s/district.cfg", district_id);

    if(stat(district_id, &st) == 0) 
    {
        if(S_ISDIR(st.st_mode))
            printf("District folder found.\n");
    }
    else
        mkdir(district_id, 0750);

    int fd = open(file_path, O_APPEND | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

    if(fd != -1) 
        chmod(file_path, 0664);

    if(fd == -1)
    {
        printf("Error opening the report file!\n");
        return;
    }

    if(fstat(fd, &st) == -1) 
    {
        printf("Error on fstat!\n");
        close(fd);
        return;
    }

    int fs = open(severity_log_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);

    if(fs != -1) 
        chmod(severity_log_path, 0640);

    if(fs == -1)
    {
        printf("Error opening the district severity level file!\n");
        close(fd);
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

    printf("Severity level(1|2|3):"); 
    scanf("%d", &report.severity_level);

    printf("Description:");
    getchar(); 
    scanf("%[^\n]", report.description); //reading until ENTER

    strcpy(report.name, user);

    report.timestamp = time(NULL);

    int id = generate_id(fd);

    if(id == -1)
    {
        printf("Error generating id. Aborting add command.\n");
        close(fd);
        return;
    }

    report.report_id = id;

    if(write(fd, &report, sizeof(Report)) == -1)
        printf("Error when writing in report file!\n");

    close(fd);

    write_in_log(log_path, user, role, "add", time(NULL));

    printf("The report has been added with the id: %d.\n", id);
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
    if(report_id_char == NULL)
    {
        printf("Not enough arguments provided!\n");
        return;
    }

    int report_id = atoi(report_id_char);

    struct stat st;
    char file_path[256];
    char log_path[256];

    if(!get_district_paths(district_id, file_path, log_path)) 
        return;

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

    write_in_log(log_path, user, role, "view", time(NULL));

    Report r;
    int found = 0;

    while(read(fd, &r, sizeof(Report)))
    {
        if(r.report_id == report_id)
        {
            list_report_structure(r);
            found = 1;
            break;
        }
    }

    if(found == 0)
        printf("The ID provided is invalid.\n");
    
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

void list(char *district_id, char *user, char *role) 
{
    struct stat st_file;
    char file_path[256];
    char log_path[256];

    if(!get_district_paths(district_id, file_path, log_path)) 
        return;

    int fd = open(file_path, O_RDONLY); 

    if(fd == -1)
    {
        perror("Error opening the file!");
        return;
    }

    if(fstat(fd, &st_file) == -1)
    {
        perror("Error on fstat");
        close(fd);
        return;
    }
    
    write_in_log(log_path, user, role, "list", time(NULL));

    printf("Permission: ");
    print_permission(st_file.st_mode);

    printf("File size:%lld\n", st_file.st_size);
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
    if(value == NULL)
    {
        printf("Not enough arguments provided!\n");
        return;
    }

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

    if((st_file.st_mode & 0777) != 0640)
    {
        printf("Diagnostic: Permission bits of district.cfg have been changed (not 640). Refusing update.\n");
        return;
    }

    if(is_manager(role) == 0)
    {
        printf("You do not have permission to write in this file!\n");
        return;
    }

    int fd = open(file_path, O_RDONLY | O_WRONLY);

    if(fd == -1)
    {
        printf("Error opening the file!\n");
        return;
    }

    write_in_log(log_path, user, role, "update_threshold", time(NULL));

    char write_buff[512];
    int len = sprintf(write_buff, "%s", value);

    if(write(fd, write_buff, len) == -1)
        printf("Error when writing in district severity level file!\n");
    else
        printf("Threshold updated to: %s.\n", value);

    close(fd);
}

void remove_report(char *district_id, char *report_id_char, char *user, char *role)
{
    if(report_id_char == NULL)
    {
        printf("Not enough arguments provided!\n");
        return;
    }

    if(is_inspector(role))
    {
        printf("Only managers can call the remove report command!");
        return;
    }

    int report_id = atoi(report_id_char);

    struct stat st;
    char file_path[256], log_path[256];

    if(!get_district_paths(district_id, file_path, log_path)) 
        return;

    int fd = open(file_path, O_RDWR); 

    if(fd == -1)
    {
        printf("Error opening the file!\n");
        return;
    }

    if(fstat(fd, &st) == -1)
    {
        printf("Error on fstat!\n");
        close(fd);
        return;
    }

    Report r;
    int pos = 0, found = 0;

    while(read(fd, &r, sizeof(Report))) 
    {
        if(r.report_id == report_id)
        {
            found = 1;

            printf("Removing report:\n");
            list_report_structure(r);

            break;
        }

        pos += sizeof(Report);
    }
        
    if(found == 1)
    {
        Report next_r;

        while(read(fd, &next_r, sizeof(Report)))
        {
            lseek(fd, pos, SEEK_SET);
            write(fd, &next_r, sizeof(Report));
            
            pos += sizeof(Report);
            lseek(fd, pos + sizeof(Report), SEEK_SET);
        }

        if(ftruncate(fd, st.st_size - sizeof(Report)) == -1)
        {
            printf("Error with ftruncate!\n");
            close(fd);
            return;
        }

        printf("The report has been removed.\n");
        write_in_log(log_path, user, role, "remove_report", time(NULL));
    }
    else
        printf("The report could not be found.\n");
    
    close(fd);
}

int parse_condition(const char *input, char *field, char *op, char *value) 
{
    if(sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3)
        return 1;
    
    return 0;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) 
{
    if(strcmp(field, "severity") == 0)
    {
        int val = atoi(value); 
        
        if(strcmp(op, "==") == 0)
            return r->severity_level == val;

        if(strcmp(op, "!=") == 0)
            return r->severity_level != val;

        if(strcmp(op, "<")  == 0)
            return r->severity_level < val;

        if(strcmp(op, "<=") == 0)
            return r->severity_level <= val;

        if(strcmp(op, ">")  == 0)
            return r->severity_level > val;

        if(strcmp(op, ">=") == 0)
            return r->severity_level >= val;
    }
    else if (strcmp(field, "timestamp") == 0) 
    {
        long val = atol(value); 

        if(strcmp(op, "==") == 0)
            return r->timestamp == val;

        if(strcmp(op, "!=") == 0)
            return r->timestamp != val;

        if(strcmp(op, "<")  == 0)
            return r->timestamp < val;

        if(strcmp(op, "<=") == 0)
            return r->timestamp <= val;

        if(strcmp(op, ">")  == 0)
            return r->timestamp > val;

        if(strcmp(op, ">=") == 0)
            return r->timestamp >= val;
    }
    else if(strcmp(field, "category") == 0)
    {
        if(strcmp(op, "==") == 0)
            return strcmp(r->issue_category, value) == 0;

        if(strcmp(op, "!=") == 0)
            return strcmp(r->issue_category, value) != 0;
    }
    else if(strcmp(field, "inspector") == 0)
    {
        if(strcmp(op, "==") == 0)
            return strcmp(r->name, value) == 0;

        if(strcmp(op, "!=") == 0)
            return strcmp(r->name, value) != 0;
    }

    return 0; 
}

void filter(char *district_id, char **condition, char *user, char *role)
{
    struct stat st;
    char file_path[256], log_path[256];

    if(!get_district_paths(district_id, file_path, log_path)) 
        return;

    int fd = open(file_path, O_RDONLY); 

    if(fd == -1)
    {
        printf("Error opening the file!\n");
        return;
    }
    
    if(fstat(fd, &st) == -1)
    {
        printf("Error on fstat.\n");
        close(fd);
        return;
    }

    write_in_log(log_path, user, role, "filter", time(NULL));

    Report r;

    int any_matches = 0;

    while(read(fd, &r, sizeof(Report))) 
    {
        int matches_all = 1;
        int i = 0;

        while(condition[i] != NULL)
        {
            char field[256], op[256], value[256];

            if(parse_condition(condition[i], field, op, value) == 1)
            {
                if(match_condition(&r, field, op, value) == 0)
                {
                    matches_all = 0;
                    break;
                }
            }
            else
            {
                printf("The filter condition does not respect the format. Aborting command.\n");
                close(fd);
                return;
            }

            i++;
        }

        if(matches_all == 1)
        {
            list_report_structure(r);
            printf("\n");
            any_matches = 1;
        }
    }

    if(any_matches == 0)
        printf("No reports matched the condtion/s.\n");
    
    close(fd);
}

void remove_district(char *district_id, char *user, char *role)
{
    if(is_manager(role) == 0)
    {
        printf("Only managers may remove districts!\n");
        return;
    }

    pid_t pid = fork();

    if(pid < 0)
    {
        printf("Error on fork!\n");
        return;
    }
    else if(pid == 0)
    {
        //child
        execlp("rm", "rm", "-rf", district_id, NULL);
        printf("Error on execpl!\n");
        exit(0);
    }
    else if(pid > 0)
    {
        //parent
        int status;

        waitpid(pid, &status, WCONTINUED);

        if(status != 0)
        {
            printf("The child process did not end as expected!\n");
            return;
        }

        char active_link_path[256];

        sprintf(active_link_path, "active_reports-%s", district_id);

        if(unlink(active_link_path) == -1)
        {
            printf("Error on unlinking active_reports!\n");
            return;
        }
    }

    printf("The '%s' district has been removed.\n", district_id);
}

void which_command(char *command, char **string, char *user, char *role)
{
    if(strcmp(command, "--add") == 0)
        add(string[6], user, role); 

    if(strcmp(command, "--list") == 0)
        list(string[6], user, role);

    if(strcmp(command, "--view") == 0)
        view(string[6], string[7], user, role);

    if(strcmp(command, "--remove_report") == 0)
        remove_report(string[6], string[7], user, role);

    if(strcmp(command, "--update_threshold") == 0)
        update_threshold(string[6], string[7], user, role); 

    if(strcmp(command, "--filter") == 0)
        filter(string[6], &string[7], user, role);

    if(strcmp(command, "--remove_district") == 0)
        remove_district(string[6], user, role);
}

int main(int argc, char **argv)
{
    check_active_links();

    if(argc < 6) 
    {
        printf("Error at the number of arguments!\n");
        exit(-1);
    }

    char role[100];
    char user[100];
    char command[256];

    if(argument_validation(argv, user, role, command) == 0)
    {
        printf("The arguments are not correct!\n");
        exit(-2);
    }

    which_command(command, argv, user, role);

    return 0;
}
