#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "includes/firmata.h"
#include "includes/iniparser.h"

int parse_ini_file(char * ini_name);
void init(void);

#define NUM_REL 16

char* serial_port;


//char *mpd_name[5];

int rel[NUM_REL];
char* file[NUM_REL];
int inv[NUM_REL];

int ledr, ledb, ledg;
t_firmata     *firmata;

int main()
{
    printf("Starting Audioserver Control...\n");
    init();
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
    printf("Read configuration...\n");
    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1 ;
    }

    //iniparser_dump(ini, stderr);

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
void init(void){
    int i;
    parse_ini_file("/home/jkerrinnes/git/audiosrv-relais/rel.conf");
    firmata = firmata_new(serial_port); //init Firmata
    while(!firmata->isReady) //Wait until device is up
    firmata_pull(firmata);

    for(i=0; i<NUM_REL; i++) {
        firmata_pinMode(firmata, rel[i], MODE_OUTPUT);
        firmata_digitalWrite(firmata, rel[i], inv[i]);
    }

}

