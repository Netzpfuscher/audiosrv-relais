#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "includes/firmata.h"
#include "includes/iniparser.h"
//#include "includes/util.h"

int parse_ini_file(char * ini_name);
void init(void);
int f1, f2, f3, f4, f5, f6, f7, f8;

char* serial_port;
char* file1;
char* file2;
char* file3;
char* file4;
char* file5;
char* file6;
char* file7;
char* file8;

//char *mpd_name[5];

int rel1, rel2, rel3, rel4, rel5, rel6, rel7, rel8, ledr, ledb, ledg;
t_firmata     *firmata;

int main()
{
    printf("Starting Audioserver Control...\n");
    init();
    FILE *fp;


    //struct mpd_connection *conn = setup_connection();

    int   i = 0;


    while (1)
    {
        fp = fopen(file1,"r");
        if (fp != NULL ){
            f1 = 1;
            fclose(fp);
        }else{
            f1 = 0;
        }
        fp = fopen(file2,"r");
        if (fp != NULL ){
            f2 = 1;
            fclose(fp);
        }else{
            f2 = 0;
        }
        fp = fopen(file3,"r");
        if (fp != NULL ){
            f3 = 1;
            fclose(fp);
        }else{
            f3 = 0;
        }
        fp = fopen(file4,"r");
        if (fp != NULL ){
            f4 = 1;
            fclose(fp);
        }else{
            f4 = 0;
        }
        fp = fopen(file5,"r");
        if (fp != NULL ){
            f5 = 1;
            fclose(fp);
        }else{
            f5 = 0;
        }
        fp = fopen(file6,"r");
        if (fp != NULL ){
            f6 = 1;
            fclose(fp);
        }else{
            f6 = 0;
        }
        fp = fopen(file7,"r");
        if (fp != NULL ){
            f7 = 1;
            fclose(fp);
        }else{
            f7 = 0;
        }
        fp = fopen(file8,"r");
        if (fp != NULL ){
            f8 = 1;
            fclose(fp);
        }else{
            f8 = 0;
        }


        sleep(2);
        if (f1){
            firmata_digitalWrite(firmata, rel1, LOW);
        }else{
            firmata_digitalWrite(firmata, rel1, HIGH);
        }
        if (f2){
            firmata_digitalWrite(firmata, rel2, LOW);
        }else{
            firmata_digitalWrite(firmata, rel2, HIGH);
        }
        if (f3){
            firmata_digitalWrite(firmata, rel3, LOW);
        }else{
            firmata_digitalWrite(firmata, rel3, HIGH);
        }
        if (f4){
            firmata_digitalWrite(firmata, rel4, LOW);
        }else{
            firmata_digitalWrite(firmata, rel4, HIGH);
        }
        if (f5){
            firmata_digitalWrite(firmata, rel5, LOW);
        }else{
            firmata_digitalWrite(firmata, rel5, HIGH);
        }
        if (f6){
            firmata_digitalWrite(firmata, rel6, LOW);
        }else{
            firmata_digitalWrite(firmata, rel6, HIGH);
        }
        if (f7){
            firmata_digitalWrite(firmata, rel7, LOW);
        }else{
            firmata_digitalWrite(firmata, rel7, HIGH);
        }
        if (f8){
            firmata_digitalWrite(firmata, rel8, LOW);
        }else{
            firmata_digitalWrite(firmata, rel8, HIGH);
        }

        if (i == 0){
            i++;
            firmata_digitalWrite(firmata, ledb, HIGH);
            firmata_digitalWrite(firmata, ledg, LOW);
            firmata_digitalWrite(firmata, ledr, LOW);
        }else if (i == 1){
            i++;
            firmata_digitalWrite(firmata, ledb, LOW);
            firmata_digitalWrite(firmata, ledg, HIGH);
            firmata_digitalWrite(firmata, ledr, LOW);
        }else{
            i = 0;
            firmata_digitalWrite(firmata, ledb, LOW);
            firmata_digitalWrite(firmata, ledg, LOW);
            firmata_digitalWrite(firmata, ledr, HIGH);
        }

	}
    return 0;
}

int parse_ini_file(char * ini_name)
{
    dictionary  *   ini ;
    const char  *   s;
    printf("Read configuration...\n");
    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1 ;
    }

    //iniparser_dump(ini, stderr);

    s = iniparser_getstring(ini, "firmata:port", NULL);
    serial_port = strdup(s);

    s = iniparser_getstring(ini, "files:file1", NULL);
    file1 = strdup(s);

    s = iniparser_getstring(ini, "files:file2", NULL);
    file2 = strdup(s);

    s = iniparser_getstring(ini, "files:file3", NULL);
    file3 = strdup(s);

    s = iniparser_getstring(ini, "files:file4", NULL);
    file4 = strdup(s);

    s = iniparser_getstring(ini, "files:file5", NULL);
    file5 = strdup(s);

    s = iniparser_getstring(ini, "files:file6", NULL);
    file6 = strdup(s);

    s = iniparser_getstring(ini, "files:file7", NULL);
    file7 = strdup(s);

    s = iniparser_getstring(ini, "files:file8", NULL);
    file8 = strdup(s);


    rel1 = iniparser_getint(ini, "firmata:rel1", -1);
    rel2 = iniparser_getint(ini, "firmata:rel2", -1);
    rel3 = iniparser_getint(ini, "firmata:rel3", -1);
    rel4 = iniparser_getint(ini, "firmata:rel4", -1);
    rel5 = iniparser_getint(ini, "firmata:rel5", -1);
    rel6 = iniparser_getint(ini, "firmata:rel6", -1);
    rel7 = iniparser_getint(ini, "firmata:rel7", -1);
    rel8 = iniparser_getint(ini, "firmata:rel8", -1);

    ledg = iniparser_getint(ini, "firmata:ledg", -1);
    ledb = iniparser_getint(ini, "firmata:ledb", -1);
    ledr = iniparser_getint(ini, "firmata:ledr", -1);
    iniparser_freedict(ini);
    return 0 ;
}
void init(void){
    parse_ini_file("/etc/rel.conf");
    firmata = firmata_new(serial_port); //init Firmata
    while(!firmata->isReady) //Wait until device is up
    firmata_pull(firmata);

    firmata_pinMode(firmata, rel1, MODE_OUTPUT);
    firmata_pinMode(firmata, rel2, MODE_OUTPUT);
    firmata_pinMode(firmata, rel3, MODE_OUTPUT);
    firmata_pinMode(firmata, rel4, MODE_OUTPUT);
    firmata_pinMode(firmata, rel5, MODE_OUTPUT);
    firmata_pinMode(firmata, ledb, MODE_OUTPUT);
    firmata_pinMode(firmata, ledg, MODE_OUTPUT);
    firmata_pinMode(firmata, ledr, MODE_OUTPUT);
    firmata_pinMode(firmata, rel6, MODE_OUTPUT);
    firmata_pinMode(firmata, rel7, MODE_OUTPUT);
    firmata_pinMode(firmata, rel8, MODE_OUTPUT);

    firmata_digitalWrite(firmata, rel1, HIGH);
    firmata_digitalWrite(firmata, rel2, HIGH);
    firmata_digitalWrite(firmata, rel3, HIGH);
    firmata_digitalWrite(firmata, rel4, HIGH);
    firmata_digitalWrite(firmata, rel5, HIGH);
    firmata_digitalWrite(firmata, rel6, HIGH);
    firmata_digitalWrite(firmata, rel7, HIGH);
    firmata_digitalWrite(firmata, rel8, HIGH);

}

