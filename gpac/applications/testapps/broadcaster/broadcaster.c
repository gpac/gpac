#include "broadcaster.h"
#include "debug.h"

static void printIncompatibleOptions()
{
	fprintf(stderr, "Options config file and tcp port are incompatible !\n");
}

extern GF_Err SampleCallBack(void *, u16, char *data, u32 size, u64 ts);

/**
 * Returns a port from a char value, will return 0 if port is not valid
 */
static unsigned short port_from_string(const char * port_to_parse)
{
	unsigned long int v;
	char * endptr = '\0';
	const char * value = port_to_parse;
	if (value == NULL || value[0] == '\0') {
		fprintf(stderr, "Value for port cannot be empty");
		return 0;
	}
	v = strtoul(value, &endptr, 10);
	if (*endptr != '\0' || v < 1 || v > 65535) {
		fprintf(stderr, "Value %s is not a valid port, port must be between 1 and 65535 !\n", value);
		return 0;
	}
	return (unsigned short) v;
}

static int command_line_parsing(int argc, const char** argv, unsigned short * tcp_port,
                                char *config_file, int *config_flag, unsigned short * mtu_size,
                                int * debug, u32 * socketType_for_updates)
{
	int counter = 1;
	if (argc < 2 || argc%2 != 1) {
		fprintf(stderr, "Incorrect number of arguments, must be multiple of 2 (Please specify at least -f or -p arguments) !\n");
		return -5;
	}

	for(counter = 1; counter < (argc - 1); counter+=2)
	{
		const char * a = argv[counter];
		if (!strcmp("-p", a) || !strcmp("--port", a))
		{
			if (*config_flag)
			{
				printIncompatibleOptions();
				return -2;
			}
			(*tcp_port) = port_from_string( argv[counter + 1] );
			if (!(*tcp_port)) return -3;
		}
		else if (!strcmp("-f", a) || !strcmp("--file", a))
		{
			if (*tcp_port) {
				printIncompatibleOptions();
				return -2;
			}
			strcpy(config_file, argv[counter+1]);
			(*config_flag) = 1;
		}
		else if (!strcmp("-m", a) || !strcmp("--mtu", a))
		{
			if (mtu_size) {
				*mtu_size = atoi(argv[counter+1]);
				if (! (*mtu_size)) return -3;
			}
		}
		else if (!strcmp("-d", a) || !strcmp("--debug", a))
		{
			*debug = atoi(argv[counter+1]);
		}
		else if (!strcmp("-s", a) || !strcmp("--socket-type-for-updates", a))
		{
			*socketType_for_updates = 0 == stricmp("TCP", argv[counter+1]);
		}
		else
		{
			fprintf(stderr, "Unknown parameter %s.", a);
			return -2;
		}
	}

	if (!(*config_flag) && !(*tcp_port)) {
		fprintf(stderr, "No config file or port specified !\n");
		return -6;
	}

	return 0;
}

void print_usage(void)
{
	fprintf(stdout, "BIFS Scene encoder and streamer (c) Telecom ParisTech 2009\n");
	fprintf(stdout, "USAGE: broadcaster [-p tcp_port] [-s TCP|UDP] [-f config_file_name] [-m mtu_size] -d [debug]\n");
	fprintf(stdout, "\tIndicate the location of the configuration file either with a TCP port number or a file name\n");
	fprintf(stdout, "\tmtu_size : the MTU size (default = 1492)\n");
	fprintf(stdout, "\t-s or --socket-type-for-updates : connection type for updates (UDP by default)\n");
	fprintf(stdout, "\tdebug: OR debug mask (broadcaster = 1, scene_generator=2, sdp_generator=4, ALL=31)\n");
}

u32 RAP_send(void *par)
{
	RAP_Input *input = par;
	PNC_CallbackData *data = input->data;

	input->status = 1;
	while (input->status==1) {
		u32 *timer;
		gf_mx_p(input->carrousel_mutex);

		timer = input->RAPtimer;
		data->RAPsent++;
		dprintf(DEBUG_broadcaster, "Sending RAP, will sleep for %d seconds\n", *timer);
		data->RAP = 1;
		gf_seng_aggregate_context(data->codec, 0);
		gf_seng_encode_context(data->codec, SampleCallBack);

		gf_mx_v(input->carrousel_mutex);
		gf_sleep((*timer)*1000);
	}
	input->status = 2;
	return GF_OK;
}

GF_Err parse_config(GF_Config *gf_config_file, CONF_Data *conf, int debug)
{
	conf->scene_init_file = gf_cfg_get_key(gf_config_file, MAIN_SECTION, SCENE_INIT);
	if (!conf->scene_init_file) {
		fprintf(stderr, "Cannot find initial scene from configuration file\n");
		return GF_IO_ERR;
	} else {
		dprintf(DEBUG_broadcaster, "Using initial scene: %s\n", conf->scene_init_file);
	}

	conf->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
	if (!conf->rap_timer) conf->rap_timer = "2";
	dprintf(DEBUG_broadcaster, "Using a RAP period of %s seconds\n", conf->rap_timer);

	conf->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
	if (!conf->config_input_port) conf->config_input_port = "5000";
	dprintf(DEBUG_broadcaster, "Using Configuration Port: %s\n", conf->config_input_port);

	conf->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
	if (!conf->modif_input_port)
		conf->modif_input_port = "8000";
	dprintf(DEBUG_broadcaster, "Using Update Port: %s\n", conf->modif_input_port);

	conf->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
	if (!conf->dest_ip)
		conf->dest_ip = "127.0.0.1";
	conf->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
	if (!conf->dest_port)
		conf->dest_port = "7000";
	dprintf(DEBUG_broadcaster, "Destination: %s:%s\n", conf->dest_ip, conf->dest_port);

	conf->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
	if (!conf->feedback_ip) conf->feedback_ip = "127.0.0.1";
	conf->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
	if (!conf->feedback_port) conf->feedback_port = "5757";
	dprintf(DEBUG_broadcaster, "Feedback host: %s:%s\n", conf->feedback_ip, conf->feedback_port);
	return GF_OK;
}

u32 tcp_server(void *par)
{
	TCP_Input *input = par;
	u32 *timer = input->RAPtimer;
	char buffer[MAX_BUF];
	unsigned char temp[MAX_BUF];
	FILE *fp;
	u32 byte_read;
	GF_Config *gf_config_file;
	GF_Socket *TCP_socket;
	GF_Socket *conn_socket;
	GF_Err e;

	int debug = input->debug;
	input->status = 1;

	TCP_socket = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(TCP_socket, NULL, input->port, NULL, 0, 0);
	e = gf_sk_listen(TCP_socket, 1);
	e = gf_sk_set_block_mode(TCP_socket, 1);
	e = gf_sk_server_mode(TCP_socket, 0);

	while(input->status == 1)
	{
		int ret;
		memset(buffer, 0, sizeof(buffer));
		e = gf_sk_accept(TCP_socket, &conn_socket);
		if (e == GF_OK) {
			memset(buffer, 0, sizeof(buffer));
			e = gf_sk_receive(conn_socket, buffer, MAX_BUF, 0, &byte_read);
		}

		switch (e) {
		case GF_IP_NETWORK_EMPTY:
			gf_sleep(33);
			continue;
		case GF_OK:
			break;
		default:
			fprintf(stderr, "Error with TCP socket : %s\n", gf_error_to_string(e));
			exit(1);
			break;
		}

		if((*(input->config_flag)) == 0)
		{
			u32 num_retry;
			fp = gf_fopen("temp.cfg", "w+");
			if (!fp) {
				fprintf(stderr, "Error opening temp file for the configuration\n");
				exit(1);
			}
			ret = gf_fwrite(buffer, 1, byte_read, fp);
			gf_fclose(fp);

			/* parsing config info */
			gf_config_file = gf_cfg_new(".", "temp.cfg");
			if (!gf_config_file) {
				fprintf(stderr, "Error opening the config file %s\n", gf_error_to_string(e));
				exit(-1);
			}
			parse_config(gf_config_file, input->config, debug);

			/* Acknowledging the configuration */
			gf_sk_send(conn_socket, "OK\n", 3);

			memset(temp, 0, sizeof(temp));
			fp = gf_fopen(input->config->scene_init_file, "w+");
			if (!fp) {
				fprintf(stderr, "Error opening temp file for reception of the initial scene\n");
				exit(1);
			}
			num_retry=10;

			while (1)
			{
				e = gf_sk_receive(conn_socket, temp, sizeof(temp), 0, &byte_read);

				if (e == GF_OK) {
					gf_fwrite(temp, 1, byte_read, fp);
				} else if (e==GF_IP_NETWORK_EMPTY) {
					num_retry--;
					if (!num_retry)
						break;
					gf_sleep(1);
				} else {
					fprintf(stderr, "Error receiving initial scene: %s\n", gf_error_to_string(e));
					break;
				}
			}
			gf_fclose(fp);
			*(input->config_flag) = 1;
		}
		/* we only wait now for the config updates */
		if ( (*(input->config_flag)) == 1) {
			ret = sscanf(buffer, "DelaiMax=%d\n", timer);
			fprintf(stdout, "RAP timer changed, now : %d\n", *timer);
		}
		gf_sk_del(conn_socket);
	}

	input->status = 2;
	return GF_OK;
}

u8 get_a_char();
Bool has_input();

int main (const int argc, const char** argv)
{
	GF_Err e;
	Bool run;

	/* location of the configuration file: 0 wait for config on a socket, 1 use the given file */
	u32 config_flag;
	char config_file_name[MAX_BUF];

	int dest_port;
	unsigned short tcp_port = 0;
	/* Should be fine on WIFI network */
	unsigned short mtu_size = 1492;
	int debug = 0;
	TCP_Input *tcp_conf = NULL;
	GF_Thread *tcp_thread;
	GF_Err th_err_tcp;

	GF_Err th_err_rap;
	RAP_Input *rap_conf;
	GF_Thread *rap_thread;

	CONF_Data *conf;
	GF_Config *gf_config_file;
	GF_Err res;

	GF_Socket *UDP_feedback_socket;
	u32 socketType_for_updates;

	PNC_CallbackData * data;
	GF_RTPChannel * chan;
	GF_RTPHeader hdr;
	u32 timer = -1;

	GF_Mutex *carrousel_mutex;
	char sdp_fmt[5000];
	tcp_thread = NULL;

	/* init gpac lib */
	gf_sys_init(GF_MemTrackerNone);

	GF_SAFEALLOC(conf, CONF_Data);

	tcp_port = config_flag = 0;
	socketType_for_updates = GF_SOCK_TYPE_UDP;
	if (command_line_parsing(argc, argv, &tcp_port, config_file_name, (int *) &config_flag, &mtu_size, &debug, &socketType_for_updates)) {
		print_usage();
		return -1;
	}
	setDebugMode( debug );
	gf_config_file = NULL;
	if (config_flag == 1)
	{
		char *cfg_path;
		char *cfg_fname;
		char *tmp;

		cfg_fname = config_file_name;
		cfg_path = config_file_name;
		tmp = strrchr(cfg_fname, GF_PATH_SEPARATOR);
		if (tmp) {
			cfg_fname = tmp+1;
			tmp[0] = 0;
		} else {
			cfg_path = ".";
		}
		gf_config_file = gf_cfg_new(cfg_path, cfg_fname);
		if (!gf_config_file) {
			fprintf(stderr, "Cannot open config file %s\n", config_file_name);
			return -1;
		} else {
			dprintf(DEBUG_broadcaster, "Using config file %s.\n", config_file_name);
		}
		if (parse_config(gf_config_file, conf, debug)) return -1;
		tcp_port = atoi(conf->config_input_port);
	}
	else
	{
		GF_SAFEALLOC(tcp_conf, TCP_Input);
		tcp_conf->config_flag = &config_flag;
		tcp_conf->RAPtimer = &timer;
		tcp_conf->port = tcp_port;
		tcp_conf->config = conf;
		tcp_thread = gf_th_new("TCPInterface");

		/* Starting the thread which will write the received config in a temporary file */
		th_err_tcp = gf_th_run(tcp_thread, tcp_server, tcp_conf);

		fprintf(stdout, "Waiting for configuration on port %d...\n", tcp_conf->port);

		while(config_flag == 0) {
			gf_sleep(1000);
		}
		fprintf(stdout, "Configuration File received. Starting Streaming ...\n");
	}

	timer = atoi(conf->rap_timer);
	dest_port = atoi(conf->dest_port);
	res = PNC_InitRTP(&chan, (char *)conf->dest_ip, dest_port, mtu_size);
	if (res != 0) {
		fprintf(stderr, "Cannot initialize RTP output (error: %d)\n", res);
		exit(1);
	}

	carrousel_mutex = gf_mx_new("Carrousel");
	data = PNC_Init_SceneGenerator(chan, &hdr, (char *) conf->scene_init_file,
	                               socketType_for_updates, (u16) atoi(conf->modif_input_port), debug);
	if (!data) {
		fprintf(stderr, "Cannot initialize Scene Generator\n");
		exit(1);
	}
	data->carrousel_mutex = carrousel_mutex;
	data->RAPsent = 1;

	UDP_feedback_socket = gf_sk_new(GF_SOCK_TYPE_UDP);
	e = gf_sk_bind(UDP_feedback_socket, NULL, (u16)atoi(conf->feedback_port), (char*)conf->feedback_ip, (u16)atoi(conf->feedback_port), 0);
	if (e) {
		fprintf(stderr, "Cannot bind socket for bitrate feedback information (%s)\n", gf_error_to_string(e));
	} else {
		e = gf_sk_set_block_mode(UDP_feedback_socket, 1);
		if (e) {
			fprintf(stderr, "Cannot set feedback socket block mode (%s)\n", gf_error_to_string(e));
		}
	}
	data->feedback_socket = UDP_feedback_socket;

	PNC_InitPacketiser(data, sdp_fmt, mtu_size);
	PNC_SendInitScene(data);

	GF_SAFEALLOC(rap_conf, RAP_Input);
	rap_conf->RAPtimer = &timer;
	rap_conf->carrousel_mutex = carrousel_mutex;
	rap_conf->data = data;
	rap_thread = gf_th_new("RAPGenerator");
	th_err_rap = gf_th_run(rap_thread, RAP_send, rap_conf);

	sdp_generator(data, (char *)conf->dest_ip, sdp_fmt);

	run = 1;
	while (run)
	{
		e = PNC_processBIFSGenerator(data);
		if (e) {
			fprintf(stderr, "Cannot Process BIFS data (%s)\n", gf_error_to_string(e));
			break;
		}

		if (has_input()) {
			char c = get_a_char();
			switch (c) {
			case 'q':
				run = 0;
				break;
			}
		}
		gf_sleep(10);
	}

	/* waiting for termination of the RAP thread */
	rap_conf->status = 0;
	while (rap_conf->status != 2)
		gf_sleep(0);
	gf_free(rap_conf);
	gf_th_del(rap_thread);

	/* waiting for termination of the TCP listening thread */
	if (tcp_conf) {
		tcp_conf->status = 0;
		while (tcp_conf->status != 2)
			gf_sleep(0);
		gf_free(tcp_conf);
		gf_th_del(tcp_thread);
	}

	PNC_Close_SceneGenerator(data);

	gf_free(conf);

	if (gf_config_file)
		gf_cfg_del(gf_config_file);

	gf_mx_del(carrousel_mutex);
	gf_sys_close();
	return 0;
}

#ifdef WIN32
#include <conio.h>
#include <windows.h>
Bool has_input()
{
	return kbhit();
}
u8 get_a_char()
{
	return getchar();
}
void set_echo_off(Bool echo_off)
{
	DWORD flags;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hStdin, &flags);
	if (echo_off) flags &= ~ENABLE_ECHO_INPUT;
	else flags |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hStdin, flags);
}
#else
/*linux kbhit/getchar- borrowed on debian mailing lists, (author Mike Brownlow)*/
#include <termios.h>

static struct termios t_orig, t_new;
static s32 ch_peek = -1;

void init_keyboard()
{
	tcgetattr(0, &t_orig);
	t_new = t_orig;
	t_new.c_lflag &= ~ICANON;
	t_new.c_lflag &= ~ECHO;
	t_new.c_lflag &= ~ISIG;
	t_new.c_cc[VMIN] = 1;
	t_new.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t_new);
}
void close_keyboard(Bool new_line)
{
	tcsetattr(0,TCSANOW, &t_orig);
	if (new_line) fprintf(stdout, "\n");
}

void set_echo_off(Bool echo_off)
{
	init_keyboard();
	if (echo_off) t_orig.c_lflag &= ~ECHO;
	else t_orig.c_lflag |= ECHO;
	close_keyboard(0);
}

Bool has_input()
{
	u8 ch;
	s32 nread;

	init_keyboard();
	if (ch_peek != -1) return 1;
	t_new.c_cc[VMIN]=0;
	tcsetattr(0, TCSANOW, &t_new);
	nread = read(0, &ch, 1);
	t_new.c_cc[VMIN]=1;
	tcsetattr(0, TCSANOW, &t_new);
	if(nread == 1) {
		ch_peek = ch;
		return 1;
	}
	close_keyboard(0);
	return 0;
}

u8 get_a_char()
{
	u8 ch;
	if (ch_peek != -1) {
		ch = ch_peek;
		ch_peek = -1;
		close_keyboard(1);
		return ch;
	}
	int v = read(0,&ch,1);
	close_keyboard(1);
	if (v == 0)
		return 0;
	return ch;
}

#endif
