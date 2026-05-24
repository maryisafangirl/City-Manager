#include <stdio.h>
#include <string.h> //strings
#include <stdlib.h> //everything
#include <errno.h> //errors
#include <dirent.h> //working with directories
#include <time.h> //timestamp
#include <sys/stat.h> //for stat
#include <sys/types.h> //for fork
#include <sys/wait.h> //signals 
#include <signal.h> //also signals
#include <unistd.h> //close & open
#include <fcntl.h> //files

//struct for the two parts of the coordonates
typedef struct 
{
    float longitude;
    float latitude;
}Coordonates;

//struct for a report
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

int monitor_notification = 0; //for seeing whether the monitor has been notified or not

//sees if the arguments are written in the correct form and order 
//returns 1 if they are, and 0 if they're not
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

//generates a unique id for each report; it is the last report's last id value +1
int generate_id(int fd) 
{
    struct stat st;
    
    //fstat returns information about a file
    if(fstat(fd, &st) == -1) 
    {
        perror("Error on fstat: ");
        return -1; 
    }

    if(st.st_size == 0) //if the size of the file is 0 then the first id is 1
        return 1;

    Report r;
   
    lseek(fd, -sizeof(Report), SEEK_END); //we move to the last report of the file
 
    //we read the last report in the file
    if(read(fd, &r, sizeof(Report)) == -1) 
    {
        printf("Error reading last report!\n");
        return -1;
    }

    //we set the id of the new report as the last report+1 to avoid having identical ids
    return r.report_id + 1;
}

//check if the current user's role is manager
int is_manager(char *role) 
{
    if(strcmp(role, "manager") == 0)
        return 1;

    return 0;
}

//check if the current user's role is inspector
int is_inspector(char *role)  
{
    if(strcmp(role, "inspector") == 0)
        return 1;

    return 0;
}

//sees if it can get the district folder and writes the file paths 
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

//writes the data about a commnand given as a function paramater
void write_in_log(char *log_path, char *user, char *role, char *command, time_t timestamp) 
{
    struct stat st_perm;
    
    //checks file permissions if the file exists
    if (stat(log_path, &st_perm) == 0) 
    {
        if (is_inspector(role) && !(st_perm.st_mode & S_IWGRP)) 
        {
            printf("Only the manager has the permission to write in the log!\n");
            return;
        }
        if (is_manager(role) && !(st_perm.st_mode & S_IWUSR)) 
        {
            printf("You do not have permission to write in this file!\n");
            return;
        }
    } 
    else if (is_inspector(role)) 
    {
        //fallback for when the file is about to be created
        printf("Only the manager has the permission to write in the log!\n");
        return;
    }

    struct stat st;
    int log_fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT, 0644); //rw-r--r--

    if(log_fd != -1)
    {
        chmod(log_path, 0644); //in case the file doesn't have the right permissions

        if(fstat(log_fd, &st) == -1)
        {
            close(log_fd);
            return;
        }

        char log_buffer[512];
        int len; //the size of the string we want to write in the log file

        if(strcmp(command, "add") == 0) //if the commnand is add we need to know wheteher the monitor waas notified or not
        {
            if(monitor_notification) //we need to know if the monitor has been notified or not
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

//checks the active link and deletes it if there is an error so that it's not dangling
void check_active_links() 
{
    DIR *dir = opendir("."); //opens current directory

    if(dir == NULL)
    {
        printf("Could not open directory!\n");
        return;
    }
    
    struct dirent *entry; //holds information about the directory
    struct stat lst, st; //store metadata

    while((entry = readdir(dir)) != NULL)
    {
        //lstat gets the metadata of the current item and saves it into lst
        if(lstat(entry->d_name, &lst) == -1) 
            continue; //skip the rest of the code and move on to the next file in the directory

        //checks if the current file is a symbloic link
        if(S_ISLNK(lst.st_mode)) 
        {
            //checks if the symbolic link starts with active-reports- so it doesnt't delete other symbolic links
            if(strncmp(entry->d_name, "active_reports-", 15) == 0) 
            {
                //stat() tries to follow the symbolic link to the file 
                //if the file exists, stat succeeds, and if not, the link points to nothing and nees to be deleted
                if(stat(entry->d_name, &st) == -1)
                {
                    printf("Warning, dangling link detected: %s\n", entry->d_name);
                    unlink(entry->d_name); //deletes the broken symbolic link
                }
            }
        }
    }

    closedir(dir); //closes the directory
}

void add(char *district_id, char *user, char *role)
{
    int fm = open(".monitor_pid", O_RDONLY); //we open the monitoring file so we can monitor when a report has been added

    if(fm == -1)
        printf("Monitor is not on!\n");
        
    int pid = -1;
    char buffer[32];

    memset(buffer, 0, sizeof(buffer)); //prevents the writing of unwanted characters

    if(read(fm, buffer, sizeof(buffer) - 1) > 0) //we read the pid from the monitoring file
        pid = atoi(buffer); //we turn the pid from string to integer
    
    close(fm);

    //checks if the monitor pid is valid and the system successfully delivers an alter to it
    if(pid > 0 && kill(pid, SIGUSR1) == 0)
        monitor_notification = 1; //monitor on
    else 
        monitor_notification = 0; //monitor off

    struct stat st;
    char file_path[256];
    char log_path[256];
    char severity_log_path[256];

    char target[256];
    char link_name[256];

    sprintf(target, "%s/reports.dat", district_id);
    sprintf(link_name, "active_reports-%s", district_id);

    //creates a symbolic link
    if(symlink(target, link_name) == -1) 
    {
        if (errno != EEXIST)
            printf("Warning when creating the symlink: %s\n", strerror(errno));
    }

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id); 
    sprintf(severity_log_path, "%s/district.cfg", district_id);

    //checks if there is a district folder, and if not, makes one
    if(stat(district_id, &st) == 0) 
    {
        if(S_ISDIR(st.st_mode))
            printf("District folder found.\n");
    }
    else
        mkdir(district_id, 0750); //rwxr-x---

    int fd = open(file_path, O_APPEND | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

    if(fd != -1) 
        chmod(file_path, 0664); //rw-rw-r-- 

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

    //we create district.cfg
    int fs = open(severity_log_path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);

    if(fs != -1) 
        chmod(severity_log_path, 0640); //rw-r-----

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
    scanf("%255[^\n]", report.description); //reading until ENTER

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

//prints all the details about a report
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

//view a specific report based on it's id
void view(char *district_id, char *report_id_char, char *user, char *role)
{
    if(report_id_char == NULL)
    {
        printf("Not enough arguments provided!\n");
        return;
    }

    int report_id = atoi(report_id_char); //we transform the id from string to integer

    struct stat st;
    char file_path[256];
    char log_path[256];

    if(!get_district_paths(district_id, file_path, log_path)) 
        return;

    int fd = open(file_path, O_RDONLY); 

    if(fd == -1)
    {
        printf("Error opening the reports file!");
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

    //we parse the reports file until we find the one we're looking for
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

//prints the permissions for a file
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

//prints all the reports from a district
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
        perror("Error opening the reports file!");
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

    //we read each report and print it
    while(read(fd, &r, sizeof(Report)))
    {
       list_report_structure(r);
       printf("\n");
    }
    
    close(fd);
}

//updates the severity level already in the file, if there is one, to the value parameter
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
        printf("Severity level file not found!\n");
        return;
    }

    //checks permission checking using the extracted bits
    if (is_inspector(role) && !(st_file.st_mode & S_IWGRP)) 
    {
        printf("You do not have permission to write in this file!\n");
        return;
    }
    if (is_manager(role) && !(st_file.st_mode & S_IWUSR)) 
    {
        printf("You do not have permission to write in this file!\n");
        return;
    }

    if((st_file.st_mode & 0777) != 0640)
    {
        printf("Diagnostic: Permission bits of district.cfg have been changed (not 640). Refusing update.\n");
        return;
    }

    int fd = open(file_path, O_RDONLY | O_WRONLY); //we always overwrite whatever is in the file

    if(fd == -1)
    {
        printf("Error opening the district.cfg file!\n"); 
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

//removes a report specified by it's id
void remove_report(char *district_id, char *report_id_char, char *user, char *role)
{
    if(report_id_char == NULL)
    {
        printf("Not enough arguments provided!\n");
        return;
    }

    struct stat st_dir_perm;
    
    //checks the district folder permissions since reports.dat allows group write but this command is restricted
    if(stat(district_id, &st_dir_perm) == 0) 
    {
        if (is_inspector(role) && !(st_dir_perm.st_mode & S_IWGRP)) 
        {
            printf("Only managers can call the remove report command!\n");
            return;
        }
    } 
    else if(is_inspector(role)) 
    {
        printf("Only managers can call the remove report command!\n");
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
        printf("Error opening the reports file!\n");
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

    //we read teh reports file until we find the one we want to remove
    while(read(fd, &r, sizeof(Report))) 
    {
        if(r.report_id == report_id)
        {
            found = 1;

            printf("Removing report:\n");
            list_report_structure(r);

            break;
        }

        pos += sizeof(Report); //we need to have the specific position before the report we want to remove 
    } 
        
    //if we can find the corresponding id in the file, we remove the report
    if(found == 1)
    {
        Report next_r;

        while(read(fd, &next_r, sizeof(Report)))
        {
            lseek(fd, pos, SEEK_SET); //we move the cursor to the right position in the file
            write(fd, &next_r, sizeof(Report)); //overwrites the deleted or outdated record slot with the data from the subsequent record
            
            pos += sizeof(Report); //advances the cursor forward for the next iteration
            lseek(fd, pos + sizeof(Report), SEEK_SET); //skips over the block that was jusr proccessed
        }

        //removes the redundant space left by the deleted report
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

//parses the condition basesd on the specific format
int parse_condition(const char *input, char *field, char *op, char *value) 
{
    if(sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3)
        return 1;
    
    return 0;
}

//matches the field to the available fields and we see if the value matches the condition
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

//
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

    //we read each report from the report file and we see if it matches all of the conditions
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
                    matches_all = 0; //if there are multiple conditions, we need to see if it matches all of them
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

        //if the report matches all the conditions, we print it
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

//removes the specified district 
void remove_district(char *district_id, char *user, char *role)
{
    struct stat st_dir_perm;
    if(stat(district_id, &st_dir_perm) == 0) 
    {
        if (is_inspector(role) && !(st_dir_perm.st_mode & S_IWGRP)) 
        {
            printf("Only managers may remove districts!\n");
            return;
        }
        if (is_manager(role) && !(st_dir_perm.st_mode & S_IWUSR)) 
        {
            printf("Only managers may remove districts!\n");
            return;
        }
    } 
    else if(is_inspector(role)) 
    {
        printf("Only managers may remove districts!\n");
        return;
    }

    //split the execution
    pid_t pid = fork();

    if(pid < 0)
    {
        //fail
        printf("Error on fork!\n");
        return;
    }
    else if(pid == 0)
    {
        //child
        execlp("rm", "rm", "-rf", district_id, NULL); //recursively and forcefully deletes the files in the district 
        printf("Error on execpl!\n");
        exit(0);
    }
    else if(pid > 0)
    {
        //parent
        int status; //to check whether or not the directory has been deleted

        waitpid(pid, &status, WCONTINUED); //we wait for the child to finish

        if(status != 0)
        {
            printf("The child process did not end as expected!\n");
            return;
        }

        char active_link_path[256];

        //we also delete the symbolic links
        sprintf(active_link_path, "active_reports-%s", district_id);

        if(unlink(active_link_path) == -1)
        {
            printf("Error on unlinking active_reports!\n");
            return;
        }
    }

    printf("The '%s' district has been removed.\n", district_id);
}

//decides based on the command which function to call and what arguments to pass
void which_command(char *command, char **string, char *user, char *role)
{
    if(strcmp(command, "--add") == 0) //the add function need the name of the district we add to
        add(string[6], user, role); 

    if(strcmp(command, "--list") == 0) //the list function needs the name of the district we want to see the reports from
        list(string[6], user, role);

    if(strcmp(command, "--view") == 0) //the view function needs the name of the district and the id of the report we want to see
        view(string[6], string[7], user, role);

    if(strcmp(command, "--remove_report") == 0) //the remove_report function needs the name of the district and the id of the report we want to remove
        remove_report(string[6], string[7], user, role);

    if(strcmp(command, "--update_threshold") == 0) //the update_threshold function needs the name of the district and the severity level we want to put
        update_threshold(string[6], string[7], user, role); 

    if(strcmp(command, "--filter") == 0) //the filter function needs the name of the district and argv[7] which contains a string with the filter constraints that need to be parsed
        filter(string[6], &string[7], user, role);

    if(strcmp(command, "--remove_district") == 0) //the remove_district function needs the name of the district we want to remove
        remove_district(string[6], user, role);
}

int main(int argc, char **argv)
{
    check_active_links();

    //on all the commands we have at least 6 arugments that are always there, so if there's any less then there's an error
    if(argc < 6) 
    {
        printf("Error at the number of arguments!\n");
        exit(-1);
    }

    //we save the string for each of these because we need them for the commands later on
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