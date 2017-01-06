#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "includes/firmata.h"
#include "includes/iniparser.h"
#include "includes/common.h"

#define FALSE 0
#define TRUE  1

int parse_ini_file(char * ini_name);
int init(void);
void show_help(void);

void set_relais(int rel, int state);
void led_blink(void);
int get_cards(void);
int get_active (void);
void write_mpd(void);

void mpd_startup(void);


#define PCM_PATH "/proc/asound/pcm"
#define NUM_REL 16

int power_time = 0;
int amp_time = 0;
int power_count = 0;

struct ini {
	int relais;
	int invert;
	int matrix;
	int relais_state;
	char* alsa_dev;
	char* shair_name;
	FILE *shairport;
};

int num_mpd_instances;
struct mpd {
	int port;
	int num_outputs;
	int outputs[NUM_REL];
};

struct led_state {
	int red;
	int green;
	int blue;
	int state;
};



char* serial_port;
char pbuffer[128];

int active_cards[NUM_REL];
struct ini confi[NUM_REL];
struct led_state led;
struct mpd mpd_conf[NUM_REL];

int power_relais = 0;

int port = 5000;
int port_incr = 10;

char* arg_ini;

int num_cards;

t_firmata     *firmata;

void sig_handler(int signo)
{
	if (signo == SIGINT)
    	printf("received SIGINT\n");
	exit(0);
}


int main(int argc, char *argv[])
{
    num_cards = get_cards();

    printf("\n");

    if (signal(SIGINT, sig_handler) == SIG_ERR) printf("\ncan't catch SIGINT\n");

    if (argc < 2){
        printf(C_TOPIC "Main: " C_DEF "No config specified... use default: /etc/relais.conf\n");
        arg_ini = "/etc/relais.conf";
    }else{
    	arg_ini = strdup(argv[1]);
    }
    printf(C_TOPIC "Main: " C_DEF "Starting Audioserver Control...\n");

    if(init() == -1){
        printf(C_TOPIC "Main: " C_DEF "Error... Exit\n\n");
        return 0;
    }

    mpd_startup();

    int   i = 0;
    char *new_str;
	int ret;
	for(i=0;i<num_cards;i++){
		if(confi[i].alsa_dev != NULL && confi[i].shair_name != NULL){
        	ret = asprintf(&new_str,"shairport-sync -p %i -a %s -o alsa -- -d %s",port,confi[i].shair_name,confi[i].alsa_dev);
			confi[i].shairport = popen(new_str, "r");
			usleep(100000);
			free(new_str);
			port = port + port_incr;
		}
	}

    for(i=0;i<NUM_REL;i++){
	active_cards[i] = FALSE;
    }

    while (1)
    {
	int pow;
	pow = get_active();
	set_relais(power_relais,pow);

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
    int w;
    num_mpd_instances = 0;
    printf(C_TOPIC "Parse: " C_DEF "Read configuration: %s\n", ini_name);
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
	mpd_conf[i].num_outputs = 0;

        ret = asprintf(&new_str,"%s%d","firmata:rel_port_",i);
        if(ret>0){
		confi[i].relais = iniparser_getint(ini, new_str, -1);
		free(new_str);
	}

	ret = asprintf(&new_str,"%s%d","firmata:rel_inv_",i);
	if(ret>0){
        	confi[i].invert = iniparser_getint(ini, new_str, -1);
		free(new_str);
	}

	ret = asprintf(&new_str,"%s%d","matrix:card",i);
	if(ret>0){
		confi[i].matrix = iniparser_getint(ini, new_str, -1);
		free(new_str);
	}

        ret = asprintf(&new_str,"mpd:mpd_port%d",i);
        if(ret>0){
                mpd_conf[i].port = iniparser_getint(ini, new_str, -1);
		if(mpd_conf[i].port != -1){
			num_mpd_instances++;
		}
                free(new_str);
        }

	for(w=0;w < num_cards;w++){
	        ret = asprintf(&new_str,"mpd:mpd_alsa%d-%d",i,w);
        	if(ret>0){
                	mpd_conf[i].outputs[w] = iniparser_getint(ini, new_str, -1);
			if(mpd_conf[i].outputs[w] != -1){
				mpd_conf[i].num_outputs++;
			}
                	free(new_str);
        	}
	}


	ret = asprintf(&new_str,"%s%d","alsa:device",i);
	 if(ret>0){
                s = iniparser_getstring(ini, new_str, NULL);
		if(s != NULL){
			confi[i].alsa_dev = strdup(s);
		}
                free(new_str);
        }

	 ret = asprintf(&new_str,"%s%d","shairport:shair",i);
         if(ret>0){
                s = iniparser_getstring(ini, new_str, NULL);
		if(s != NULL){
	                confi[i].shair_name = strdup(s);
		}
                free(new_str);
        }
    }

    	port = iniparser_getint(ini, "shairport:port_base",5000);
    	port_incr  = iniparser_getint(ini, "shairport:port_incr",10);
    	power_time = iniparser_getint(ini, "timing:power_time",0);
   	amp_time = iniparser_getint(ini, "timing:amp_time",0);
    	power_relais = iniparser_getint(ini, "firmata:power_relais",-1);
	led.green = iniparser_getint(ini, "firmata:ledg", -1);
	led.blue = iniparser_getint(ini, "firmata:ledb", -1);
    	led.red = iniparser_getint(ini, "firmata:ledr", -1);


	printf(C_TOPIC"ALSA: " C_DEF "Cards %i\n", num_cards);
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
	confi[i].relais_state = LOW;
        firmata_digitalWrite(firmata, confi[i].relais, confi[i].invert);
    }
    return 0;
}

void set_relais (int rel, int state){
	if (state > 0 && confi[rel].relais_state == LOW){
		confi[rel].relais_state = HIGH;
		if (confi[rel].invert == FALSE){
			firmata_digitalWrite(firmata, confi[rel].relais, HIGH);
		}else{
			firmata_digitalWrite(firmata, confi[rel].relais, LOW);
		}
	}else if(state == 0 && confi[rel].relais_state == HIGH){
		confi[rel].relais_state = LOW;
		if (confi[rel].invert == FALSE){
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
	char needle[10] = "Stop";
	char *new_str;
	int i;
	int card;
	int power = 0;

	for (card=0;card < num_cards;card++){
		if(asprintf(&new_str,"/proc/asound/card%i/stream0",card) != -1){
			stream = fopen(new_str,"r");
			if (stream != NULL){
				for (i=0;i<LINE_PLAY;i++){
					if(fgets(str, BYTE_READ, stream)!=NULL ){
						if(i==LINE_PLAY-1 && strstr(str, needle)){
							if(active_cards[card]){
							active_cards[card]--;
							}
						}else if(i==LINE_PLAY-1){
							active_cards[card] = amp_time;
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
	return power_count;
}

void write_mpd(void){
	#define MPD_TEMP_FIRST "/etc/mpd_first.temp"
	#define MPD_TEMP_FILE "/etc/mpd_alsa.temp"
	#define MPD_TEMP_LAST "/etc/mpd_last.temp"
	#define MPD_CONF "/etc/mpd.conf"
	FILE *stream;
	int card;
	stream = fopen(MPD_TEMP_FILE, "w+");
	if (stream != NULL){
		for(card=0;card < num_cards;card++){
			fprintf(stream, "audio_output {\n\ttype \"alsa\" \n\tname \"%s\" \n\tdevice \"%s\" \n\tmixer_type \"software\"\n}\n\n", confi[card].shair_name, confi[card].alsa_dev);
		}
		fclose(stream);
	}
	if(system("cat "MPD_TEMP_FIRST" "MPD_TEMP_FILE" "MPD_TEMP_LAST" > "MPD_CONF) != -1){
        printf(C_TOPIC "MPD: " C_DEF "Configuration for MPD written\n");
	}


}

void mpd_startup(void){
	printf(C_TOPIC"MPD: "C_DEF"Instances %i\n", num_mpd_instances);
	write_mpd();
	if(system("service mpd restart") != -1){
        	printf(C_TOPIC "MPD: " C_DEF "MPD restarted\n");
        }
}
