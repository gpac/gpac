#include "broadcaster.h"


extern GF_Err SampleCallBack(void *calling_object, char *data, u32 size, u64 ts);

/* fonction de gestion de la ligne de commande */
void command_line_parsing(int* argc, char** argv, int* tcp_port, char *config_file, int *config_flag)
{
	int argument, counter;
	char value[MAX_BUF];
	
    // cas de tous parametres necessaires
	if ((*argc) == 3)
	{
		/* parsing des parametres de la ligne de commande */
	    argument=-1;
    	for(counter = 0; counter < ((*argc) - 2); counter = counter+2)
	    {	
		    argument = server_command_line(argv[counter+1], argv[counter+2], value, argument);
		    if (argument == 0)	(*tcp_port) = atoi(value);
		    // if (argument == 1)	(*udp_port) =  atoi(value);
			if (argument == 2)	
			{
				strcpy(config_file, value);
				(*config_flag) = 1;
			}
    	}
	}	
	else
    {
		print_usage();
		exit(0);
    }	
}

/* fonction pour la gestion de base de la ligne de commande */
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
		/*
		case 'u':
			argument = 1;
			break;
		*/
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

/* gestion usage */
void print_usage(void)
{
	fprintf(stdout, "[broadcaster] usage : ./broadcaster [-p tcp_port] [-f fichier_config]\n");
	fprintf(stdout, "[broadcaster] usage : il faut specifier un fichier de configuration ou un port TCP pour l'interface GUI\n");
}

/*  fonction pour envoyer les messages RAP */
u32 RAP_send(void *par)
{
	RAP_Input *input = par;
	PNC_CallbackData *data = input->data;
	u32 *timer;
	
	input->status = 1;
	while(input->status==1) {
		// mutex avec le thread qui envoie les RAP pour l'envoi avec carrousel
		while(gf_mx_try_lock(input->carrousel_mutex) == 0)
		{ 
			gf_sleep(1); 
		}
		// locking
		// gf_mx_p(input->carrousel_mutex);
		// ici il faut integrer la fonction du carrousel pour envoyer RAP
		/* envoi de RAP */
		timer = input->RAPtimer;
		data->RAPsent++;
		fprintf(stdout, "[broadcaster] : RAP %d seconds\n", *(input->RAPtimer)); // *timer);
		data->RAP=1; //On demande que RAP soit positionné dans le SL
		fprintf(stdout, "[gpaclib] : ");
		gf_beng_aggregate_context(data->codec);
		gf_beng_encode_context(data->codec, SampleCallBack);
		// unlocking
		gf_mx_v(input->carrousel_mutex);
		// gf_sleep(input->RAPtimer);
		gf_sleep(*timer*1000);
	}
	input->status = 2;
	return GF_OK;
}

/* gestion tcp pour interface gui */
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
	      		fprintf(stdout, "[broadcaster] : Error with TCP socket : %d\n", e);
	      		exit(1);
	      		break;
		}

		if((*(input->config_flag)) == 0) {

			u32 num_retry;

			/* waiting for the configuration info */
			fp = fopen("temp.cfg", "w+");
			if (!fp) {
				fprintf(stdout, "[broadcaster] : Error opening temp file for the configuration\n");
				exit(1);
			}
			ret = fwrite(buffer, 1, byte_read, fp);
			fclose(fp);
			
			/* parsing config info */
			gf_config_file = gf_cfg_new(".", "temp.cfg");
			input->config->scene_init_file = gf_cfg_get_key(gf_config_file, MAIN_SECTION, SCENE_INIT);
			input->config->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
			input->config->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
			input->config->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
	
			input->config->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
			input->config->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
	
			input->config->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
			input->config->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
						
			/* Acknowledging the configuration */ 
			gf_sk_send(conn_socket, "OK\n", 3);

			memset(temp, 0, sizeof(temp));
			fp = fopen(input->config->scene_init_file, "w+");
			if (!fp) {
				fprintf(stdout, "[broadcaster] : Error opening temp file for the initial scene\n");
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

				} else 

					break;
			}
			fclose(fp);
			*(input->config_flag) = 1;
		}
		/* we only wait now for the config updates */
		if ( (*(input->config_flag)) == 1) {
			ret = sscanf(buffer, "DelaiMax=%d\n", timer);							
			fprintf(stdout, "[broadcaster] : RAP timer changed, now : %d\n", *timer);
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
	int tcp_port; 
	u32 config_flag;	// pour savoir s'il faut lire la configuration du fichier ou de l'interface
	char config_file[MAX_BUF];

	TCP_Input *tcp_conf;
	RAP_Input *rap_conf;
	CONF_Data *conf;
	
	/* parametres de configuration */
	GF_Config *gf_config_file;
	GF_Thread *tcp_thread;
	GF_Thread *rap_thread;
	GF_Err th_err_tcp;
	GF_Err th_err_rap;
	GF_Err res;
	
	GF_Socket *UDP_feedback_socket;
	
	PNC_CallbackData * data;
	GF_RTPChannel * chan;
  	GF_RTPHeader hdr;
	u32 timer;
	
	GF_Mutex *carrousel_mutex;	// objet en mutex
	char sdp_fmt[5000];
	
	/* init gpac lib */
	gf_sys_init();
	
	/* allocation structures */
	GF_SAFEALLOC(tcp_conf, TCP_Input)
	GF_SAFEALLOC(rap_conf, RAP_Input)
	GF_SAFEALLOC(conf, CONF_Data)
		
	/* gestion la ligne de commande */
	tcp_port = config_flag = 0;
	command_line_parsing(&argc, argv, &tcp_port, config_file, (int *) &config_flag);
	// fprintf(stdout, "[broadcaster] : tcp:%d, udp:%d, config_flag:%d\n", tcp_port, udp_port, config_flag);
	
	/* controle si les deux parametres necessaires ont ete specifies */
	tcp_conf->config_flag = &config_flag;
	
	gf_config_file = NULL;

	/* controle pour savoir ou il faut prendre la configuration */
	if(config_flag == 1)
	{
		/* ici il faut lire du fichier de configuration */
		gf_config_file = gf_cfg_new(NULL, config_file);
		conf->scene_init_file = gf_cfg_get_key(gf_config_file, MAIN_SECTION, SCENE_INIT);
		conf->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
		conf->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
		tcp_port = atoi(conf->config_input_port);
		conf->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);

		conf->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
		conf->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);

		conf->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
		conf->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
	}
	
	// thread pour ecouter les données de l'interface tcp (et configuration si config_flag == 0)

	// il faut definir le poineur ici pour que apres tout ça marche bien
	tcp_conf->RAPtimer = &timer;
	tcp_conf->port = tcp_port;
	tcp_conf->config = conf;	// dans le cas ou on prend la configuration de l'interface ici on peut modifier les donnees
	tcp_thread = gf_th_new("TCPInterface");
	th_err_tcp = gf_th_run(tcp_thread, tcp_server, tcp_conf);
	
	if(config_flag == 0)
		fprintf(stdout, "[broadcast] : en attente de la configuration de l'interface...\n");

	// ici il faut attendre que les donnees arrivent de l'interface
	while(config_flag == 0)
	{ 
		gf_sleep(1000); 
	}
	fprintf(stdout, "[broadcast] : configuration OK, init serveur\n");
	
	// setting the timer RAP
	// cette partie change dans le cas ou on prend la configuratoin de l'interface
	timer = atoi(conf->rap_timer);
	
	// a ce point la nous avons la configuration du serveur
	// initialisation carrousel
	// Initialisation de la connexion
	// ici il faut donner un adresse de broadcast
  	res = PNC_InitRTP(&chan, (char *)conf->dest_ip, atoi(conf->dest_port));
 	if (res != 0) 
	{
		printf("[carrousel] : erreur d'initialisation RTP -> M4Err = %d\n", res); 
		exit(1);
	} 
  
	// objet mutex
	carrousel_mutex = gf_mx_new("Carrousel");
	
	// recupere l'objet data, necessaire pour toutes operations du carrousel
  	data = PNC_Init_SceneGenerator(chan, &hdr, (char *) conf->scene_init_file, atoi(conf->modif_input_port)); // la generation de scene
	data->carrousel_mutex = carrousel_mutex;	// ici on passe l'objet mutex à l'objet data
	data->RAPsent = 1;
	
	// preparation du socket pour envoyer le données de débit à l'interface
	UDP_feedback_socket = gf_sk_new(GF_SOCK_TYPE_UDP);
	e = gf_sk_bind(UDP_feedback_socket, NULL, (u16) atoi(conf->feedback_port),  (char *) conf->feedback_ip, (u16) atoi(conf->feedback_port), 0);
	// fprintf(stdout, "[broadcaster] : bind udp for feedback : %d, port : %d\n", e, atoi(port_feedback));
	//e = gf_sk_connect(UDP_feedback_socket, (char *) conf->feedback_ip, (u16) atoi(conf->feedback_port, NULL));
	// fprintf(stdout, "[broadcaster] : connect udp feedback socket : %d, host : %s\n", e, ip_feedback);
	gf_sk_set_block_mode(UDP_feedback_socket, 0);
	data->feedback_socket = UDP_feedback_socket;

  	PNC_InitPacketiser(data, sdp_fmt); // le packetiser, qui initialise le RTPBuilder, les headers, etc.
  	PNC_SendInitScene(data); // ici on envoie le RAP initial

	// ouvrir thread boucle pour envoi rap
	rap_conf->RAPtimer = &timer;
	rap_conf->carrousel_mutex = carrousel_mutex;	// ici on passe l'objet mutex
	rap_conf->data = data;
	rap_thread = gf_th_new("RAPGenerator");
	th_err_rap = gf_th_run(rap_thread, RAP_send, rap_conf);

	// generation du fichier SDP
	sdp_generator(data, conf->dest_ip, sdp_fmt);
	
	// udp boucle pour les données de modification
	run = 1;
	while (run) {

		// cette fonction est modifié par rapport à la fonction du carrousel normale
		// car elle integre le control sur la variable mutex pour envoyer en exclusion
		// avec le thread des RAP
		GF_Err e = PNC_processBIFSGenerator(data); 
		if (e) break;

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
	rap_conf->status = 0;
	while (rap_conf->status != 2)
		gf_sleep(0);

	tcp_conf->status = 0;
	while (tcp_conf->status != 2)
		gf_sleep(0);

	/* nettoyage final */
	PNC_Close_SceneGenerator(data);
	
	free(tcp_conf);
	free(rap_conf);
	free(conf);
	fprintf(stdout, "[broadcaster] : structures freed\n");
	
	if (gf_config_file)
		gf_cfg_del(gf_config_file);
	gf_delete_file("temp.cfg");

	gf_th_del(tcp_thread);
	gf_th_del(rap_thread);

	gf_mx_del(carrousel_mutex);
	gf_sys_close();
	return 0;
}
	/*seems OK under mingw also*/
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

