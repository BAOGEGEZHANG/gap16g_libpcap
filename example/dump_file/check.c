#include <pcap.h>
#include <stdlib.h>
#include <stdio.h>




int main(int argc, char *argv[])
{
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *device;
	
	device = pcap_open_live("eth0", 1518, 1, 0, errbuf);
	if (NULL == device){
		printf ("open device failed\n");
		return (-1);
	}

	pcap_dumper_t *device_dumper;
	device_dumper = pcap_dump_open( device, "savefile");
	if ( NULL == device_dumper){
		printf ("device_dumper open failed\n");
		return (-1);
	}
	

	pcap_loop( device, -1, pcap_dump, (u_char *)pcap_dump_file(device_dumper));
	printf ("end of pcap_loop\n");

	pcap_dump_close( device_dumper );
	device_dumper = NULL;
	


	pcap_close(device);
	device = NULL;

	return 0;
}
