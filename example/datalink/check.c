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
	
	int dlt;
	dlt = pcap_datalink( device);
	printf ("dlt: %d\n", dlt);
	puts (pcap_datalink_val_to_name(dlt));
	dlt = 	pcap_datalink_name_to_val( pcap_datalink_val_to_name(dlt));
	printf ("dlt: %d\n", dlt);

	pcap_close(device);
	printf ("close card successful\n");
}
