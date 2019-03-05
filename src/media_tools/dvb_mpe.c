#include <gpac/internal/dvb_mpe_dev.h>
#include <gpac/network.h>
#include <string.h>


#ifdef GPAC_ENABLE_MPE

static void gf_m2ts_Delete_IpPacket(GF_M2TS_IP_Packet *ip_packet);


static void empty_list(GF_List * list)
{
	void *obj;
	while(gf_list_count(list)) {
		obj = gf_list_get(list,0);
		gf_list_rem(list,0);
		gf_free(obj);
		obj = NULL;
	}

}

static void on_dvb_mpe_section(GF_M2TS_Demuxer *ts, u32 evt_type, void *par)
{
	GF_M2TS_SL_PCK *pck = (GF_M2TS_SL_PCK *)par;
	unsigned char *data;
	u32 u32_data_size;
	u32 u32_table_id;

	if (evt_type == GF_M2TS_EVT_DVB_MPE) {
		data = pck->data;
		u32_data_size = pck->data_len;
		u32_table_id = data[0];

		switch(u32_table_id) {
		case GF_M2TS_TABLE_ID_INT:
			gf_m2ts_process_int(ts, (GF_M2TS_SECTION_ES *)pck->stream, data, u32_data_size, u32_table_id);
			break;

		case GF_M2TS_TABLE_ID_MPE_FEC:
		case GF_M2TS_TABLE_ID_DSM_CC_PRIVATE:
			if ((ts->ip_platform != NULL) || ts->direct_mpe) {
				GF_M2TS_SECTION_MPE* mpe = (GF_M2TS_SECTION_MPE*)pck->stream;
				gf_m2ts_process_mpe(ts, mpe, data, u32_data_size, u32_table_id);
			}
			else {
				//fprintf(stderr, "Time Slice Parameters for MPE-FEC have not been found yet \n");
			}
			break;
		default:
			return;
		}
	}
}

static void on_dvb_mpe_fec_frame(GF_M2TS_Demuxer *ts, MPE_FEC_FRAME *mff)
{
	if (ts->ip_platform != NULL) {
		if(ts->dvb_h_demux == 0) {
			gf_m2ts_gather_ipdatagram_information(mff,ts);
		} else {
			gf_m2ts_process_ipdatagram(mff,ts);
		}
	}
}

GF_EXPORT
void gf_dvb_mpe_init(GF_M2TS_Demuxer *ts)
{
	if (ts && !ts->on_mpe_event) ts->on_mpe_event = on_dvb_mpe_section;
}

GF_EXPORT
void gf_dvb_mpe_shutdown(GF_M2TS_Demuxer *ts)
{
	GF_M2TS_IP_Stream *ip_stream_buff;

	GF_M2TS_IP_PLATFORM * ip_platform;
	if (!ts)
		return;
	ip_platform = ts->ip_platform;

	if (!ip_platform) return;

	if (ip_platform->ip_streams) {
		while(gf_list_count(ip_platform->ip_streams)) {
			ip_stream_buff=gf_list_get(ip_platform->ip_streams, 0);

			while (gf_list_count(ip_stream_buff->targets)) {
				GF_M2TS_IP_Target *ip_targets = gf_list_get(ip_stream_buff->targets, 0);
				gf_free(ip_targets);
				gf_list_rem(ip_stream_buff->targets,0);
			}
			gf_free(ip_stream_buff);
			gf_list_rem(ip_platform->ip_streams,0);

		}
		gf_list_del(ip_platform->ip_streams);
	}
	ip_platform->ip_streams = NULL;
	if (ip_platform->socket_struct) {
		while(gf_list_count(ip_platform->socket_struct)) {
			GF_SOCK_ENTRY *socket_struct = gf_list_get(ip_platform->socket_struct, 0);
			gf_free(socket_struct);
			gf_list_rem(ip_platform->socket_struct,0);
		}
		gf_list_del(ip_platform->socket_struct);
	}
	ip_platform->socket_struct = NULL;
	gf_free(ip_platform);
	ts->ip_platform = NULL;
}

GF_EXPORT
GF_M2TS_ES *gf_dvb_mpe_section_new()
{
	GF_M2TS_ES *es;

	GF_M2TS_SECTION_MPE *ses;
	GF_SAFEALLOC(ses, GF_M2TS_SECTION_MPE);
	ses->mff = NULL;
	es = (GF_M2TS_ES *)ses;
	es->flags = GF_M2TS_ES_IS_SECTION | GF_M2TS_ES_IS_MPE;
	return es;
}

GF_EXPORT
void gf_dvb_mpe_section_del(GF_M2TS_ES *es)
{
	GF_M2TS_SECTION_MPE *ses = (GF_M2TS_SECTION_MPE *)es;

	/*TODO - cleanup MPE FEC frame state & co*/
	if (ses->mff) {
		if (ses->mff->mpe_holes)
			gf_list_del(ses->mff->mpe_holes);
		ses->mff->mpe_holes = NULL;
		gf_free(ses->mff);
		ses->mff=NULL;
	}
}


/*void print_bytes(u8* data, u32 line_length, u32 length, Bool last_line)
{
	u32 i;
	for (i = 0; i < length; i ++) {
		if (i%line_length == 0) {
			fprintf(stderr, "%2d: ", i/line_length);
		} else if (i%8 == 0) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "%02x", data[i]);
		if ((i+1)%line_length == 0) {
			fprintf(stderr, "\n");
		}
	}
	if (last_line) fprintf(stderr, "\n");
}
*/

void gf_m2ts_process_mpe(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_MPE *mpe, unsigned char *data, u32 data_size, u8 table_id)
{
	GF_M2TS_IP_Stream *ip_stream_buff;
	GF_M2TS_IP_PLATFORM * ip_platform = ts->ip_platform;
	u32 table_boundry_flag;
	u32 frame_boundry_flag;
	u32 offset;
	u32 i_streams,j;
	u32 section_number, last_section_number;
	s32 len_left = data_size;
	assert( ts );


	i_streams = 0;

//	fprintf(stderr, "Processing MPE/MPE-FEC data PID %d (%d/%d)\n",mpe->pid, data[6],data[7]);

	if (!gf_m2ts_crc32_check(data, data_size - 4)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("CRC error in the MPE/MPE-FEC data \n"));
	}

	/*get number of rows of mpe_fec_frame from descriptor*/
	section_number = data[6];
	last_section_number = data[7];
	//fprintf(stderr,  "table_id: %x section_length: %d section_number: %d last : %d \n", data[0], (data[1] & 0xF)<<8|data[2], section_number, last_section_number);

	if (ts->direct_mpe) {
		if (table_id != GF_M2TS_TABLE_ID_DSM_CC_PRIVATE) return;
		if (section_number != last_section_number) {
			fprintf(stderr, "MPE IP datagram on several section not supported\n");
			return;
		}
		/* send the IP data :
		   Remove the first 12 bytes of header (from table id to end of real time parameters
		   Remove also the last four 4 bytes of the section (CRC 32)
		*/
		gf_m2ts_mpe_send_datagram(ts, mpe->pid, data +12, data_size - (12+4));
		return;
	}


	/*get number of rows of mpe_fec_frame from descriptor*/
	/* Real-Time Parameters */
	//delta_t = (data[8]<<4)|(data[9]>>4);
	table_boundry_flag = (data[9] >> 3 )& 0x1;
	frame_boundry_flag = (data[9] >> 2 )& 0x1;

	offset = ((data[9] & 0x3)<< 16) | (data[10] << 8)| data[11];

	/* Using MFF structure attached to the MPE Stream */
	if(mpe->mff) {
		if(!mpe->mff->mpe_holes) {
			mpe->mff->mpe_holes = gf_list_new();
		}
	} else if(offset != 0) {
		/* If no MFF structure is attached to the MPE Stream, wait for a new IP Datagram before processing data */
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[IpdcEgine] buffer is not null, waiting for a new IP Datagram before processing data\n"));
		return;
	} else {
		GF_SAFEALLOC(mpe->mff,MPE_FEC_FRAME);

		assert( ip_platform );
		assert(ip_platform->ip_streams);
		i_streams = gf_list_count(ip_platform->ip_streams);
		for(j=0; j<i_streams; j++) {
			ip_stream_buff=gf_list_get(ip_platform->ip_streams, j);

			if(mpe->program->number == ip_stream_buff->location.service_id) {
				switch(ip_stream_buff->time_slice_fec.frame_size) {
				case 0:
					mpe->mff->rows =256;
					break;
				case 1:
					mpe->mff->rows =512;
					break;
				case 2:
					mpe->mff->rows =768;
					break;
				case 3:
					mpe->mff->rows =1024;
					break;
				default:
					break;
				}
				break;
			}
		}
		/*initialize the mpe fec frame */
		if (init_frame(mpe->mff, mpe->mff->rows)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("MFF initialization successed \n"));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("MFF initialization failed \n"));
			return;
		}
	}

	//fprintf(stderr, "PID: %4d, sec: %2d/%2d, t_id: %2d, d: %3d, tb: %d, fb: %d, add.: %8d\n", mpe->pid, section_number, last_section_number, id, delta_t, table_boundry_flag, frame_boundry_flag, offset);
	mpe->mff->PID = mpe->pid;

	while (len_left>0) {

		switch (table_id) {
		case GF_M2TS_TABLE_ID_DSM_CC_PRIVATE: /* MPE */
			/* Sets the IP data in the Application Data Table
			   Remove the first 12 bytes of header (from table id to end of real time parameters
			   Remove also the last four 4 bytes of the section (CRC 32)*/
			setIpDatagram( mpe->mff, offset, data +12, data_size-(12+4));
			len_left = 0;
			break;
		case GF_M2TS_TABLE_ID_MPE_FEC:
			/*	RS data is set by column, one column at a time */
			//setColRS( mpe->mff, offset, data + 12, mpe->mff->rows);
			len_left = 0;
			//data += (mff->rows +12+4 );
			break;
		default:
			//	fprintf(stderr, "Unknown table id %x \n", table_id );
			len_left = 0;
		}

		if(table_boundry_flag == 1) { /* end of reception of ADT data or RS data */
			if(table_id == 0x3E) { /* end of ADT */
				mpe->mff->ADT_done =1;
				if(mpe->mff->current_offset_adt+1 !=  mpe->mff->capacity_total) {
					memset( mpe->mff->p_adt+mpe->mff->current_offset_adt,0, mpe->mff->capacity_total-( mpe->mff->current_offset_adt+1));
				}
			}
			/* end of RS should be catched below by frame_boundary_flag */
		}

		if(frame_boundry_flag == 1) {
			if(table_id == 0x78) {
				if( mpe->mff->current_offset_rs+1 !=  mpe->mff->rows*64) {
					memset( mpe->mff->p_rs+ mpe->mff->current_offset_rs,0,( mpe->mff->rows*64)-( mpe->mff->current_offset_rs+1));
				}
			}
		}

	}

	if (frame_boundry_flag && table_boundry_flag && mpe->mff->ADT_done ==1 ) {
		//decode_fec(mpe->mff);
		on_dvb_mpe_fec_frame(ts, mpe->mff);
		resetMFF(mpe->mff);
		/*for each IP datagram reconstructed*/
	}

}

void gf_m2ts_process_ipdatagram(MPE_FEC_FRAME *mff,GF_M2TS_Demuxer *ts)
{
	GF_M2TS_IP_Packet *ip_packet;
	u8* ip_datagram;
	u32 i, i_holes;
	MPE_Error_Holes *mff_holes;
	u32 offset; /* offset to get through the datagram */
	u8 ip_address_bootstrap[4];
	Bool Boostrap_ip;

	offset =0;
	ip_datagram = mff->p_adt;

	GF_SAFEALLOC(ip_packet,GF_M2TS_IP_Packet);


	while(offset<mff->current_offset_adt)
	{
		/* Find the parts of the ADT which contain errors and skip them */
		if((mff->p_error_adt+offset)[0] == 0x01010101) {
			i_holes = gf_list_count(mff->mpe_holes);
			for(i=0; i<i_holes; i++) {
				mff_holes=gf_list_get(mff->mpe_holes, i);
				if(mff_holes->offset == offset) {
					offset += mff_holes->length;
					break;
				}
			}
		}

		if(gf_m2ts_ipdatagram_reader(ip_datagram, ip_packet, offset)) {


			/* update the offset */
			//offset += ip_packet->u32_total_length;
			offset += (ip_packet->u32_hdr_length*4) + ip_packet->u32_udp_data_size;


			/* 224.0.23.14 IP Bosstrap */
			ip_address_bootstrap[0]=224;
			ip_address_bootstrap[1]=0;
			ip_address_bootstrap[2]=23;
			ip_address_bootstrap[3]=14;
			socket_simu(ip_packet,ts, 1);

			if(ip_packet->u8_rx_adr[3] == 8) {
				fprintf(stderr, "\n");
			}

			/* compare the destination ip address and the ESG Bootstrap address */
			Boostrap_ip = gf_m2ts_compare_ip(ip_packet->u8_rx_adr,ip_address_bootstrap);
			if(Boostrap_ip) {
				fprintf(stderr, "ESG Bootstrap found !\n");
			}
		} else {
			offset += (ip_packet->u32_total_length);
		}

		if(ip_packet->data) {
			gf_free(ip_packet->data);
		}
		ip_packet->data = NULL;

	}

	//gf_memory_print();
	//gf_memory_size();

	empty_list(mff->mpe_holes);
	gf_list_del(mff->mpe_holes);
	mff->mpe_holes = NULL;
	gf_m2ts_Delete_IpPacket(ip_packet);
}


void gf_m2ts_mpe_send_datagram(GF_M2TS_Demuxer *ts, u32 mpe_pid, unsigned char *data, u32 data_size)
{
	GF_M2TS_IP_Packet ip_pck;
	u8 *udp_data;
	u32 hdr_len;

	ip_pck.u32_version = data[0] >>4;
	ip_pck.u32_hdr_length =data[0] & 0xF;
	ip_pck.u32_total_length = data[2]<<8 | data[3];
	ip_pck.u32_id_nb = data[4]<<8 | data[5];
	ip_pck.u32_flag = data[6] >>5;
	ip_pck.u32_frag_offset = (data[6] & 0x1F)<<8 | data[7];
	ip_pck.u32_TTL = data[8];
	ip_pck.u32_protocol = data[9];
	ip_pck.u32_crc = data[10]<<8 | data[11];
	memcpy(ip_pck.u8_tx_adr,data+12,sizeof(ip_pck.u8_tx_adr));
	memcpy(ip_pck.u8_rx_adr,data+16,sizeof(ip_pck.u8_rx_adr));

	hdr_len = ip_pck.u32_hdr_length;
	udp_data = data+(hdr_len*4);

	ip_pck.u32_tx_udp_port = udp_data[0]<<8 | udp_data[1];
	if(!ip_pck.u32_tx_udp_port) {
		return;
	}
	ip_pck.u32_rx_udp_port = udp_data[2]<<8 | udp_data[3];
	if(!ip_pck.u32_rx_udp_port) {
		return;
	}
	ip_pck.u32_udp_data_size = udp_data[4]<<8 | udp_data[5];
	if(ip_pck.u32_udp_data_size == 0) {
		return;
	}
	ip_pck.u32_udp_chksm = udp_data[6]<<8 | udp_data[7];

	/*excluding UDP header*/
	ip_pck.data = udp_data + 8;

	socket_simu(&ip_pck, ts, 0);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("MPE PID %d - send datagram %d bytes to %d.%d.%d.%d port:%d\n", mpe_pid, ip_pck.u32_udp_data_size-8, ip_pck.u8_rx_adr[0], ip_pck.u8_rx_adr[1], ip_pck.u8_rx_adr[2], ip_pck.u8_rx_adr[3], ip_pck.u32_rx_udp_port));
}


Bool gf_m2ts_compare_ip(u8 rx_ip_address[4], u8 ip_address_bootstrap[4])
{
	u8 i;
	for (i=0; i<4; i++)
	{
		if (rx_ip_address[i] != ip_address_bootstrap[i])
			return 0;
	}
	return 1;
}


u32 gf_m2ts_ipdatagram_reader(u8 *datagram,GF_M2TS_IP_Packet *ip_packet, u32 offset)
{

	ip_packet->u32_version = ((datagram+offset)[0])>>4;
	ip_packet->u32_hdr_length =(((datagram+offset)[0])&0xF);
	ip_packet->u32_total_length = (datagram+offset)[2]<<8|(datagram+offset)[3];
	ip_packet->u32_id_nb = (datagram+offset)[4]<<8|datagram[5];
	ip_packet->u32_flag = ((datagram+offset)[6])>>5;
	ip_packet->u32_frag_offset = ((datagram+offset)[6]&0x1F)<<8|(datagram+offset)[7];
	ip_packet->u32_TTL = (datagram+offset)[8];
	ip_packet->u32_protocol = (datagram+offset)[9];
	ip_packet->u32_crc = (datagram+offset)[10]<<8|(datagram+offset)[11];
	memcpy(ip_packet->u8_tx_adr,(datagram+offset)+12,sizeof(ip_packet->u8_tx_adr));
	memcpy(ip_packet->u8_rx_adr,(datagram+offset)+16,sizeof(ip_packet->u8_rx_adr));

	ip_packet->u32_tx_udp_port = ((datagram+offset)+(ip_packet->u32_hdr_length*4))[0]<<8|((datagram+offset)+(ip_packet->u32_hdr_length*4))[1];
	if(!ip_packet->u32_tx_udp_port) {
		return 0;
	}
	ip_packet->u32_rx_udp_port = ((datagram+offset)+(ip_packet->u32_hdr_length*4))[2]<<8|((datagram+offset)+(ip_packet->u32_hdr_length*4))[3];
	if(!ip_packet->u32_rx_udp_port) {
		return 0;
	}
	ip_packet->u32_udp_data_size = ((datagram+offset)+(ip_packet->u32_hdr_length*4))[4]<<8|((datagram+offset)+(ip_packet->u32_hdr_length*4))[5];
	if(ip_packet->u32_udp_data_size == 0) {
		return 0;
	}
	ip_packet->u32_udp_chksm = ((datagram+offset)+(ip_packet->u32_hdr_length*4))[6]<<8|((datagram+offset)+(ip_packet->u32_hdr_length*4))[7];


	ip_packet->data = gf_malloc((ip_packet->u32_udp_data_size-8)*sizeof(u8));
	memcpy(ip_packet->data,datagram+offset+(ip_packet->u32_hdr_length*4)+8,(ip_packet->u32_udp_data_size-8)*sizeof(u8));
	/*ip_packet->data = gf_malloc((ip_packet->u32_total_length-ip_packet->u32_hdr_length)*sizeof(char));
	memcpy(ip_packet->data,datagram+offset+20,(ip_packet->u32_total_length-ip_packet->u32_hdr_length)*sizeof(char));*/

	fprintf(stderr, "TX addr: %d.%d.%d.%d RX addr : %d.%d.%d.%d port:%d(0x%x) \n",ip_packet->u8_tx_adr[0],ip_packet->u8_tx_adr[1],ip_packet->u8_tx_adr[2],ip_packet->u8_tx_adr[3],ip_packet->u8_rx_adr[0],ip_packet->u8_rx_adr[1],ip_packet->u8_rx_adr[2],ip_packet->u8_rx_adr[3],ip_packet->u32_rx_udp_port,ip_packet->u32_rx_udp_port);

	return 1;

}

static void gf_m2ts_Delete_IpPacket(GF_M2TS_IP_Packet *ip_packet)
{
	ip_packet->u32_version = 0;
	ip_packet->u32_hdr_length =0;
	ip_packet->u32_total_length = 0;
	ip_packet->u32_id_nb = 0;
	ip_packet->u32_flag = 0;
	ip_packet->u32_frag_offset = 0;
	ip_packet->u32_TTL = 0;
	ip_packet->u32_protocol = 0;
	ip_packet->u32_crc = 0;
	ip_packet->u32_tx_udp_port = 0;
	ip_packet->u32_rx_udp_port = 0;
	ip_packet->u32_udp_data_size = 0;
	ip_packet->u32_udp_chksm = 0;

	if(ip_packet->data) {
		gf_free(ip_packet->data);
	}
	gf_free(ip_packet);
	ip_packet = NULL;
}



void gf_m2ts_process_int(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *ip_table, unsigned char *data, u32 data_size, u32 table_id)
{

	GF_M2TS_IP_PLATFORM * ip_platform = ts->ip_platform ;
	assert( ts );
//	fprintf(stderr, "Processing IP/MAC Notification table (PID %d) %s\n", ip_table->pid, (status==GF_M2TS_TABLE_REPEAT)?"repeated":"");
	//if ( status == GF_M2TS_TABLE_REPEAT ) return ;
	if ( ip_platform == NULL )
	{
		GF_SAFEALLOC(ip_platform,GF_M2TS_IP_PLATFORM );
		ip_platform->ip_streams= gf_list_new();
		//fprintf(stderr,  " \n initialize the Iip_platform \n");
		section_DSMCC_INT (ip_platform, data, data_size);
		ts->ip_platform = ip_platform;
	}
	//fprintf(stderr, "processing the INT table \n");



}


/*the following code is copied from dvbsnoop project : http://dvbsnoop.sourceforge.net */
void section_DSMCC_INT(GF_M2TS_IP_PLATFORM* ip_platform,u8 *data, u32 data_size)
{
	/* EN 301 192 7.x */

	s32  length,i;


	length = data_size ;

	data += 12 ;

	assert( ip_platform);
	i = dsmcc_pto_platform_descriptor_loop(ip_platform,data);
	data   += i;
	length -= i;

	while (length > 4) {
		GF_M2TS_IP_Stream *ip_str;
		GF_SAFEALLOC(ip_str,GF_M2TS_IP_Stream );

		i = dsmcc_pto_descriptor_loop(ip_str,data);
		data   += i;
		length -= i;

		i = dsmcc_pto_descriptor_loop(ip_str,data);
		data   += i;
		length -= i;
		assert( ip_platform->ip_streams );
		gf_list_add(ip_platform->ip_streams, ip_str);
	}

	return ;
}


/* Platform Descriptor */

u32 dsmcc_pto_platform_descriptor_loop(GF_M2TS_IP_PLATFORM* ip_platform, u8 *data)
{
	u32 loop_length;
	s32 length,i;


	loop_length = ((data[0]) & 0xF ) | data[1];
	length = loop_length;
	data += 2;
	while (length > 0) {
		assert( ip_platform);
		i   = platform_descriptorDSMCC_INT_UNT(ip_platform,data);
		data   += i;
		length -= i;
	}

	return  (loop_length+2);
}


u32  platform_descriptorDSMCC_INT_UNT(GF_M2TS_IP_PLATFORM* ip_platform, u8 *data)

{
	u32 length;
	u32 id;


	id  =   data[0];
	length = data[1]+2;

	switch (id) {


	case GF_M2TS_DVB_IP_MAC_PLATFORM_NAME_DESCRIPTOR:
	{
		//fprintf(stderr, " Information on the ip platform found \n");
		gf_ip_platform_descriptor(ip_platform, data);
	}
	break;
	case GF_M2TS_DVB_IP_MAC_PLATFORM_PROVIDER_NAME_DESCRIPTOR:
	{
		//fprintf(stderr, " Information on the ip platform found \n");
		gf_ip_platform_provider_descriptor(ip_platform, data);
	}
	break;

	default:
		break;
	}
	return length;   // (descriptor total length)
}

void gf_ip_platform_descriptor(GF_M2TS_IP_PLATFORM* ip_platform,u8 * data)
{
	u32 length;
	length = data[1];
	assert( ip_platform );
	/* allocation ofr the name of the platform */
	ip_platform->name = gf_malloc(sizeof(char)*(length-3+1));
	memcpy(ip_platform->name, data+5, length-3);
	ip_platform->name[length-3] = 0;
	return ;
}

void gf_ip_platform_provider_descriptor(GF_M2TS_IP_PLATFORM* ip_platform, u8 * data)
{
	u32 length;
	length = data[1];
	/* allocation of the name of the platform */
	assert( ip_platform );
	ip_platform->provider_name = gf_malloc(sizeof(char)*(length-3+1));
	memcpy(ip_platform->provider_name, data+5, length-3);
	ip_platform->provider_name[length-3] = 0;
	return ;
}



/* IP Stream Descriptors */
u32 dsmcc_pto_descriptor_loop ( GF_M2TS_IP_Stream *ip_str,u8 *data)
{
	u32 loop_length;
	s32 length,i;

	loop_length = ((data[0]) & 0xF ) | data[1];

	length = loop_length;
	data += 2;
	while (length > 0) {
		i   = descriptorDSMCC_INT_UNT(ip_str,data);
		data   += i;
		length -= i;
	}

	return  (loop_length+2);
}


u32  descriptorDSMCC_INT_UNT(GF_M2TS_IP_Stream *ip_str,u8 *data)

{
	u32 length;
	u32 id;

	id  = data[0];
	length = data[1] +2;

	switch (id) {


	case GF_M2TS_DVB_TARGET_IP_SLASH_DESCRIPTOR:
	{

		gf_m2ts_target_ip(ip_str,data);
	}
	break;

	case GF_M2TS_DVB_TIME_SLICE_FEC_DESCRIPTOR:
	{
		descriptorTime_slice_fec_identifier(ip_str,data);
	}
	break;
	case GF_M2TS_DVB_STREAM_LOCATION_DESCRIPTOR:
	{
		descriptorLocation(ip_str , data);
	}
	break;
	default:
		break;
	}


	return length;   // (descriptor total length)
}

void descriptorTime_slice_fec_identifier( GF_M2TS_IP_Stream *ip_str,u8 * data)
{

	ip_str->time_slice_fec.time_slicing = (data[2] >> 7) & 0x1;
	ip_str->time_slice_fec.mpe_fec = (data[2] >> 5 ) & 0x3 ;
	ip_str->time_slice_fec.frame_size = data[2] & 0x7 ;
	ip_str->time_slice_fec.max_burst_duration = data[3];
	ip_str->time_slice_fec.max_average_rate = (data[4]  >> 4) & 0xf ;
	ip_str->time_slice_fec.time_slice_fec_id = data[4] & 0xf;
	ip_str->time_slice_fec.id_selector = gf_malloc( data[1] - 3 ) ;
	memcpy(ip_str->time_slice_fec.id_selector, data + 4, data[1]-3 );
	return ;
}

void descriptorLocation(GF_M2TS_IP_Stream *ip_str , u8 * data)
{
	ip_str->location.network_id = (data[2]<<8)|data[3];
	ip_str->location.original_network_id =  (data[4]<<8)|data[5];
	ip_str->location.transport_stream_id = (data[6]<<8)|data[7];
	ip_str->location.service_id = (data[8]<<8)|data[9];
	ip_str->location.component_tag = data[10];
	return;
}

void gf_m2ts_target_ip(GF_M2TS_IP_Stream* ip_str, u8 * data)
{
	u32 i,j;
	u8 length;
	i=j=0;
	ip_str->targets = gf_list_new();
	length = data[1];
	for(i=0; i<length; i= i+5)
	{
		GF_M2TS_IP_Target* ip_data;
		GF_SAFEALLOC(ip_data,GF_M2TS_IP_Target);
		//ip_data=gf_malloc(sizeof(GF_M2TS_IP_Target));
		ip_data->type = 0;
		ip_data->address_mask = 0;
		memcpy(ip_data->address, data+2+i, 4);
		ip_data->slash_mask=data[2+i+4];

		gf_list_add(ip_str->targets,ip_data);
	}

	return;
}


/*generate RS code and fullfill the RS table of MPE_FEC_FRAME*/
void encode_fec(MPE_FEC_FRAME * mff)
{
#if 0
	u8 adt_rs_en_buffer [ MPE_ADT_COLS + MPE_RS_COLS ];
	u32 rows = mff->rows;
	u32 i = 0;
	u32 cols = 0;

	cols = mff->col_adt;
	for ( i = 0; i < rows; i ++ ) {
		/* read a row from ADT into buffer */
		getRowFromADT(mff, i, adt_rs_en_buffer);
		/* ************** */
		/* Encode data into codeword, adding NPAR parity bytes */
		encode_data(adt_rs_en_buffer, cols, adt_rs_en_buffer);
		fprintf(stderr, "Encoded data is   : \"%s\"\n", adt_rs_en_buffer);
		fprintf(stderr, "data with error is: \"%s\"\n", adt_rs_en_buffer);
		/*set a row of RS into RS table*/
		setRowRS ( mff, i , (unsigned char *)(adt_rs_en_buffer + cols) );
	}
#endif
}

/*decode the MPE_FEC_FRAME*/
void decode_fec(MPE_FEC_FRAME * mff)
{
	u32 i,ML,offset;
	u8 *data;
	u8 linebuffer[255];

	//fprintf(stderr, "Starting FEC decoding ...\n");

	data = gf_malloc((mff->rows*191)*sizeof(char));
	memset(data,0,sizeof(data));

	initialize_ecc ();
	ML = 255;
	memset(linebuffer, 0, 255);
	offset = 0;

	/*fprintf(stderr, "ADT data\n");
	print_bytes(mff->p_adt, 32, 1, 0);
	print_bytes(mff->p_adt+512, 32, 1, 0);
	print_bytes(mff->p_adt+512+512, 32, 1, 0);
	print_bytes(mff->p_adt+512+512+512, 32, 1, 1);
	fprintf(stderr, "RS data\n");
	print_bytes(mff->p_rs, 32, 1, 0);
	print_bytes(mff->p_rs+512, 32, 1, 0);
	print_bytes(mff->p_rs+512+512, 32, 1, 0);
	print_bytes(mff->p_rs+512+512+512, 32, 1, 1);*/

	for ( i = 0; i < mff->rows; i ++ )	{
		u8 tmp[255];

		getRowFromADT(mff, i, linebuffer);
		getRowFromRS(mff, i, linebuffer + mff->col_adt);

		encode_data(linebuffer, 191, tmp);

		decode_data(linebuffer, ML);
		if (check_syndrome () != 0) {
			u32 nerasures = 0;
			u32 *erasures = NULL;

			/* TODO: set the erasures and errors based on the MFF */
			if(correct_errors_erasures (linebuffer, ML, nerasures,  erasures) == 0)
			{
				//fprintf(stderr, "Correction Error line %d \n", i);
			}

			/* TODO: replace the current line in MFF */
		}

		memcpy(data+offset,linebuffer,sizeof(data));
		offset += 191;
	}
	//fprintf(stderr, "FEC decoding done.\n");
	memcpy(mff->p_adt,data,sizeof(data));
	//gf_m2ts_ipdatagram_reader(mff->p_adt,ip_datagram);


}


void gf_m2ts_gather_ipdatagram_information(MPE_FEC_FRAME *mff,GF_M2TS_Demuxer *ts)
{
	GF_M2TS_IP_Packet *ip_packet;
	u8* ip_datagram;
	u32 i, i_holes,i_streams, i_targets,k,j,l;
	MPE_Error_Holes *mff_holes;
	u32 offset; /* offset to get through the datagram */
	//GF_TSImport *tsimp = (GF_TSImport *) ts->user;
	//GF_MediaImporter *import= (GF_MediaImporter *)tsimp->import;
	GF_M2TS_IP_Stream *ip_stream_buff;
	GF_M2TS_IP_Target *ip_targets;
	GF_M2TS_IP_PLATFORM * ip_platform = ts->ip_platform;

	assert( ts );
	offset =0;
	ip_datagram = mff->p_adt;
	GF_SAFEALLOC(ip_packet,GF_M2TS_IP_Packet);
	GF_SAFEALLOC(mff_holes,MPE_Error_Holes);
	assert( ip_platform );
	while(offset<mff->current_offset_adt)
	{
		/* Find the parts of the ADT which contain errors and skip them */
		if((mff->p_error_adt+offset)[0] == 0x01010101)
		{
			i_holes = gf_list_count(mff->mpe_holes);
			for(i=0; i<i_holes; i++)
			{
				mff_holes=gf_list_get(mff->mpe_holes, i);
				if(mff_holes->offset == offset)
				{
					offset += mff_holes->length;
					break;
				}
			}

		}

		if(gf_m2ts_ipdatagram_reader(ip_datagram, ip_packet, offset)) {


			/* update the offset */
			//offset += ip_packet->u32_total_length;
			offset += (ip_packet->u32_hdr_length*4) + ip_packet->u32_udp_data_size;

			if(ip_platform->all_info_gathered != 1)
			{

				i_streams = 0;
				i_targets = 0;


				assert( ip_platform->ip_streams );
				i_streams = gf_list_count(ip_platform->ip_streams);
				for(k=0; k<i_streams; k++)
				{
					ip_stream_buff=gf_list_get(ip_platform->ip_streams, k);

					if(ip_stream_buff == NULL || ip_stream_buff->stream_info_gathered ==1)
					{
						break;
					}
					else
					{
						i_targets = gf_list_count(ip_stream_buff->targets);
						l=0;
						for(j=0; j<i_targets; j++)
						{
							ip_targets = gf_list_get(ip_stream_buff->targets, j);

							if(gf_m2ts_compare_ip(ip_packet->u8_rx_adr,ip_targets->address))
							{
								for(l=0; l<9; l++)
								{
									if(ip_targets->rx_port[l] == ip_packet->u32_rx_udp_port) goto next;
									if(ip_targets->rx_port[l] ==0) break;
								}

								ip_targets->rx_port[l] = ip_packet->u32_rx_udp_port;
								ip_stream_buff->PID = mff->PID;
								goto next;


							}

						}
					}
				}
			}
		} else {

			offset += (ip_packet->u32_hdr_length*4) + ip_packet->u32_udp_data_size;
		}
next :
		;

	}
	empty_list(mff->mpe_holes);
	gf_list_del(mff->mpe_holes);
	mff->mpe_holes = NULL;
	gf_m2ts_Delete_IpPacket(ip_packet);

}

GF_EXPORT
void gf_m2ts_print_mpe_info(GF_M2TS_Demuxer *ts)
{
	u32 i_streams, i_targets,i,j,l;
	GF_M2TS_IP_Stream *ip_stream_buff;
	GF_M2TS_IP_Target *ip_targets;
	u8 *ip_address;
	GF_M2TS_IP_PLATFORM * ip_platform = ts->ip_platform;
	assert( ts );
	if (!ts->ip_platform) return;

	/* provider and ip platform name */
	fprintf(stderr, " IP Platform : %s provided by %s \n",ip_platform->name,ip_platform->provider_name);

	i_streams = 0;
	i_targets = 0;


	assert(ip_platform->ip_streams);
	i_streams = gf_list_count(ip_platform->ip_streams);
	for(i=0; i<i_streams; i++)
	{
		ip_stream_buff=gf_list_get(ip_platform->ip_streams, i);
		fprintf(stderr, "PID:%d \n",ip_stream_buff->PID);
		fprintf(stderr, "Target IP Adress : \n");
		/*Print the target IP address  */
		i_targets = gf_list_count(ip_stream_buff->targets);
		for(j=0; j<i_targets; j++)
		{
			ip_targets = gf_list_get(ip_stream_buff->targets, j);

			l=0;

			ip_address = ip_targets->address;
			fprintf(stderr, "%d.%d.%d.%d/%d ",ip_address[0],ip_address[1],ip_address[2],ip_address[3],ip_targets->slash_mask);
			fprintf(stderr, "RX port :");
			while(ip_targets->rx_port[l] != 0)
			{
				fprintf(stderr, " %d ",ip_targets->rx_port[l]);
				l++;
			}
			fprintf(stderr, "\n");


		}

		/*Print the time slice fec descriptor */
		fprintf(stderr, "Time Slice Fec Descriptor : \n");

		if(ip_stream_buff->time_slice_fec.time_slicing==0)
		{
			fprintf(stderr, " No Time Slicing \n");
		} else
		{
			fprintf(stderr, " Time Slicing\n");
		}

		if(ip_stream_buff->time_slice_fec.mpe_fec==0)
		{
			fprintf(stderr, " No MPE FEC used \n");
		} else
		{
			fprintf(stderr, " MPE FEC used \n");
		}

		switch(ip_stream_buff->time_slice_fec.frame_size)
		{
		case 0:
		{
			fprintf(stderr, " Frame size : 256 rows \n");
			fprintf(stderr, " Max Burst Duration 512 kbits\n");
		}
		break;
		case 1:
		{
			fprintf(stderr, " Frame size : 512 rows \n");
			fprintf(stderr, " Max Burst Duration 1024 kbits\n");
		}
		break;
		case 2:
		{
			fprintf(stderr, " Frame size : 768 rows \n");
			fprintf(stderr, " Max Burst Duration 1536 kbits\n");
		}
		break;
		case 3:
		{
			fprintf(stderr, " Frame size : 1024 rows \n");
			fprintf(stderr, " Max Burst Duration 2048 kbits\n");
		}
		break;
		default:
			break;
		}

		fprintf(stderr, " Time Slice Fec ID : %x\n",ip_stream_buff->time_slice_fec.time_slice_fec_id);

		/* Locayion descriptor */

		fprintf(stderr, "Location Descriptor \n");
		fprintf(stderr, "Network ID:%d \n",ip_stream_buff->location.network_id);
		fprintf(stderr, "Original Network ID:%d \n",ip_stream_buff->location.original_network_id);
		fprintf(stderr, "Transport Stream ID:%d \n",ip_stream_buff->location.transport_stream_id);
		fprintf(stderr, "Service ID:%d \n",ip_stream_buff->location.service_id);
		fprintf(stderr, "Component Tag:%d \n",ip_stream_buff->location.component_tag);


		getchar(); // attendre l'appuie d'une touche
	}



}


void socket_simu(GF_M2TS_IP_Packet *ip_packet, GF_M2TS_Demuxer *ts, Bool yield)
{
	char name[100];
	u32 ipv4_addr;
	GF_Err e;
	u8 nb_socket_struct, i;
	GF_SOCK_ENTRY *Sock_Struct = NULL;
	assert( ts );
	if(!ts->ip_platform) {
		GF_SAFEALLOC(ts->ip_platform,GF_M2TS_IP_PLATFORM );
	}
	if(ts->ip_platform->socket_struct == NULL) ts->ip_platform->socket_struct= gf_list_new();

	ipv4_addr = GF_4CC(ip_packet->u8_rx_adr[0], ip_packet->u8_rx_adr[1], ip_packet->u8_rx_adr[2], ip_packet->u8_rx_adr[3]);
	nb_socket_struct = gf_list_count(ts->ip_platform->socket_struct);
	for(i=0; i<nb_socket_struct; i++) {
		Sock_Struct = gf_list_get(ts->ip_platform->socket_struct,i);
		if ((Sock_Struct->ipv4_addr==ipv4_addr)&& (Sock_Struct->port == (u16) ip_packet->u32_rx_udp_port)) {
			if (Sock_Struct->bind_failure) return;
			break;
		}
		Sock_Struct = NULL;
	}
	if (Sock_Struct == NULL) {
		GF_SAFEALLOC(Sock_Struct, GF_SOCK_ENTRY);

		Sock_Struct->ipv4_addr = ipv4_addr;
		Sock_Struct->port = ip_packet->u32_rx_udp_port;
		Sock_Struct->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (Sock_Struct->sock == NULL) {
			gf_free(Sock_Struct);
			return;
		}

		sprintf(name, "%d.%d.%d.%d", ip_packet->u8_rx_adr[0],ip_packet->u8_rx_adr[1], ip_packet->u8_rx_adr[2],ip_packet->u8_rx_adr[3]);

		if (gf_sk_is_multicast_address(name) ) {
			e = gf_sk_setup_multicast(Sock_Struct->sock, name, ip_packet->u32_rx_udp_port, 1/*TTL - FIXME this should be in a cfg file*/, 0, NULL/*FIXME this should be in a cfg file*/);
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Setting up multicast socket for MPE on %s:%d\n", name, ip_packet->u32_rx_udp_port ));
		} else {
			/*
				binding of the socket to send data to port 4600 on the local machine
				the first address / port parameters are NULL or 0 because there are not needed for sending UDP datagrams
				the second address is "localhost" and the port is the destination port on localhost
			*/
			e = gf_sk_bind(Sock_Struct->sock, "127.0.0.1", ip_packet->u32_rx_udp_port,/*name*/"127.0.0.1", ip_packet->u32_rx_udp_port, 0);
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Setting up socket for MPE on 127.0.0.1:%d\n", ip_packet->u32_rx_udp_port ));
		}

		if (e != GF_OK) {
			fprintf(stderr, "Server Bind Error: %s\n", gf_error_to_string(e));
			Sock_Struct->bind_failure = 1;
		}
		gf_list_add(ts->ip_platform->socket_struct, Sock_Struct);
	}

	// ********************************************************
	// Envoi des donn�es
	// ********************************************************

	e = gf_sk_send(Sock_Struct->sock, ip_packet->data, ip_packet->u32_udp_data_size - 8);
	if (e != GF_OK) {
		fprintf(stderr, "Error sending to \n");
	}
	if (yield) gf_sleep(10);

}

/*void gf_sock_shutdown()
{
	gf_sk_del(sock);
}*/

/* allocate the necessary memory space*/
Bool init_frame(MPE_FEC_FRAME * mff, u32 rows)
{
	assert (mff != NULL);
	if (rows != 256 && rows != 512 && rows != 768 && rows != 1024) return 0;
	mff->rows = rows ;
	mff->col_adt = MPE_ADT_COLS;
	mff->col_rs = MPE_RS_COLS;
	mff->p_adt = (u8 *)gf_calloc(MPE_ADT_COLS*rows,sizeof(u8));
	mff->p_rs = (u8 *)gf_calloc(MPE_RS_COLS*rows,sizeof(u8));


	fprintf(stderr, "MPE_RS_COLS*rows :%d \n",MPE_RS_COLS*rows);

	mff->capacity_total = mff->col_adt*rows;
	mff->p_error_adt = (u32 *)gf_calloc(mff->col_adt*rows,sizeof(u32));
	mff->p_error_rs = (u32 *)gf_calloc(mff->col_rs*rows,sizeof(u32));
	mff->current_offset_adt = 0;
	mff->current_offset_rs = 0;
	mff->ADT_done = 0;
	mff->PID = 0;
//	fprintf(stderr, "MFF: rows: %d, adt_col: %d, rs_col : %d, capacity : %d\n", mff->rows, mff->col_adt, mff->col_rs, mff->capacity_total );
	mff->mpe_holes = gf_list_new();
	mff->initialized = 1;
	return 1;
}

void resetMFF(MPE_FEC_FRAME * mff)
{
	mff->current_offset_adt = 0;
	mff->current_offset_rs = 0;
	memset(mff->p_error_adt, 0, mff->col_adt * mff->rows*sizeof(u32));
	memset(mff->p_error_rs, 0, mff->col_rs * mff->rows*sizeof(u32));
	memset(mff->p_adt, 0, MPE_ADT_COLS* mff->rows*sizeof(u8));
	memset(mff->p_rs, 0, MPE_RS_COLS* mff->rows*sizeof(u8));
	mff->ADT_done = 0;
	mff->PID = 0;
	if(mff->mpe_holes) {
		empty_list(mff->mpe_holes);
		//gf_list_del(mff->mpe_holes);
	}

}

/* return a row of appplicatio data table*/
void getRowFromADT(MPE_FEC_FRAME * mff, u32 index, u8* adt_row)
{
	u32 i = 0 ;
	u32 base = 0;
	//assert ( sizeof ( adt_row ) >= MPE_ADT_COLS );
	for ( i = 0; i < mff->col_adt ; i ++ ) {
		adt_row [ i ] = mff -> p_adt [ index + base ];
		base += mff-> rows ;
	}
}

/*return a row of RS table*/
void getRowFromRS (MPE_FEC_FRAME * mff, u32 index, u8* rs_row)
{
	u32 i = 0 ;
	u32 base = 0;
	assert (rs_row != NULL );
	//assert (sizeof ( rs_row ) >= MPE_ADT_COLS );
	for ( i = 0; i < mff->col_rs ; i ++ ) {
		rs_row [ i ] = mff -> p_rs [ index + base ];
		base += mff -> rows ;
	}
}

void setRowRS(MPE_FEC_FRAME *mff, u32 index, u8 *p_rs)
{
	u32 i = 0;
	u32 base = 0;
	assert ( p_rs != NULL );
	//assert ( sizeof (p_rs ) >= MPE_RS_COLS ) ;
	for ( i = 0; i < mff -> col_rs; i ++ ) {
		mff -> p_rs [ base + index ] = p_rs [ i];
		base += mff -> rows;
	}

}
void addPadding(MPE_FEC_FRAME *mff , u32 offset)
{
	u32 i = 0;
	fprintf(stderr, "add paddings from %d to the end %d\n", offset, mff->capacity_total );
	for ( i = offset ; i <mff->capacity_total; i ++ )
		mff -> p_adt [i] = 0xff ;
}
#ifdef GPAC_UNUSED_FUNC
static void print_bytes2(u8 * data, u32 length ) /*print_bytes2 */
{
	u32 i = 0;
	u32 row_num = 0;
	u32 k = 0;
	for ( i = 0; i < length ; i ++ ) {
		if (k == 0) {
			fprintf(stderr, "%x0  : ", row_num);
			k = 0;
		}
		fprintf(stderr, "%#x ", data[i]);
		k++;
		if (k == 16) {
			fprintf(stderr, "\n");
			k = 0;
			row_num ++;
		}
	}
}
#endif /*GPAC_UNUSED_FUNC*/

/*add a ip datagram into mpe fec frame, and indicate error positions*/
void setIpDatagram(MPE_FEC_FRAME * mff, u32 offset, u8* dgram, u32 length )
{
	MPE_Error_Holes *mpe_error_holes;

	GF_SAFEALLOC(mpe_error_holes,MPE_Error_Holes);


	if (offset >= mff->capacity_total) {
		fprintf(stderr, "Offset %d bigger than capacity %d \n", offset, mff->capacity_total );
	}
	if (offset+length >= mff->capacity_total) {
		fprintf(stderr, "Offset+length %d+%d bigger than capacity %d \n", offset, length, mff->capacity_total );
	}
	if (mff->current_offset_adt != offset) {
		if (mff->current_offset_adt > offset) {
			fprintf(stderr, "We missed an offset reset (%d to %d)\n", mff->current_offset_adt, offset);
			mff->current_offset_adt = offset;
		} else {
			fprintf(stderr, "there is an error hole in the ADT from %d to %d \n", mff->current_offset_adt, offset);
		}
		setErrorIndicator( mff->p_error_adt , mff->current_offset_adt , (offset - mff->current_offset_adt)*sizeof(u32)  ) ;
		mpe_error_holes->offset = mff->current_offset_adt;
		mpe_error_holes->length = offset - mff->current_offset_adt;
		gf_list_add(mff->mpe_holes,mpe_error_holes);
		mff->current_offset_adt = offset  ; // update the offset
	}

	memcpy(mff->p_adt+mff->current_offset_adt,dgram, length);
	mff->current_offset_adt = offset+length  ; // update the offset

}
/*set RS data into mpe fec frame*/
void setColRS( MPE_FEC_FRAME * mff, u32 offset, u8 * pds, u32 length )
{
	if ( mff->current_offset_rs != offset)	{
		fprintf(stderr, "there is an error hole in the RS from %d to %d \n", mff->current_offset_rs, offset );
		setErrorIndicator( mff->p_error_rs , mff->current_offset_rs , (offset - mff->current_offset_rs)*sizeof(u32));
		mff->current_offset_rs = offset;
	}
	assert(mff->rows == length);
	memcpy(mff->p_rs + mff->current_offset_rs , pds, length*sizeof(u8) );
	mff->current_offset_rs = offset + length ;

}

void getColRS(MPE_FEC_FRAME * mff, u32 offset, u8 * pds, u32 length)
{
	memcpy(pds,mff->p_rs + offset, length);
}

void getErrorPositions(MPE_FEC_FRAME *mff, u32 row, u32 * errPositions)
{
	u32 i = 0 ;
	u32 base = row;
	/*get error from adt*/
	for ( i = 0; i < mff->col_adt ; i ++ ) {
		errPositions [i] = mff->p_error_adt[base ];
		base += mff->rows;
	}
	base = row;
	for ( i = mff->col_adt ; i < mff->col_adt + mff-> col_rs ; i ++ ) {
		errPositions [i] = mff->p_error_rs [ base ];
		base += mff->rows ;
	}
}

u32  getErrasurePositions( MPE_FEC_FRAME *mff , u32 row, u32 *errasures)
{
	u32 i = 0;
	u32 base = row;
	u32 nb = 0;
	u32 k =0;
	for ( i = 0  ; i <  mff-> col_rs ; i ++ ) {
		if ( mff->p_error_rs[base] ==  1 ) {
			errasures [k++] = mff->p_error_rs [ base ];
			nb ++;
		}
		base += mff->rows ;
	}
	fprintf(stderr, " the erasure locations is:\n");
	for ( i = 0; i < nb ; i ++ )
		fprintf(stderr, "%d ", errasures[i]);
	return nb;
}

void setErrorIndicator(u32 * data , u32 offset, u32 length)
{
//	fprintf(stderr, "setting the error indication \n");
	memset(data+offset, 1, length);
}


/*void descriptor_PRIVATE (u8 *b, DTAG_SCOPE tag_scope){
	fprintf(stderr, "private descriptor \n");
	return ;
}*/


#endif //GPAC_ENABLE_MPE
