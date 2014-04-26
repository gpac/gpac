/* includes default */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* includes for gpac library */
#include <gpac/scene_engine.h>
#include <gpac/config_file.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/ietf.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/base_coding.h>
#include <gpac/constants.h>
#include <gpac/scene_manager.h>
#include <gpac/bifs.h>

/* includes for BIFS carousel */
/* RTP sends */
#include "RTP_serv_sender.h"
/* packetization */
#include "RTP_serv_packetizer.h"
/* applicative module */
#include "RTP_serv_generator.h"

/* for SDP generation */
#include "sdp_generator.h"

/* definitions */
#define MAX_BUF 4096

/* configuration data*/
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

/* data struct on the server side */
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
	u16 port;	// server port
	u32 *config_flag;	// indicates whether the tcp server waits for configuration data
	// GF_Socket *socket;	// socket tcp for the GUI interface
	u32 *RAPtimer;
	CONF_Data *config;
	u32 status;
	int debug;
} TCP_Input;

typedef struct rap_input
{
	GF_Mutex *carrousel_mutex;
	u32 *RAPtimer;
	PNC_CallbackData *data;
	u32 status;
} RAP_Input;

/*void command_line_parsing(int* argc, const char** argv, int *tcp_port, const char *config_file, int *config_flag);
int server_command_line(char *arg_a, char *arg_b, char *value, int argument);*/
u32 tcp_server(void *par);
u32 RAP_send(void *par);
void print_usage(void);
