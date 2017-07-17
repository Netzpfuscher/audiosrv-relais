#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include "includes/firmata.h"
#include "includes/iniparser.h"
#include "includes/common.h"
#include "MQTTClient.h"
#include <ctype.h>

#define FALSE 0
#define TRUE  1
#define ERROR -1

#define MQTT_TIMEOUT 10000L
#define MQTT_QOS 1
#define MQTT_CLIENTID "Audioserver"
#define MQTT_SLOTS 16

MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_deliveryToken token;
volatile MQTTClient_deliveryToken deliveredtoken;
int flyingtokens[MQTT_SLOTS];
static char *message_buffer[MQTT_SLOTS];
static char *topic_buffer[MQTT_SLOTS];


int parse_ini_file(char * ini_name);
int init(void);
void show_help(void);

void set_relais(int rel, int state);
void led_blink(void);
int get_cards(void);
int get_active (void);
void write_mpd(void);
void write_mpd_file(void);
void write_asound(void);
static void device_list(void);
void delivered(void *context, MQTTClient_deliveryToken dt);
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connlost(void *context, char *cause);
void mqtt_send(char *message, char *topic);
char* copy_ini_string_i(dictionary * ini, char* ini_topic, int num);
char* copy_ini_string(dictionary * ini, char* ini_topic);
int copy_ini_int_i(dictionary * ini, char* ini_topic, int num);
int search_card(const char* card_name);

void mpd_startup(void);
static snd_pcm_stream_t alsastream = SND_PCM_STREAM_PLAYBACK;

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
	char* dac_room;
	FILE *shairport;
	char* dac_group;
	char* dac_name;
	char* dac_short;
	int alsa_num;
};

int num_mpd_instances;
struct mpd {
	int port;
	char* log_file;
	char* music_dir;
	char* playlist_dir;
	char* db_file;
	char* pid_file;
	char* state_file;
	char* sticker_file;
	FILE *instance;
};
struct led_state {
	int red;
	int green;
	int blue;
	int state;
};

struct mqtt {
	char* server;
	char* topic;
	int connected;
};

int mqtt_init(struct mqtt* conf);
void mqtt_subscribe(struct mqtt* conf);

int debug = FALSE;
char* serial_port;
char pbuffer[128];

int active_cards[NUM_REL];
struct ini confi[NUM_REL];
struct led_state led;
struct mpd mpd_conf[NUM_REL];
struct mqtt mqtt_conf;

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
   	MQTTClient_disconnect(client, 10000);
    	MQTTClient_destroy(&client);
	exit(0);
}


int main(int argc, char *argv[])
{


	if (signal(SIGINT, sig_handler) == SIG_ERR) printf("\ncan't catch SIGINT\n");

	if (argc < 2){
        	printf(C_TOPIC "Main: " C_DEF "No config specified... use default: /etc/relais.conf\n");
        	arg_ini = "/etc/relais.conf";
    	}else{
    		if(strcmp(argv[1] ,"-d") == 0){
			arg_ini =  "/etc/relais.conf";
			debug = TRUE;
			printf(C_TOPIC "Main: " C_DEF "Debug mode... use default config: /etc/relais.conf\n");
		}else{
			arg_ini = strdup(argv[1]);
		}
    	}
    	printf(C_TOPIC "Main: " C_DEF "Starting Audioserver Control...\n");

	if(init() == -1){
        	printf(C_TOPIC "Main: " C_DEF "Error... Exit\n\n");
        	return 0;
    	}

    	device_list();
	write_mpd_file();

	int   i = 0;
    	char *new_str;
	for(i=0;i<num_cards;i++){
		if(confi[i].alsa_dev != NULL && confi[i].dac_room != NULL){
        		if(asprintf(&new_str,"shairport-sync -p %i -a %s -o alsa -- -d %s",port,confi[i].dac_room,confi[i].alsa_dev) != ERROR){
				if(debug == FALSE){
					confi[i].shairport = popen(new_str, "r");
					usleep(100000);
				}else{
					printf(C_TOPIC "Shairport: " C_DEF "%s\n", new_str);
				}
				free(new_str);
				port = port + port_incr;
			}
		}
	}

    	for(i=0;i<NUM_REL;i++){
		active_cards[i] = FALSE;
    	}

	if(mqtt_init(&mqtt_conf)){
		printf(C_TOPIC "MQTT: " C_DEF "Sucessfully connected to server %s\n", mqtt_conf.server);
	}else{
		printf(C_TOPIC "MQTT: " C_DEF "Can't connect to server %s\n", mqtt_conf.server);
	}
	mqtt_subscribe(&mqtt_conf);

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
    int i;
    num_mpd_instances = 0;
    printf(C_TOPIC "Parse: " C_DEF "Read configuration: %s\n", ini_name);
    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        return ERROR ;
    }

    serial_port = copy_ini_string(ini, "firmata:port");
    mqtt_conf.server = copy_ini_string(ini, "mqtt:server");
    mqtt_conf.topic = copy_ini_string(ini, "mqtt:topic");

    for(i=0; i<NUM_REL; i++) {
	confi[i].relais = copy_ini_int_i(ini, "firmata:rel_port_%d",i);
	confi[i].invert = copy_ini_int_i(ini, "firmata:rel_inv_%d",i);
	confi[i].matrix = copy_ini_int_i(ini, "matrix:dac%d_relais",i);
	mpd_conf[i].port = copy_ini_int_i(ini, "mpd:port%d",i);
	if(mpd_conf[i].port != -1){
		num_mpd_instances++;
	}


	confi[i].dac_room = copy_ini_string_i(ini,"matrix:dac%d_room",i);
	confi[i].dac_name = copy_ini_string_i(ini,"matrix:dac%d_name",i);
	confi[i].dac_group = copy_ini_string_i(ini,"matrix:dac%d_group",i);
	mpd_conf[i].log_file = copy_ini_string_i(ini,"mpd:log_file%d",i);
	mpd_conf[i].music_dir = copy_ini_string_i(ini,"mpd:music_directory%d",i);
    	mpd_conf[i].playlist_dir = copy_ini_string_i(ini,"mpd:playlist_directory%d",i);
	mpd_conf[i].db_file = copy_ini_string_i(ini,"mpd:db_file%d",i);
	mpd_conf[i].pid_file = copy_ini_string_i(ini,"mpd:pid_file%d",i);
	mpd_conf[i].state_file = copy_ini_string_i(ini,"mpd:state_file%d",i);
	mpd_conf[i].sticker_file = copy_ini_string_i(ini,"mpd:sticker_file%d",i);

	}

    	port = iniparser_getint(ini, "shairport:port_base",5000);
    	port_incr  = iniparser_getint(ini, "shairport:port_incr",10);
    	power_time = iniparser_getint(ini, "timing:power_time",0);
   	amp_time = iniparser_getint(ini, "timing:amp_time",0);
    	power_relais = iniparser_getint(ini, "firmata:power_relais",-1);
	led.green = iniparser_getint(ini, "firmata:ledg", -1);
	led.blue = iniparser_getint(ini, "firmata:ledb", -1);
    	led.red = iniparser_getint(ini, "firmata:ledr", -1);

    	iniparser_freedict(ini);
    	return 0 ;
}

char* copy_ini_string_i(dictionary * ini, char* ini_topic, int num){
	char *new_str;
	const char  *   s;
	if(asprintf(&new_str,ini_topic,num) != ERROR){
                s = iniparser_getstring(ini, new_str, NULL);
		free(new_str);
                if (s != NULL){
                        return strdup(s);
                }else{
			return NULL;
		}
        }else{
		return NULL;
	}
}
int copy_ini_int_i(dictionary * ini, char* ini_topic, int num){
	char *new_str;
	int temp;
        if(asprintf(&new_str,ini_topic,num) != ERROR){
		temp = iniparser_getint(ini, new_str, -1);
		free(new_str);
		return temp;
        }else{
		return ERROR;
	}
}
char* copy_ini_string(dictionary * ini, char* ini_topic){
	const char * s;
    	s = iniparser_getstring(ini, ini_topic, NULL);
	if(s != NULL){
    		return strdup(s);
	}else{
		return NULL;
	}
}


int init(void){
    int i;
    if(parse_ini_file(arg_ini) ==  ERROR){
        return ERROR;
    }
    firmata = firmata_new(serial_port); //init Firmata
    if (firmata == NULL){
        return ERROR;
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
		if(asprintf(&new_str,"/proc/asound/%s/stream0",confi[card].dac_short) != -1){
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
			fprintf(stream, "audio_output {\n\ttype \"alsa\" \n\tname \"%s\" \n\tdevice \"%s\" \n\tmixer_type \"software\"\n}\n\n", confi[card].dac_room, confi[card].alsa_dev);
		}
		fclose(stream);
	}
	if(system("cat "MPD_TEMP_FIRST" "MPD_TEMP_FILE" "MPD_TEMP_LAST" > "MPD_CONF) != -1){
        printf(C_TOPIC "MPD: " C_DEF "Configuration for MPD written\n");
	}


}

void write_mpd_file(void){
        int card;
	int i;
	char *new_str;
	char *start_str;
        FILE *stream;
	for(i=0; i<num_mpd_instances; i++) {
		if(asprintf(&new_str,"test%d.conf",i) != ERROR){
	        	stream = fopen(new_str, "w+");
		        if (stream != NULL){

        		        fprintf(stream, "music_directory\t\t\"%s\"\n", mpd_conf[i].music_dir);
	                	fprintf(stream, "playlist_directory\t\t\"%s\"\n", mpd_conf[i].playlist_dir);
		                fprintf(stream, "db_file\t\t\"%s\"\n", mpd_conf[i].db_file);
        		        fprintf(stream, "log_file\t\t\"%s\"\n", mpd_conf[i].log_file);
	               		fprintf(stream, "pid_file\t\t\"%s\"\n", mpd_conf[i].pid_file);
		                fprintf(stream, "state_file\t\t\"%s\"\n", mpd_conf[i].state_file);
        		        fprintf(stream, "sticker_file\t\t\"%s\"\n", mpd_conf[i].sticker_file);
	                	fprintf(stream, "#user\t\t\"mpd\"\n");
	        	        fprintf(stream, "#group\t\t\"nogroup\"\n");
        	        	fprintf(stream, "bind_to_address\t\t\"any\"\n");
				fprintf(stream, "port\t\t\"%d\"\n",mpd_conf[i].port);
	        	        fprintf(stream, "input {\n\tplugin \"curl\"\n}\n");

        	        	for(card=0;card < num_cards;card++){
                	        	fprintf(stream, "audio_output {\n\ttype \"alsa\" \n\tname \"%s\" \n\tdevice \"%s\" \n\tmixer_type \"software\"\n}\n\n", confi[card].dac_room, confi[card].alsa_dev);
		                }

        		        fprintf(stream, "filesystem_charset\t\t\"UTF-8\"\n");
                		fprintf(stream, "id3v1_encoding\t\t\"UTF-8\"\n");

	                	fclose(stream);
				if(asprintf(&start_str,"mpd --no-daemon %s",new_str) != ERROR){
                                	if(debug == FALSE){
                                        	mpd_conf[i].instance = popen(start_str, "r");
                                        	usleep(100000);
                                	}else{
                                        	printf(C_TOPIC "MPD: " C_DEF "%s\n", start_str);
                                	}
                                	free(start_str);
				}

	        	}
			free(new_str);
		}
	}


}


void mpd_startup(void){
	printf(C_TOPIC"MPD: "C_DEF"Instances %i\n", num_mpd_instances);
	write_mpd();
	if(!debug){
		if(system("service mpd restart") != ERROR){
        		printf(C_TOPIC "MPD: " C_DEF "MPD restarted\n");
	        }
	}
}

static void device_list(void)
{
	snd_ctl_t *handle;
	int card, err, dev;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
	card = -1;
	if (snd_card_next(&card) < 0 || card < 0) {
		printf("no soundcards found...");
		return;
	}
        snd_pcm_stream_name(alsastream);
	while (card >= 0) {
		char name[32];
		sprintf(name, "hw:%d", card);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			printf("control open (%i): %s", card, snd_strerror(err));
			goto next_card;
		}
		if ((err = snd_ctl_card_info(handle, info)) < 0) {
			printf("control hardware info (%i): %s", card, snd_strerror(err));
			snd_ctl_close(handle);
			goto next_card;
		}
		dev = -1;
		while (1) {
			if (snd_ctl_pcm_next_device(handle, &dev)<0)
				printf("snd_ctl_pcm_next_device");
			if (dev < 0)
				break;
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, alsastream);
			if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
				if (err != -ENOENT)
					printf("control digital audio info (%i): %s", card, snd_strerror(err));
				continue;
			}
			//printf(C_TOPIC"ALSA:"C_DEF" card %i: %s [%s], device %i: %s [%s]\n",
			//	card, snd_ctl_card_info_get_id(info), snd_ctl_card_info_get_name(info),
			//	dev,
			//	snd_pcm_info_get_id(pcminfo),
			//	snd_pcm_info_get_name(pcminfo));

			int temp_num = search_card(snd_ctl_card_info_get_name(info));
			if(temp_num != ERROR){
				confi[temp_num].alsa_num = card;
				confi[temp_num].dac_short = strdup(snd_ctl_card_info_get_id(info));
				if(asprintf(&confi[temp_num].alsa_dev,"duplex_%i",card) == ERROR){
        	                        printf("Error allocating memory\n");
	                        }
				printf(C_TOPIC"ALSA:"C_DEF" dac: %s [%s] found at ALSA dev: %d\n",confi[temp_num].dac_name,confi[temp_num].dac_short,card);

			}else{
				printf(C_TOPIC"ALSA:"C_DEF" dac: %s not found\n",snd_ctl_card_info_get_name(info));
			}

			if (num_cards < card){
				num_cards = card;
			}
		}
		snd_ctl_close(handle);
	next_card:
		if (snd_card_next(&card) < 0) {
			printf("snd_card_next");
			break;
		}
	}
	num_cards++;
	printf(C_TOPIC"ALSA: " C_DEF "Cards %i\n", num_cards);

	write_asound();
}
int search_card(const char* card_name){
	for(int i=0; i<NUM_REL;i++){
		if(confi[i].dac_name != NULL){
			if(strcmp(card_name,confi[i].dac_name) == 0){
			return i;
			}
		}
	}
	return ERROR;
}



void write_asound(void){
        #define ASOUND_CONF "/etc/asound.conf"
        FILE *stream;
        int card;
        stream = fopen(ASOUND_CONF, "w+");
        if (stream != NULL){
                for(card=0;card < num_cards;card++){
			fprintf(stream, "#----------------------------------------------------------------------\n");
                        fprintf(stream, "pcm.room%i {\n\ttype hw\n\tcard %i\n\tdevice 0\n}\n",card , card);
			fprintf(stream, "ctl.room%i {\n\ttype hw\n\tcard %i\n\tdevice 0\n}\n",card , card);
			fprintf(stream, "pcm.dmixer_%i {\n\ttype dmix\n\tipc_key 1024\n\tipc_perm 0666\n\tslave.pcm \"room%i\"\n\tslave {\n\t\tperiod_time 0\n\t\tperiod_size 2048\n\t\tbuffer_size 8192\n\t\trate 44100\n\t\tchannels 2\n\t}\n\tbindings {\n\t\t0 0\n\t\t1 1\n\t}\n}\n",card, card);
			fprintf(stream, "pcm.dsnooper_%i {\n\ttype snoop\n\tipc_key 1024\n\tipc_perm 0666\n\tslave.pcm \"room%i\"\n\tslave {\n\t\tperiod_time 0\n\t\tperiod_size 2048\n\t\tbuffer_size 8192\n\t\trate 44100\n\t\tchannels 2\n\t}\n\tbindings {\n\t\t0 0\n\t\t1 1\n\t}\n}\n",card, card);
			fprintf(stream, "pcm.duplex_%i {\n\ttype asym\n\tplayback.pcm \"dmixer_%i\"\n\tcapture.pcm \"dsnooper_%i\"\n}\n", card, card, card);
                }
                fclose(stream);
        }
        printf(C_TOPIC "ALSA: " C_DEF "Configuration for ALSA written\n");
}

void delivered(void *context, MQTTClient_deliveryToken dt){
        for(int i=0;i<MQTT_SLOTS;i++){
                if(flyingtokens[i] == dt){
                        free(message_buffer[i]);
			free(topic_buffer[i]);
			flyingtokens[i] = -1;
                        break;
                }
        }


}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message){
    int i;
    char* payloadptr;


    printf(C_TOPIC "MQTT: " C_DEF "Message arrived at topic: %s", topicName);
    printf("   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause){
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int mqtt_init(struct mqtt* conf){
    int rc;
    for(int i=0;i<MQTT_SLOTS;i++){
	flyingtokens[i] = -1;
    }
    MQTTClient_create(&client, conf->server, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        return FALSE;
    }else{
	conf->connected = TRUE;
	return TRUE;
    }

}
void mqtt_subscribe(struct mqtt* conf){
	char *new_str;
	if (!conf->connected){
		return;
	}
        for(int i=0;i<num_cards;i++){
                if(confi[i].alsa_dev != NULL && confi[i].dac_room != NULL){
			if(asprintf(&new_str, "%s%s",conf->topic, confi[i].dac_room) != ERROR){
				for(int w = 0; new_str[w]; w++){
 					new_str[w] = tolower(new_str[w]);
				}
				MQTTClient_subscribe(client, new_str, MQTT_QOS);
				printf(C_TOPIC "MQTT: " C_DEF "Subscribe to: %s\n", new_str);
				free(new_str);
			}
                }
        }
	return;
}

void mqtt_send(char *message, char *topic){
	int token_num=-1;
	static MQTTClient_message pubmsg = MQTTClient_message_initializer;
	for(int i=0;i<MQTT_SLOTS;i++){
		if(flyingtokens[i] == -1){
			token_num = i;
			break;
		}
	}
	if(token_num != -1){
		message_buffer[token_num] = strdup(message);
		topic_buffer[token_num] = strdup(topic);
		pubmsg.payload = message_buffer[token_num];
		pubmsg.payloadlen = strlen(message_buffer[token_num]);
		pubmsg.qos = MQTT_QOS;
		pubmsg.retained = 0;
		MQTTClient_publishMessage(client, topic_buffer[token_num], &pubmsg, &flyingtokens[token_num]);
	}
}
