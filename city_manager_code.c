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
        return -1; 
    }

    if(st.st_size == 0)
        return 1;

    Report r;
   
    lseek(fd, -sizeof(Report), SEEK_END);
 
    if (read(fd, &r, sizeof(Report)) == -1) 
    {
        perror("Error reading last report");
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
        int len = sprintf(log_buffer, "%s %s %s %s", user, role, command, ctime(&timestamp)); 

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

    int fd = open(file_path, O_APPEND | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

    if(fd == -1)
    {
        perror("Error opening the report file!");
        return;
    }

    if (fstat(fd, &st) == -1) 
    {
        perror("Error on fstat");
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

    int id = generate_id(fd);

    if(id == -1)
    {
        printf("Error generating id. Aborting add command.\n");
        close(fd);
        return;
    }

    report.report_id = id;

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

    if (fstat(fd, &st) == -1) 
    {
        perror("Error on fstat");
        return;
    }

    write_in_log(log_path, user, role, "view", time(NULL));

    Report r;

    while(read(fd, &r, sizeof(Report)))
    {
        if(r.report_id == report_id)
        {
            list_report_structure(r);
            return;
        }
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

    if (fstat(fd, &st_file) == -1) 
    {
        perror("Error on fstat");
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

    if(!(st_file.st_mode & S_IWUSR))
    {
        printf("The system has not given permission for the manager to write in this file!\n");
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

    if (fstat(fd, &st_file) == -1) 
    {
        perror("Error on fstat");
        return;
    }

    write_in_log(log_path, user, role, "update_threshold", time(NULL));

    char write_buff[512];
    int len = sprintf(write_buff, "%s", value);

    if(write(fd, write_buff, len) == -1)
        perror("Error when writing in district severity level file!");

    close(fd);
}

void remove_report(char *district_id, char *report_id_char, char *user, char *role)
{
    if(is_manager(role) == 0)
    {
        printf("Only managers can call the remove report command!");
        return;
    }

    int report_id = atoi(report_id_char);

    struct stat st;
    char file_path[256], log_path[256];

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id);

    if(stat(district_id, &st) == -1) 
    {
        printf("District folder not found.\n");
        return;
    }

    int fd = open(file_path, O_RDWR); 

    if(fd == -1)
    {
        perror("Error opening the file!");
        return;
    }

    if (fstat(fd, &st) == -1) 
    {
        close(fd);
        return;
    }
    
    if (fstat(fd, &st) == -1) 
    {
        perror("Error on fstat");
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
            perror("Error with ftruncate!\n");
            close(fd);
            return;
        }

        printf("The report has been removed.\n");
        write_in_log(log_path, user, role, "view", time(NULL));
    }
    else
        printf("The report could not be found.\n");
    
    close(fd);
}

int parse_condition(const char *input, char *field, char *op, char *value) 
{
    // Folosim sscanf pentru a extrage datele dintr-un format fix.
    // %[^:] inseamna "citeste absolut toate caracterele pana cand intalnesti caracterul ':'"
    // Ultimul %s citeste restul string-ului.
    
    if (sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3) 
        return 1; // Succes: am extras exact 3 elemente
    
    return 0; // Eroare: string-ul nu a respectat formatul "a:b:c"
}

int match_condition(Report *r, const char *field, const char *op, const char *value) 
{
    // 1. Verificam pentru SEVERITY (are nevoie de conversie in INT)
    if (strcmp(field, "severity") == 0) 
    {
        int val = atoi(value); // Transformam string-ul in numar intreg
        
        if (strcmp(op, "==") == 0) 
            return r->severity_level == val;

        if (strcmp(op, "!=") == 0) 
            return r->severity_level != val;

        if (strcmp(op, "<")  == 0) 
            return r->severity_level < val;

        if (strcmp(op, "<=") == 0) 
            return r->severity_level <= val;

        if (strcmp(op, ">")  == 0) 
            return r->severity_level > val;

        if (strcmp(op, ">=") == 0) 
            return r->severity_level >= val;
    }
    
    // 2. Verificam pentru TIMESTAMP (are nevoie de conversie in LONG)
    else if (strcmp(field, "timestamp") == 0) 
    {
        long val = atol(value); // Transformam string-ul in long
        
        if (strcmp(op, "==") == 0) 
            return r->timestamp == val;

        if (strcmp(op, "!=") == 0) 
            return r->timestamp != val;

        if (strcmp(op, "<")  == 0) 
            return r->timestamp < val;

        if (strcmp(op, "<=") == 0) 
            return r->timestamp <= val;

        if (strcmp(op, ">")  == 0) 
            return r->timestamp > val;

        if (strcmp(op, ">=") == 0) 
            return r->timestamp >= val;
    }
    
    // 3. Verificam pentru CATEGORY (comparare de string-uri)
    else if (strcmp(field, "category") == 0)
    {
        if (strcmp(op, "==") == 0) 
            return strcmp(r->issue_category, value) == 0;

        if (strcmp(op, "!=") == 0) 
            return strcmp(r->issue_category, value) != 0;
    }
    
    // 4. Verificam pentru INSPECTOR (comparare de string-uri folosind 'name')
    else if (strcmp(field, "inspector") == 0)
    {
        if (strcmp(op, "==") == 0) 
            return strcmp(r->name, value) == 0;

        if (strcmp(op, "!=") == 0) 
            return strcmp(r->name, value) != 0;
    }

    // Daca ajungem aici, inseamna ca field-ul sau operatorul sunt invalide
    return 0; 
}

void filter(char *district_id, char *condition, char *user, char *role)
{
    char field[256], op[256], value[256];

    if(parse_condition(condition, field, op, value) == 0)
    {
        printf("The filter condition does not respect the format. Aborting command.\n");
        return;
    }

    struct stat st;
    char file_path[256], log_path[256];

    sprintf(file_path, "%s/reports.dat", district_id);
    sprintf(log_path, "%s/logged_district", district_id);

    if(stat(district_id, &st) == -1) 
    {
        printf("District folder not found.\n");
        return;
    }

    int fd = open(file_path, O_RDWR); 

    if(fd == -1)
    {
        perror("Error opening the file!");
        return;
    }

    if (fstat(fd, &st) == -1) 
    {
        close(fd);
        return;
    }
    
    if (fstat(fd, &st) == -1) 
    {
        perror("Error on fstat");
        return;
    }

    Report r;


    while(read(fd, &r, sizeof(Report))) 
    {
        if(match_condition(&r, field, op, value) == 1)
            list_report_structure(r);
    }

    close(fd);
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
    {
        char conditions[256];

        int i = 7;

        while(string[i] != NULL)
        {
            strcat(conditions, string[i]);
            i++;
        }

        filter(string[6], conditions, user, role);
    }
}

int main(int argc, char **argv)
{
    if(argc < 6) 
    {
        printf("Eroare la numarul de argumente\n");
        exit(-1);
    }

    char role[100];
    char user[100];
    char command[256];

    if(argument_validation(argv, user, role, command) == 0)
    {
        printf("Argumentele introduse nu sunt corecte\n");
        exit(-2);
    }

    which_command(command, argv, user, role);

    return 0;
}