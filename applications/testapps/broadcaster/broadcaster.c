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
	
	while(1)
	{
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
	
//#if 0

#ifdef USE_TCP_STANDARD	// STANDARD
	int fd, conn_fd;
	struct sockaddr_in servaddr;
#else
	GF_Socket *TCP_socket;
	GF_Socket *conn_socket;
	GF_Err e;
#endif	
	
#ifdef USE_TCP_STANDARD	// STANDARD
	fd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(input->port);
	bind(fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	listen(fd, 1);
	// fprintf(stdout, "[broadcaster] : tcp server waiting...\n");
#else
	TCP_socket = gf_sk_new(GF_SOCK_TYPE_TCP);
	e = gf_sk_bind(TCP_socket, input->port, NULL, 0, 0);
	// fprintf(stdout, "return of bind %d\n", e);
	e = gf_sk_listen(TCP_socket, 1);
	// fprintf(stdout, "return of listen %d\n", e);
	gf_sk_set_block_mode(TCP_socket, 0);
	e = gf_sk_server_mode(TCP_socket, 0);
#endif	
	
	// ouvrir fichier temp pour avoir scene init	
	/* boucle serveur tcp */
	while(1)
	{	
		memset(buffer, 0, sizeof(buffer));
#ifdef USE_TCP_STANDARD		// STANDARD

		conn_fd = accept(fd, (struct sockaddr *) NULL, NULL);
		
		if((*(input->config_flag)) == 0)	// cas ou il faut attendre configuration
		{
			byte_read = read(conn_fd, buffer, sizeof(buffer));
			// selon le protocole on reçoit les donnees de configuration, on envoie OK 
			// et on attend pour le fichier de la scnene init
			
			// 1. recevoir les donnees de configuration
			// overture du fichier temp pour la configuration et pour avoir la scene initiale
			// fp = gf_temp_file_new();
			fp = fopen("temp.cfg", "w+");
			if(fp == NULL)
			{
				fprintf(stdout, "[broadcaster] : erreur, probleme a ouvrir file temp pour config\n");
				exit(1);
			}
			ret = fwrite(buffer, 1, byte_read, fp);
			fclose(fp);
			
			// 2. parsing des donnees de config avec librairie gpac
			// fprintf(stdout, "[broadcaster] : config temp received, now parsing...%d\n", ret);
			gf_config_file = gf_cfg_new(".", "temp.cfg");
			input->config->scene_init_file = gf_cfg_get_key(gf_config_file, MAIN_SECTION, SCENE_INIT);
			input->config->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
			input->config->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
			input->config->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
	
			input->config->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
			input->config->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
	
			input->config->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
			input->config->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
			/*
			input->config->scene_init_file = "scene_init.bt";
			
			input->config->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
			// input->config->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
			input->config->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
	
			input->config->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
			input->config->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
	
			input->config->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
			input->config->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
			*/
			// cette partie change dans le cas ou on prend la configuratoin de l'interface
			gf_delete_file("temp.cfg");
			// 3. envoi du message OK
			ret = write(conn_fd, "OK\n", 3);
			fprintf(stdout, "[broadcaster] : sent OK message to interface\n");
			// 4. sauvegarde du fichier de la scene initiale
			memset(temp, 0, sizeof(temp));	// pour stocker les parties du fichier de scene init
			fp = fopen(input->config->scene_init_file, "w+");
			if(fp == NULL)
			{
				fprintf(stdout, "[broadcaster] : erreur, probleme a ouvrir file temp pour scene initiale\n");
				exit(1);
			}
			while((byte_read = read(conn_fd, temp, sizeof(temp))) > 0)
			{
				// fprintf(stdout, "[broadcaster] : scene init : %s\n", temp);
				ret = fwrite(temp, 1, byte_read, fp);
			}
			fclose(fp);
			*(input->config_flag) = 1;
			fprintf(stdout, "[broadcaster] : configuration chargee de l'interface, pret a demarrer avec scene init...\n");
		}
		if((*(input->config_flag)) == 1)	// cas ou on attend seulement les modifications pour le RAP
		{
			while((byte_read = read(conn_fd, buffer, sizeof(buffer))) > 0)
			{
				// fprintf(stdout, "[broadcaster] : flag %d received tcp %d -> %s", *(input->config_flag), byte_read, buffer);
				ret = sscanf(buffer, "DelaiMax=%d\n", timer);
				
				//if(ret == 0)
				//	write(conn_fd, "KO\n", 3);	// pour dire a l'interface que nous n'avons pas besoin de la configuration
				//else
				fprintf(stdout, "[broadcaster] : changed RAP timer, now : %d\n", *timer);
			}
		}
		close(conn_fd);
#else		// STOP STANDARD

#if 0
		while(1)
		{
			e = gf_sk_accept(TCP_socket, &conn_socket);
			
			if (e == GF_IP_NETWORK_EMPTY) {
				gf_sleep(30);
			} else if (e == GF_OK) {
				memset(buffer, 0, sizeof(buffer));
				// la receive semble pas bloquant alors que l'API l'annonce
				e = gf_sk_receive(conn_socket, buffer, MAX_BUF, 0, &byte_read);
				fprintf(stdout, "[broadcaster] : received tcp %d -> %s", byte_read, buffer);
				// gf_sk_del(conn_socket);
				// return 0;
			}
			switch (e) 
			{
				case GF_IP_NETWORK_EMPTY:
					gf_sleep(5000);
					break;
				case GF_OK:
					
					if((*(input->config_flag)) == 0)	// cas ou il faut attendre configuration
					{
						e = gf_sk_receive(conn_socket, buffer, MAX_BUF, 0, &byte_read);
						// selon le protocole on reçoit les donnees de configuration, on envoie OK 
						// et on attend pour le fichier de la scnene init
						
						// 1. recevoir les donnees de configuration
						// overture du fichier temp pour la configuration et pour avoir la scene initiale
						// fp = gf_temp_file_new();
						fp = fopen("temp.cfg", "w+");
						if(fp == NULL)
						{
							fprintf(stdout, "[broadcaster] : erreur, probleme a ouvrir file temp pour config\n");
							exit(1);
						}
						ret = fwrite(buffer, 1, byte_read, fp);
						fclose(fp);
						
						// 2. parsing des donnees de config avec librairie gpac
						// fprintf(stdout, "[broadcaster] : config temp received, now parsing...%d\n", ret);
						gf_config_file = gf_cfg_new(".", "temp.cfg");
						input->config->scene_init_file = gf_cfg_get_key(gf_config_file, MAIN_SECTION, SCENE_INIT);
						input->config->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
						input->config->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
						input->config->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
				
						input->config->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
						input->config->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
				
						input->config->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
						input->config->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
						/*
						input->config->scene_init_file = "scene_init.bt";
						
						input->config->rap_timer = gf_cfg_get_key(gf_config_file, MAIN_SECTION, RAP_TIMER);
						// input->config->config_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_CONFIG);
						input->config->modif_input_port = gf_cfg_get_key(gf_config_file, MAIN_SECTION, PORT_MODIF);
				
						input->config->dest_ip = gf_cfg_get_key(gf_config_file, DEST_SECTION, DEST_ADDRESS);
						input->config->dest_port = gf_cfg_get_key(gf_config_file, DEST_SECTION, PORT_OUTPUT);
				
						input->config->feedback_ip = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, IP_FEEDBACK);
						input->config->feedback_port = gf_cfg_get_key(gf_config_file, FEEDBACK_SECTION, PORT_FEEDBACK);
						*/
						// cette partie change dans le cas ou on prend la configuratoin de l'interface
						gf_delete_file("temp.cfg");
						// 3. envoi du message OK
						//ret = write(conn_fd, "OK\n", 3);
						gf_sk_send(conn_socket, "OK\n", 3);
						fprintf(stdout, "[broadcaster] : sent OK message to interface\n");
						// 4. sauvegarde du fichier de la scene initiale
						memset(temp, 0, sizeof(temp));	// pour stocker les parties du fichier de scene init
						fp = fopen(input->config->scene_init_file, "w+");
						if(fp == NULL)
						{
							fprintf(stdout, "[broadcaster] : erreur, probleme a ouvrir file temp pour scene initiale\n");
							exit(1);
						}
						while((byte_read = gf_sk_receive(conn_socket, temp, sizeof(temp))) > 0)
						{
							// fprintf(stdout, "[broadcaster] : scene init : %s\n", temp);
							ret = fwrite(temp, 1, byte_read, fp);
						}
						fclose(fp);
						*(input->config_flag) = 1;
						fprintf(stdout, "[broadcaster] : configuration chargee de l'interface, pret a demarrer avec scene init...\n");
					}
					if((*(input->config_flag)) == 1)	// cas ou on attend seulement les modifications pour le RAP
					{
						while((byte_read = read(conn_fd, buffer, sizeof(buffer))) > 0)
						{
							// fprintf(stdout, "[broadcaster] : flag %d received tcp %d -> %s", *(input->config_flag), byte_read, buffer);
							ret = sscanf(buffer, "DelaiMax=%d\n", timer);
							
							//if(ret == 0)
							//	write(conn_fd, "KO\n", 3);	// pour dire a l'interface que nous n'avons pas besoin de la configuration
							//else
								fprintf(stdout, "[broadcaster] : changed RAP timer, now : %d\n", *timer);
						}
					}
				}
				break;
    		default:
      			fprintf(stdout, "[broadcaster] : erreur de socket tcp : %d\n", e);
      			exit(1);
      			break;
    	}
#endif
#endif
	}

//#endif

	return GF_OK;
}
	
int main (int argc, char** argv)
{
	GF_Err e;
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
	
	/* controle pour savoir ou il faut prendre la configuration */
	if(config_flag == 1)
	{
		/* ici il faut lire du fichier de configuration */
		gf_config_file = gf_cfg_new(".", config_file);
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
	tcp_thread = gf_th_new();
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
	carrousel_mutex = gf_mx_new();
	
	// recupere l'objet data, necessaire pour toutes operations du carrousel
  	data = PNC_Init_SceneGenerator(chan, &hdr, (char *) conf->scene_init_file, atoi(conf->modif_input_port)); // la generation de scene
	data->carrousel_mutex = carrousel_mutex;	// ici on passe l'objet mutex à l'objet data
	data->RAPsent = 1;
	
	// preparation du socket pour envoyer le données de débit à l'interface
	UDP_feedback_socket = gf_sk_new(GF_SOCK_TYPE_UDP);
	// e = gf_sk_bind(UDP_feedback_socket, (u16) atoi(conf->port_feedback),  (char *) conf->ip_feedback, (u16) atoi(conf->port_feedback), 0);
	// fprintf(stdout, "[broadcaster] : bind udp for feedback : %d, port : %d\n", e, atoi(port_feedback));
	e = gf_sk_connect(UDP_feedback_socket, (char *) conf->feedback_ip, (u16) atoi(conf->feedback_port));
	// fprintf(stdout, "[broadcaster] : connect udp feedback socket : %d, host : %s\n", e, ip_feedback);
	gf_sk_set_block_mode(UDP_feedback_socket, 0);
	data->feedback_socket = UDP_feedback_socket;

  	PNC_InitPacketiser(data, sdp_fmt); // le packetiser, qui initialise le RTPBuilder, les headers, etc.
  	PNC_SendInitScene(data); // ici on envoie le RAP initial

	// ouvrir thread boucle pour envoi rap
	rap_conf->RAPtimer = &timer;
	rap_conf->carrousel_mutex = carrousel_mutex;	// ici on passe l'objet mutex
	rap_conf->data = data;
	rap_thread = gf_th_new();
	th_err_rap = gf_th_run(rap_thread, RAP_send, rap_conf);

	// generation du fichier SDP
	sdp_generator(data, sdp_fmt);
	
	// udp boucle pour les données de modification
	while(1)
	{
		// cette fonction est modifié par rapport à la fonction du carrousel normale
		// car elle integre le control sur la variable mutex pour envoyer en exclusion
		// avec le thread des RAP
		PNC_processBIFSGenerator(data); 
	}
	
	/* nettoyage final */
	PNC_Close_SceneGenerator(data);
	
	free(tcp_conf);
	free(rap_conf);
	free(conf);
	fprintf(stdout, "[broadcaster] : structures freed\n");
	
	gf_cfg_del(gf_config_file);
	gf_th_del(tcp_thread);
	gf_th_del(rap_thread);
	gf_mx_del(carrousel_mutex);
	gf_sys_close();
	return 0;
}
