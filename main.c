
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

void set_relais(int rel, int state);
void led_blink(void);
int get_cards(void);
int get_active (void);

#define PCM_PATH "/proc/asound/pcm"
#define NUM_REL 16

int power_time = 0;
int amp_time = 0;

struct ini {
	int relais;
	int invert;
	int matrix;
};

struct led_state {
	int red;
	int green;
	int blue;
	int state;
};


char* serial_port;

int active_cards[NUM_REL];
int power_relais = 0;
struct ini confi[NUM_REL];
struct led_state led;

char* arg_ini;

//int ledr, ledb, ledg;
int num_cards;

t_firmata     *firmata;

int main(int argc, char *argv[])
{
    printf("\n");

    if (argc < 2){
        printf("Main: No config specified... use default: /etc/relais.conf\n");
        arg_ini = "/etc/relais.conf";
    }else{
    	arg_ini = strdup(argv[1]);
    }
    printf("Main: Starting Audioserver Control...\n");

    if(init() == -1){
        printf("Main: Error... Exit\n\n");
        return 0;
    }
    num_cards = get_cards();

    int   i = 0;
    for(i=0;i<NUM_REL;i++){
	active_cards[i] = 0;
    }

    while (1)
    {
	set_relais(power_relais,get_active());

        for(i=0; i<num_cards; i++) {
        	set_relais(confi[i].matrix,active_cards[i]);
        }

        sleep(1);
	led_blink();
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

        ret = asprintf(&new_str,"%s%d","firmata:rel_port_",i+1);
        if(ret>0){
		confi[i].relais = iniparser_getint(ini, new_str, -1);
		free(new_str);
	}

	ret = asprintf(&new_str,"%s%d","firmata:rel_inv_",i+1);
	if(ret>0){
        	confi[i].invert = iniparser_getint(ini, new_str, -1);
		free(new_str);
	}

	ret = asprintf(&new_str,"%s%d","matrix:card",i);
	if(ret>0){
		confi[i].matrix = iniparser_getint(ini, new_str, -1);
		free(new_str);
	}
    }
    power_time = iniparser_getint(ini, "timing:power_time",0);
    amp_time = iniparser_getint(ini, "timing:amp_time",0);
    power_relais = iniparser_getint(ini, "firmata:power_relais",-1);
    led.green = iniparser_getint(ini, "firmata:ledg", -1);
    led.blue = iniparser_getint(ini, "firmata:ledb", -1);
    led.red = iniparser_getint(ini, "firmata:ledr", -1);
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

	firmata_pinMode(firmata, led.green, MODE_OUTPUT);
	firmata_pinMode(firmata, led.blue, MODE_OUTPUT);
	firmata_pinMode(firmata, led.red, MODE_OUTPUT);

    for(i=0; i<NUM_REL; i++) {
        firmata_pinMode(firmata, confi[i].relais, MODE_OUTPUT);
        firmata_digitalWrite(firmata, confi[i].relais, confi[i].invert);
    }
    return 0;
}

void set_relais (int rel, int state){
	if (state>0){
		if (confi[rel].invert == 0){
			firmata_digitalWrite(firmata, confi[rel].relais, HIGH);
		}else{
			firmata_digitalWrite(firmata, confi[rel].relais, LOW);
		}
	}else{
		if (confi[rel].invert == 0){
			firmata_digitalWrite(firmata, confi[rel].relais, LOW);
		}else{
			firmata_digitalWrite(firmata, confi[rel].relais, HIGH);
		}
	}

}

void led_blink(void){
        if (led.state == 0){
            led.state++;
            firmata_digitalWrite(firmata, led.blue, HIGH);
            firmata_digitalWrite(firmata, led.green, LOW);
            firmata_digitalWrite(firmata, led.red, LOW);
        }else if (led.state == 1){
            led.state++;
            firmata_digitalWrite(firmata, led.blue, LOW);
            firmata_digitalWrite(firmata, led.green, HIGH);
            firmata_digitalWrite(firmata, led.red, LOW);
        }else{
            led.state = 0;
            firmata_digitalWrite(firmata, led.blue, LOW);
            firmata_digitalWrite(firmata, led.green, LOW);
            firmata_digitalWrite(firmata, led.red, HIGH);
        }
}

int get_cards(void){
	int num = 0;
	FILE *pcm;
	pcm = fopen(PCM_PATH,"r");
        if (pcm != NULL ){
		while(!feof(pcm)){
  			if(fgetc(pcm) == '\n')
  			{
    			num++;
  			}
		}
        	fclose(pcm);
		return num;
	}else{
		return -1;
	}
}

int get_active(void){
	FILE *stream;
	#define BYTE_READ 255
	#define LINE_PLAY 4
	char str[BYTE_READ];
	char needle[] = "Stop";
	char *new_str;
	int i;
	int card;
	int power = 0;
	static int power_count = 0;

	for (card=0;card < num_cards;card++){
		if(asprintf(&new_str,"/proc/asound/card%i/stream0",card) != -1){
			stream = fopen(new_str,"r");
			if (stream != NULL){
				for (i=0;i<LINE_PLAY;i++){
					if(fgets(str, BYTE_READ, stream)!=NULL ){
						if(i==LINE_PLAY-1 && strstr(str, needle)){
							if(active_cards[i]){
							active_cards[i]--;
							}
						}else if(i==LINE_PLAY-1){
							active_cards[i] = amp_time;
							power = 1;
						}
					}
				}
			fclose(stream);
			}
		free(new_str);
		}
	}
	if(power){
		power_count = power_time;
	}else{
		if(power_count){
			power_count--;
		}
	}
	return power;
}

