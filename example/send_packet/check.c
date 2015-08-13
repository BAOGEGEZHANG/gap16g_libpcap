#include "comman_header.h"



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
	pcap_t *device;
	device = pcap_open_live( "nac0", 1518, 1, 0, ebuf);
	if( NULL == device )
	{
		printf ("[%s,%d]pcap_open_live failed\n", __func__, __LINE__);
		return -1;
	}
	printf ("open card successful\n");

//	pcap_loop( device, 1, deal_packet, NULL );
	uint8_t packet[32] = "hello,world";
	pcap_sendpacket( device, packet, sizeof(packet));


	pcap_close(device);
	printf ("close card successful\n");
}
