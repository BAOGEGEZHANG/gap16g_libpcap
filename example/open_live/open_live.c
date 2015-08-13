#include "comman_header.h"



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
	pcap_close(device);
}
