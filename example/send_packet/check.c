#include "comman_header.h"

uint8_t packet_hex[]={
	0x3c,0x97,0x0e,0x95,0xa9,0x6e,0x00,
	0xe0,0x4c,0x09,0x76,0xf3,0x08,0x00,
	0x45,0x00,0x00,0x45,0xf2,0xab,0x40,
	0x00,0x40,0x11,0xc4,0x3e,0xc0,0xa8,
	0x01,0x6c,0xc0,0xa8,0x01,0x01,0xcd,
	0xd5,0x00,0x35,0x00,0x31,0x8a,0x9f,
	0xae,0xab,0x01,0x00,0x00,0x01,0x00,
	0x00,0x00,0x00,0x00,0x00,0x05,0x73,
	0x74,0x61,0x72,0x74,0x06,0x75,0x62,
	0x75,0x6e,0x74,0x75,0x03,0x63,0x6f,
	0x6d,0x06,0x72,0x6f,0x75,0x74,0x65,
	0x72,0x00,0x00,0x1c,0x00,0x01
};


void print_buf(const char *bottom, uint64_t len)
{
	printf ("[%s,%d]print_buf function\n", __func__, __LINE__);
	uint64_t loop;
	for ( loop = 0; loop < len; loop++)
	{
		if (loop % 16 == 0)
			printf ("\n");
		printf ("[%c]", bottom[loop]);
	}
}

void deal_packet( char *argc, const struct pcap_pkthdr *pcap_hdr, const char *buf)
{
	uint64_t leng;
	leng = pcap_hdr->len;
	print_buf(buf, leng);	
		
}

int main(void)
{
	uint8_t ebuf[1024];
	uint8_t packet_buff[1518] = {0x00};
	pcap_t *device;
	device = pcap_open_live( "nac0", 1518, 1, 0, ebuf);
	if( NULL == device )
	{
		printf ("[%s,%d]pcap_open_live failed\n", __func__, __LINE__);
		return -1;
	}
	printf ("open card successful\n");

	int fd = open( "single.pcap", O_RDONLY);
	if ( fd < 3){
		printf ("[%s,%d]open file failed\n", __func__, __LINE__);
		goto fail;	
	}
	printf ("fd:%d\n", fd);

	if ( (off_t)-1 == lseek( fd, 24, SEEK_SET) ){
		printf ("[%s,%d]lseek failed\n", __func__, __LINE__);
		close(fd);
		goto fail;
	}
	
	ssize_t read_cnt = read( fd, packet_buff, sizeof(packet_buff));
	if ( -1 == read_cnt ){
		printf ("[%s,%d]read failed\n",__func__, __LINE__);
		close(fd);
		goto fail;
	}	
	printf ("read_cnt:%d\n", read_cnt);
	struct pcap_pkthdr *pkthdr = ( struct pcap_pkthdr*)packet_buff;
	uint32_t caplen = ntohl(pkthdr->len);
	uint8_t *spoint = packet_buff + sizeof(struct pcap_pkthdr);
	printf ("caplen:%lu\n",caplen);

	pcap_sendpacket( device, packet_hex, sizeof(packet_hex));

	close(fd);
	fd = 0;

fail:
	pcap_close(device);
	printf ("close card successful\n");
}
