/* includes default */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* includes pour gpac library*/
#include <gpac/bifsengine.h>
#include <gpac/config_file.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/ietf.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/scene_manager.h>
#include <gpac/bifs.h>

/* include pour carrousel BIFS */
/* pour les envois RTP */
#include "RTP_serv_sender.h"
/* pour la mise en paquets */
#include "RTP_serv_packetizer.h"
/* le module applicatif */
#include "RTP_serv_generator.h"

/* include pour SDP generation */
#include "sdp_generator.h"

/* definitions */
#define MAX_BUF 4096

/* données de configuration */
#define MAIN_SECTION "Broadcaster"
#define SCENE_INIT "InitialScene"
#define RAP_TIMER "RAPPeriod"
#define PORT_CONFIG "ConfigPort"
#define PORT_MODIF "SceneUpdatePort"

#define DEST_SECTION "Destination"
#define DEST_ADDRESS "IP"
#define PORT_OUTPUT "Port"

#define FEEDBACK_SECTION "Feedback"
#define IP_FEEDBACK "IP"
#define PORT_FEEDBACK "Port"

/* structure pour les données de configuration du serveur */
typedef struct config_data
{
	const char *rap_timer;
	const char *scene_init_file;
	const char *modif_input_port;
	const char *config_input_port;

	const char *feedback_ip;
	const char *feedback_port;

	const char *dest_ip;
	const char *dest_port;
} CONF_Data;

typedef struct tcp_input
{
	u16 port;	// port sur laquelle ouvrir le serveur
	u32 *config_flag;	// pour savoir si le serveur tcp doit attendre donnees de configuration
	// GF_Socket *socket;	// socket tcp pour l'interface
	u32 *RAPtimer;
	CONF_Data *config;
	u32 status;
} TCP_Input;

typedef struct rap_input
{
	GF_Mutex *carrousel_mutex;
	u32 *RAPtimer;
	PNC_CallbackData *data;
	u32 status;
} RAP_Input;
	
void command_line_parsing(int* argc, char** argv, int *tcp_port, char *config_file, int *config_flag);
int server_command_line(char *arg_a, char *arg_b, char *value, int argument);
u32 tcp_server(void *par);
u32 RAP_send(void *par);
void print_usage(void);
