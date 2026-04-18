#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int validare_argumente(char **string)
{
    if(strcmp(string[1], "city_manager") != 0)
        return 0;

    char rol_titlu[100], rol[100];
    strcpy(rol_titlu, string[2]);
    strcpy(rol, string[3]);

    char user_titlu[100], user[100];
    strcpy(user_titlu, string[4]);
    strcpy(user, string[5]);

    char operatie[100];
    strcpy(operatie, string[6]);

    if(strcmp(rol_titlu, "--role") != 0)
        return 0;

    if(strcmp(rol, "manager") != 0 && strcmp(rol, "inspector") != 0)
        return 0;

    if(strstr(user_titlu, "--user") == NULL) //verificare daca userul exista?
        return 0;

    if(strstr(operatie, "--add") == NULL && strstr(operatie, "--list") == NULL && strstr(operatie, "--view") == NULL && strstr(operatie, "--remore_report") == NULL && strstr(operatie, "--update_threshold") == NULL) //mai multa validare si aici?
        return 0;

    return 1;
}

int main(int argc, char **argv)
{
    if(argc < 7)
    {
        printf("Eroare la numarul de argumente\n");
        exit(-1);
    }

    printf("%d\n", validare_argumente(argv));

    /*
    if(validare_argumente(argv) == 0)
    {
        printf("Argumentele introduse nu sunt corecte\n");
        exit(-2);
    }
    */

    return 0;
}
