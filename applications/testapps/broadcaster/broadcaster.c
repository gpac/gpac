#include "broadcaster.h"


extern GF_Err SampleCallBack(void *calling_object, char *data, u32 size, u64 ts);

void command_line_parsing(int* argc, char** argv, int* tcp_port, char *config_file, int *config_flag)
{
	int argument, counter;
	char value[MAX_BUF];
	
	if ((*argc) == 3) {
	    argument=-1;
    	for(counter = 0; counter < ((*argc) - 2); counter = counter+2) {	
		    argument = server_command_line(argv[counter+1], argv[counter+2], value, argument);
		    if (argument == 0)
				(*tcp_port) = atoi(value);
			if (argument == 2) {
				strcpy(config_file, value);
				(*config_flag) = 1;
			}
    	}
	} else {
		print_usage();
		exit(0);
    }	
}

int server_command_line(char *arg_a, char *arg_b, char *value, int argument)
{
	char flag;
	sscanf(arg_a, "-%c", &flag);
	strcpy(value, arg_b);
	switch (flag)
	{
		case 'p':
			argument = 0;
			break;
		case 'f':
			argument = 2;
			break;
		default:
			print_usage();
			argument = -1;
			break;
	}
	return argument;
}

void print_usage(void)
{
	fprintf(stdout, "BIFS Scene encoder and streamer (c) Telecom ParisTech 2009\n");
	fprintf(stdout, "usage : broadcaster [-p tcp_port] [-f config_file_name]\n");
	fprintf(stdout, "Indicate the location of the configuration file either with a TCP port number or a file name\n");
}

u32 RAP_send(void *par)
{
	RAP_Input *input = par;
	PNC_CallbackData *data = input->data;
	u32 *timer;
	
	input->status = 1;
	while(input->status==1) {
		gf_mx_p(input->carrousel_mutex);

		timer = input->RAPtimer;
		data->RAPsent++;
		fprintf(stdout, "Sending RAP\n");
		data->RAP = 1; 
		gf_beng_aggregate_context(data->codec);
		gf_beng_encode_context(data->codec, SampleCallBack);

		gf_mx_v(input->carrousel_mutex);
		gf_sleep((*timer)*1000);
	}
	input->status = 2;
	return GF_OK;
}

GF_Err parse_config(GF_Config *gf_config_file, CONF_Data *conf) 
{
	conf->scene_init_file = gf_cfg_get_key(gf_config_file, MAIN_SECTION, SCENE_INIT);
	if (!conf->scene_init_file) {
		fprintf(stderr, "Cannot find initial scene from configuration file\n");
		return GF_IO_ERR;
	} else {
		fprintf(stdout, "Using initial scene: %s\n", conf->scene_init_file);
	}

	conf->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
	if (!conf->rap_timer) conf->rap_timer = "2";
	fprintf(stdout, "Using a RAP period of %s seconds\n", conf->rap_timer);

	conf->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
	if (!conf->config_input_port) conf->config_input_port = "5000";
	fprintf(stdout, "Using Configuration Port: %s\n", conf->config_input_port);

	conf->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
	if (!conf->modif_input_port) conf->modif_input_port = "8000";
	fprintf(stdout, "Using Update Port: %s\n", conf->modif_input_port);

	conf->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
	if (!conf->dest_ip) conf->dest_ip = "127.0.0.1";		
	conf->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
	if (!conf->dest_port) conf->dest_port = "7000";
	fprintf(stdout, "Destination: %s:%s\n", conf->dest_ip, conf->dest_port);

	conf->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
	if (!conf->feedback_ip) conf->feedback_ip = "127.0.0.1";		
	conf->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
	if (!conf->feedback_port) conf->feedback_port = "5757";
	fprintf(stdout, "Feedback host: %s:%s\n", conf->feedback_ip, conf->feedback_port);
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
	int ret;
	GF_Config *gf_config_file;
	GF_Socket *TCP_socket;
	GF_Socket *conn_socket;
	GF_Err e;
	
	input->status = 1;

	TCP_socket = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(TCP_socket, NULL, input->port, NULL, 0, 0);
	e = gf_sk_listen(TCP_socket, 1);
	e = gf_sk_set_block_mode(TCP_socket, 0);
	e = gf_sk_server_mode(TCP_socket, 0);
	
	while(input->status == 1) {	
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

		if((*(input->config_flag)) == 0) {

			u32 num_retry;
			fp = fopen("temp.cfg", "w+");
			if (!fp) {
				fprintf(stderr, "Error opening temp file for the configuration\n");
				exit(1);
			}
			ret = fwrite(buffer, 1, byte_read, fp);
			fclose(fp);
			
			/* parsing config info */
			gf_config_file = gf_cfg_new(".", "temp.cfg");
			if (!gf_config_file) {
				fprintf(stderr, "Error opening the config file %s\n", gf_error_to_string(e));
				exit(-1);
			}
			parse_config(gf_config_file, input->config);

			/* Acknowledging the configuration */ 
			gf_sk_send(conn_socket, "OK\n", 3);

			memset(temp, 0, sizeof(temp));
			fp = fopen(input->config->scene_init_file, "w+");
			if (!fp) {
				fprintf(stderr, "Error opening temp file for reception of the initial scene\n");
				exit(1);
			}
			num_retry=10;

			while (1) {

				GF_Err e = gf_sk_receive(conn_socket, temp, sizeof(temp), 0, &byte_read);

				if (e == GF_OK) {
					fwrite(temp, 1, byte_read, fp);
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
			fclose(fp);
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

int main (int argc, char** argv)
{
	GF_Err e;
	Bool run;

	/* location of the configuration file: 0 wait for config on a socket, 1 use the given file */
	u32 config_flag;	
	char config_file_name[MAX_BUF];

	int tcp_port, dest_port; 
	TCP_Input *tcp_conf;
	GF_Thread *tcp_thread;
	GF_Err th_err_tcp;

	GF_Err th_err_rap;
	RAP_Input *rap_conf;
	GF_Thread *rap_thread;

	CONF_Data *conf;	
	GF_Config *gf_config_file;
	GF_Err res;
	
	GF_Socket *UDP_feedback_socket;
	
	PNC_CallbackData * data;
	GF_RTPChannel * chan;
  	GF_RTPHeader hdr;
	u32 timer;
	
	GF_Mutex *carrousel_mutex;	
	char sdp_fmt[5000];
	
	/* init gpac lib */
	gf_sys_init();
	gf_log_set_level(GF_LOG_ERROR);
	gf_log_set_tools(GF_LOG_NETWORK|GF_LOG_RTP|GF_LOG_SCENE|GF_LOG_PARSER|GF_LOG_AUTHOR|GF_LOG_CODING|GF_LOG_SCRIPT);
	
	GF_SAFEALLOC(conf, CONF_Data);
		
	tcp_port = config_flag = 0;
	command_line_parsing(&argc, argv, &tcp_port, config_file_name, (int *) &config_flag);
	
	gf_config_file = NULL;
	if (config_flag == 1) {
		
		gf_config_file = gf_cfg_new(NULL, config_file_name);
		if (!gf_config_file) {
			fprintf(stderr, "Cannot open config file %s\n", config_file_name);
			return -1;
		} else {
			fprintf(stdout, "Using config file %s\n", config_file_name);
		}
		if (parse_config(gf_config_file, conf)) return -1;
		tcp_port = atoi(conf->config_input_port);
	} else {
		GF_SAFEALLOC(tcp_conf, TCP_Input);
		tcp_conf->config_flag = &config_flag;
		tcp_conf->RAPtimer = &timer;
		tcp_conf->port = tcp_port;
		tcp_conf->config = conf;
		tcp_thread = gf_th_new("TCPInterface");

		/* Starting the thread which will write the received config in a temporary file */
		th_err_tcp = gf_th_run(tcp_thread, tcp_server, tcp_conf);
		
		fprintf(stdout, "Waiting for configuration on port %s...\n", conf->config_input_port);

		while(config_flag == 0) { 
			gf_sleep(1000); 
		}
		fprintf(stdout, "Configuration File received. Starting Streaming ...\n");
	}
	
	timer = atoi(conf->rap_timer);
	dest_port = atoi(conf->dest_port);
  	res = PNC_InitRTP(&chan, (char *)conf->dest_ip, dest_port);
 	if (res != 0) {
		fprintf(stderr, "Cannot initialize RTP output (error: %d)\n", res); 
		exit(1);
	} 
  
	carrousel_mutex = gf_mx_new("Carrousel");
  	data = PNC_Init_SceneGenerator(chan, &hdr, (char *) conf->scene_init_file, (u16) atoi(conf->modif_input_port)); 
	if (!data) {
		fprintf(stderr, "Cannot initialize Scene Generator\n"); 
		exit(1);
	}
	data->carrousel_mutex = carrousel_mutex;
	data->RAPsent = 1;
	
	UDP_feedback_socket = gf_sk_new(GF_SOCK_TYPE_UDP);
	e = gf_sk_bind(UDP_feedback_socket, NULL, (u16) atoi(conf->feedback_port),  (char *) conf->feedback_ip, (u16) atoi(conf->feedback_port), 0);
	if (e) {
		fprintf(stderr, "Cannot bind socket for bitrate feedback information (%s)\n", gf_error_to_string(e));
	} else {
		e = gf_sk_set_block_mode(UDP_feedback_socket, 0);
		if (e) {
			fprintf(stderr, "Cannot set feedback socket block mode (%s)\n", gf_error_to_string(e));
		}
	}
	data->feedback_socket = UDP_feedback_socket;

  	PNC_InitPacketiser(data, sdp_fmt); 
  	PNC_SendInitScene(data);

	GF_SAFEALLOC(rap_conf, RAP_Input);
	rap_conf->RAPtimer = &timer;
	rap_conf->carrousel_mutex = carrousel_mutex;
	rap_conf->data = data;
	rap_thread = gf_th_new("RAPGenerator");
	th_err_rap = gf_th_run(rap_thread, RAP_send, rap_conf);

	sdp_generator(data, (char *)conf->dest_ip, sdp_fmt);
	
	run = 1;
	while (run) {
		GF_Err e = PNC_processBIFSGenerator(data); 
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

	/* waiting for termination of the TCP listening thread */
	tcp_conf->status = 0;
	while (tcp_conf->status != 2)
		gf_sleep(0);

	PNC_Close_SceneGenerator(data);
	
	free(tcp_conf);
	free(rap_conf);
	free(conf);
	
	if (gf_config_file)
		gf_cfg_del(gf_config_file);
	gf_delete_file("temp.cfg");

	gf_th_del(tcp_thread);
	gf_th_del(rap_thread);

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
	read(0,&ch,1);
	close_keyboard(1);
	return ch;
}

#endif

