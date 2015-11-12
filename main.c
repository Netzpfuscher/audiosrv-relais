#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "includes/firmata.h"
#include "includes/iniparser.h"

#define FALSE 0
#define TRUE  1

int parse_ini_file(char * ini_name);
int init(void);
void show_help(void);

#define NUM_REL 16

char* serial_port;


//char *mpd_name[5];

int rel[NUM_REL];
char* file[NUM_REL];
char* arg_ini;
int inv[NUM_REL];

int ledr, ledb, ledg;
t_firmata     *firmata;

int main(int argc, char *argv[])
{
    printf("\n");

    if (argc < 2){
        printf("Main: No config specified... relais /etc/relais.conf\n");
        return 0;
    }
    arg_ini = strdup(argv[1]);
    printf("Main: Starting Audioserver Control...\n");

    if(init() == -1){
        printf("Main: Error... Exit\n\n");
        return 0;
    }
    FILE *fp;

    int   i = 0;
    int   w = 0;

    while (1)
    {
        for(w=0; w<NUM_REL; i++) {
            fp = fopen(file[w],"r");
            if (fp != NULL ){
                if(inv[w]==1){
                    firmata_digitalWrite(firmata, rel[w], LOW);
                }else{
                    firmata_digitalWrite(firmata, rel[w], HIGH);
                }
                fclose(fp);
            }else{
                if(inv[w]==1){
                    firmata_digitalWrite(firmata, rel[w], HIGH);
                }else{
                    firmata_digitalWrite(firmata, rel[w], LOW);
                }
            }
        }

        sleep(1);

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
    int i;
    printf("Parse: Read configuration: %s\n", ini_name);
    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        //fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1 ;
    }

    s = iniparser_getstring(ini, "firmata:port", NULL);
    serial_port = strdup(s);
    int ret;
    for(i=0; i<NUM_REL; i++) {
        char *new_str;

        ret = asprintf(&new_str,"%s%d","files:file",i+1);
        s = iniparser_getstring(ini, new_str, NULL);
        file[i] = strdup(s);
        ret = asprintf(&new_str,"%s%d","firmata:rel_port_",i+1);
        rel[i] = iniparser_getint(ini, new_str, -1);
        ret = asprintf(&new_str,"%s%d","firmata:rel_inv_",i+1);
        inv[i] = iniparser_getint(ini, new_str, -1);;
    }

    ledg = iniparser_getint(ini, "firmata:ledg", -1);
    ledb = iniparser_getint(ini, "firmata:ledb", -1);
    ledr = iniparser_getint(ini, "firmata:ledr", -1);
    iniparser_freedict(ini);
    return 0 ;
}
int init(void){
    int i;
    if(parse_ini_file(arg_ini) == -1){
        return -1;
    }
    firmata = firmata_new(serial_port); //init Firmata
    if (firmata == NULL){
        return -1;
    }
    while(!firmata->isReady) //Wait until device is up
    firmata_pull(firmata);

    for(i=0; i<NUM_REL; i++) {
        firmata_pinMode(firmata, rel[i], MODE_OUTPUT);
        firmata_digitalWrite(firmata, rel[i], inv[i]);
    }
        return 0;
}


