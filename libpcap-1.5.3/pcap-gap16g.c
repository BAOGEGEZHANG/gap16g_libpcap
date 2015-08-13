/*
 * pcap-qnf.c: Packet capture interface for Endace qnf card.
 *
 * The functionality of this code attempts to mimic that of pcap-linux as much
 * as possible.  This code is compiled in several different ways depending on
 * whether qnf_ONLY and HAVE_qnf_API are defined.  If HAVE_qnf_API is not
 * defined it should not get compiled in, otherwise if qnf_ONLY is defined then
 * the 'qnf_' function calls are renamed to 'pcap_' equivalents.  If QNF_ONLY
 * is not defined then nothing is altered - the qnf_ functions will be
 * called as required from their pcap-linux/bpf equivalents.
 *
 * Authors: Richard Littin, Sean Irvine ({richard,sean}@reeltwo.com)
 * Modifications: Jesper Peterson  <support@endace.com>
 *                Koryn Grant      <support@endace.com>
 *                Stephen Donnelly <support@endace.com>
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-qnf.c,v 1.21.2.7 2007/06/22 06:43:58 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GAP16G_API

#include <sys/param.h>			/* optionally get BSD define */
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pcap-int.h"
#include "pcap.h"

#include <ctype.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_GAP16G_API
#include <net/if.h>
#include <pthread.h>
#include "pcap-gap16g.h"
#endif
struct mbuf;		/* Squelch compiler warnings on some platforms for */
struct rtentry;		/* declarations in <net/if.h> */

#define ATM_CELL_SIZE		52
#define ATM_HDR_SIZE		4
#define CAP_LEN_DEF			(0x5F)
#define MIN_BUFFER_SIZE		64 * 1024

/*
 * A header containing additional MTP information.
 */
#define MTP2_SENT_OFFSET		0	/* 1 byte */
#define MTP2_ANNEX_A_USED_OFFSET	1	/* 1 byte */
#define MTP2_LINK_NUMBER_OFFSET		2	/* 2 bytes */
#define MTP2_HDR_LEN			4	/* length of the header */

#define MTP2_ANNEX_A_NOT_USED      0
#define MTP2_ANNEX_A_USED          1
#define MTP2_ANNEX_A_USED_UNKNOWN  2


#define NAC_RECORD_LEN		16

#define GO
#ifdef GO
#define CALL_TRACK()    printf ("\033[31m%s,%s,%d\033[0m\n",__FILE__,__func__,__LINE__)
#else
#define CALL_TRACK()
#endif

/* SunATM pseudo header */
struct sunatm_hdr
{
	unsigned char	flags;		/* destination and traffic type */
	unsigned char	vpi;		/* VPI */
	unsigned short	vci;		/* VCI */
};

static const unsigned short endian_test_word = 0x0100;

#define IS_BIGENDIAN() (*((unsigned char *)&endian_test_word))
#define MAX_NAC_PACKET 65536

static unsigned char TempPkt[MAX_NAC_PACKET];

static int qnf_setfilter(pcap_t *p, struct bpf_program *fp);
static int qnf_get_stats(pcap_t *p, struct pcap_stat *ps);
static int qnf_set_datalink(pcap_t *p, int dlt);
static int qnf_get_datalink(pcap_t *p);
static int qnf_setnonblock(pcap_t *p, int nonblock, char *errbuf);
static void qnf_platform_close(pcap_t *p);


/*  Management Data */
int shutdownInProcess;



/*
* name:  print_buf
* par : bottom:start memory pointer len:length of space
* ret : no
* description: print buffer
*/
void print_buf(const char *bottom, uint64_t len)
{
	printf("[%s,%d]print_buf function\n", __func__, __LINE__);
	uint64_t loop;

	for (loop = 0; loop < len; loop++)
	{
		if (loop % 16 == 0)
			{ printf("\n"); }

		printf("[%x]", bottom[loop]);
	}
}

/*
* name: qnf_stream_close
* par: p:handle of interface
* ret : no
* description: global stop card streat (tx&&rx), release memory
*/
static void qnf_stream_close(pcap_t *p)
{
	CALL_TRACK() ;

	if (nac_stop_stream(p->fd, p->md.rx_stream) < 0)
		{ fprintf(stderr, "qnf_stop_stream: %s\n", strerror(errno)); }

	p->md.rx_start = 0;

	if (nac_detach_stream(p->fd, p->md.rx_stream) < 0)
		{ fprintf(stderr, "qnf_detach_stream: %s\n", strerror(errno)); }

	p->md.rx_attach = 0;

	if (nac_tx_stop_stream(p->fd, p->md.tx_stream , COPY_FWD) < 0)
	{
		printf("[%s,%d]Unable to stop tx_stream %d\n",  __func__, __LINE__, p->md.tx_stream);
	}

	if (nac_tx_detach_stream(p->fd, p->md.tx_stream, COPY_FWD) < 0)
	{
		printf("[%s,%d]Unable to detach tx_stream %d\n", __func__, __LINE__, p->md.tx_stream);
	}
}

/*
* name: qnf_platform_close
* par : p:handle of interface
* ret : no
* description: default system close the pcap_t
*/
void qnf_platform_close(pcap_t *p)
{
	CALL_TRACK() ;
	qnf_stream_close(p);
}

/*
* name: atext_handler
* par : signum: signal
* ret : no
* description: set var_shutdownInProcess
*/
static void atexit_handler(int signum)
{
	shutdownInProcess = 1;
}


/*
* name: qnf_card_config
* par : handle of interface
* ret: suc 0 , failed -1
* description: global config card
*/
static  int qnf_card_config(pcap_t *p)
{
	CALL_TRACK() ;
	signal(SIGINT , atexit_handler);
	signal(SIGTERM, atexit_handler);

	if (nac_inline_msu_configure(p->fd, NUM_STREAM, COPY_FWD) < 0)
	{
		printf("[%s,%d]nac_inlien_msu_configure failed\n", __func__, __LINE__);
		return (-1);
	}

	return 0;
}

/*
*	name: qnf_stream_config
* par: handle of interface
* ret: success 0, failed -1
* description: configure stream tx & rx stream
*/
static int qnf_stream_config(pcap_t *p)
{
	CALL_TRACK() ;
	char ebuf[128] = {0x00};
	int ret ;

	/* config rx_stream */
	if (nac_attach_stream(p->fd, p->md.rx_stream, 0, 0) < 0)
	{
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "qnf_attach_stream : %s",  pcap_strerror(errno));
		goto rx_attach_fail;
	}

	p->md.rx_attach = 1;

	if (nac_set_stream_poll(p->fd, p->md.rx_stream, p->md.mindata, &(p->md.maxwait), &(p->md.poll)) < 0)
	{
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "qnf_set_stream_poll: %s\n", pcap_strerror(errno));
		goto rx_attach_fail;
	}

	printf("p->fd:%d\tp->md.rx_stream:%d\n", p->fd, p->md.rx_stream);

	if (nac_start_stream(p->fd, p->md.rx_stream) < 0)
	{
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "qnf_start_stream : %s\n",  pcap_strerror(errno));
		goto rx_attach_fail;
	}

	p->md.rx_start = 1;

	/* config tx_stream */
	if (nac_tx_attach_stream(p->fd, p->md.tx_stream, COPY_FWD, 0))
	{
		printf("[%s,%d]nac_tx_attach_stream is failed\n", __func__, __LINE__);
		goto tx_attach_fail;
	}

	p->md.tx_attach = 1;

	if (nac_tx_set_stream_poll(p->fd, p->md.tx_stream , p->md.mindata, &(p->md.maxwait), &(p->md.poll), COPY_FWD) < 0)
	{
		printf("[%s,%d]nac_tx_set_stream_poll is failed", __func__, __LINE__);
		goto tx_attach_fail;
	}

	if (nac_tx_start_stream(p->fd, (p->md.tx_stream), COPY_FWD) < 0)
	{
		printf("[%s,%d]nac_tx_start_stream is failed\n", __func__, __LINE__);
		goto tx_attach_fail;
	}

	p->md.tx_start = 1;
	return 0;
tx_attach_fail:

	if (nac_tx_detach_stream(p->fd, p->md.tx_stream , COPY_FWD) < 0)
	{
		printf("[%s,%d]nac_tx_detach_stream is failed\n", __func__, __LINE__);
	}

	p->md.tx_attach = 0;
rx_attach_fail:

	if (nac_detach_stream(p->fd, p->md.rx_stream) < 0)
	{
		fprintf(stderr, "qnf_detach_stream: %s\n", strerror(errno));
	}

	p->md.rx_attach = 0;
	return (-1);
}


/*
* name: qnf_read
* par : p: handle of interface ; cnt : number of wanted capture ;
        callback: deal function to deal each packet ; user: user_arg
* ret : num of capture
* description: main function to get packet
*/

static int qnf_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	static struct pcap_pkthdr	pcap_header;
#define TOP		(p->md.qnf_mem_top)
#define BOTTOM		(p->md.qnf_mem_bottom)
	uint64_t processed = 0;
	erf_record_t *erf_ptr = NULL;
	int	wlen = 0;
	int	rlen = 0;
	uint64_t ts ;
	uint32_t sec ;
	uint32_t usec ;
	memset(&pcap_header, 0, sizeof(struct pcap_pkthdr));

	if (0 == cnt)
	{
		return -2;
	}

	do
	{
		TOP = nac_advance_stream(p->fd, p->md.rx_stream, (uint8_t * *)&BOTTOM);

		if (TOP == NULL)
		{
			printf("[nac_read]: error, top pointer is NULL\n");
			return -1;
		}

		if (shutdownInProcess)
		{
			p->break_loop = 0;
			return -2;
		}
	}
	while ((TOP == BOTTOM) || ((TOP - BOTTOM) <= NAC_RECORD_LEN));

	while (TOP - BOTTOM > NAC_RECORD_LEN)
	{
		erf_ptr = (erf_record_t *)BOTTOM;
		wlen = ntohs(erf_ptr->wlen);
		rlen = ntohs(erf_ptr->rlen);
		/* Calculate the time of card */

		if ((TOP - BOTTOM) < rlen)
			{ break; }

		pcap_header.len = wlen;
		pcap_header.caplen = rlen;
		gettimeofday(&pcap_header.ts, NULL);
		pcap_header.ts.tv_sec = sec;
		pcap_header.ts.tv_usec = usec;
		// print_buf( BOTTOM, rlen);

		if (p->snapshot < pcap_header.len)
		{
			/* Don't do anything */
		}
		else if ((p->fcode.bf_insns == NULL) || (bpf_filter(p->fcode.bf_insns, (BOTTOM + NAC_RECORD_LEN), pcap_header.len, pcap_header.caplen)))
		{
			p->md.stat.ps_recv++;
			callback(user, &pcap_header, (BOTTOM + NAC_RECORD_LEN));
			processed++;
		}

		BOTTOM += rlen;

		if (processed == cnt)
		{
			return cnt;
		}
	}

	return processed;
}

/*
* name: qnf_inject
* par: p:handle of interface; buf: start pointer of raw packet; size : length of raw packet
* ret : returns the number of bytes written on success and -1 on failure.
* description: send packet
*/
static int qnf_inject(pcap_t *p, const void *buf _U_, size_t size _U_)
{
	CALL_TRACK();
	uint8_t *cp_org;
	uint8_t *packet;
	uint32_t  packet_len;
	erf_record_t pkt_hdr;

	if (size > 1514)
	{
		printf("[%s,%d]qnf_inject size is too long, then we will trunc\n", __func__, __LINE__);
		size = 1514;
	}

	packet_len = (size / 16 + 1) * 16;
	printf("packet_len:%u size:%d\n", packet_len, size);
	packet = malloc(packet_len);

	if (NULL == packet)
	{
		printf("[%s,%d] packet malloc failed\n", __func__, __LINE__);
		return (-1);
	}

	CALL_TRACK();
	memset(packet, 0x00, sizeof(char) * packet_len);
	pkt_hdr.rlen = htons(packet_len);
	pkt_hdr.wlen = htons(size);
	memcpy(packet, &pkt_hdr, sizeof(pkt_hdr));  /* erf_header 16Bytes*/
	memcpy(packet + NAC_RECORD_LEN, buf, size);
	//print_buf(packet , packet_len);
	cp_org = nac_tx_get_stream_space(p->fd, p->md.tx_stream, packet_len);

	if (NULL == cp_org)
	{
		printf("[%s,%d]nac_tx_get_stream_space is failed\n", __func__, __LINE__);
		goto fail;
	}

	memcpy(cp_org, packet, packet_len);

	if (NULL == nac_tx_stream_commit_bytes(p->fd, p->md.tx_stream , packet_len))
	{
		printf("[%s,%d]nac_tx_stream_commit_bytes is failed\n", __func__, __LINE__);
		goto fail;
	}

	free(packet);
	return (size);
fail:
	free(packet);
	return (-1);
}


/*
* name: qnf_active
* par : handle of interface
* ret : success 0
* description: active interface
*/
static int qnf_active(pcap_t *p)
{
	p->activated = 1;
	return 0;
}


/*
 *  Get a handle for a live capture from the given qnf device.  Passing a NULL
 *  device will result in a failure.  The promisc flag is ignored because qnf
 *  cards are always promiscuous.  The to_ms parameter is also ignored as it is
 *  not supported in hardware.
 *
 *  snaplen is now also ignored, until we get per-stream slen support. Set
 *  slen with approprite qnf tool BEFORE pcap_open_live().
 *
 *  See also pcap(3).
 *  ret: success !NULL failed NULL
 */
pcap_t *
qnf_open_live(const char *device, int snaplen, int promisc, int to_ms, char *ebuf)
{
	CALL_TRACK();
	char *s;
	int n, ret;
	uint8_t  	stream_index = 0;
	uint8_t   card_index;
	uint8_t   full_cardname[32];
	pcap_t 		*handle;
	/* device:name nac0 */

	if (!isdigit(device[3]))
	{
		printf("[%s,%d]Bad device num:%s\n", __func__, __LINE__, device);
		return (NULL);
	}

	/* full_cardname : /dev/nac0 */
	strcpy(full_cardname, "/dev/");
	strcat(full_cardname, device);
	/* Allocate a handle for this session. */
	handle = malloc(sizeof(*handle));

	if (handle == NULL)
	{
		printf("[%s,%d]malloc failed\n", __func__, __LINE__);
		goto fail;
	}

	memset(handle, 0, sizeof(*handle));

	/* Management data  */
	/* get drv fd */

	if ((handle->fd = nac_open(full_cardname)) < 0)
	{
		printf("[%s,%d]nac_open device failed,card_name:%s\n", __func__, __LINE__, full_cardname);
		goto fail;
	}

	/* CAPTURE_LENGTH */
	handle->snapshot	= snaplen;
	handle->bufsize 	= 0;
	handle->linktype 	= DLT_EN10MB;
	handle->oldstyle 	= 1;
	/*
	 * "select()" and "poll()" don't work on qnf device descriptors.
	 */
	handle->selectable_fd   = handle->fd;
	handle->read_op 	= qnf_read;
	handle->inject_op 	= qnf_inject;
	handle->setfilter_op 	= qnf_setfilter;
	handle->setdirection_op = NULL; 						/* Not implemented.*/
	handle->set_datalink_op = qnf_set_datalink;
	handle->getnonblock_op 	= pcap_getnonblock_fd;
	handle->setnonblock_op 	= qnf_setnonblock;
	handle->stats_op 	= qnf_get_stats;
	handle->cleanup_op 	= qnf_platform_close;
	handle->oneshot_callback = pcap_oneshot;
	handle->swapped = 0;
	handle->opt.promisc = 1;
	handle->opt.source = NULL;
	handle->activate_op 	= qnf_active;
	handle->md.rx_stream 	= 0;
	handle->md.tx_stream = 0 + NUM_STREAM;
	handle->md.mindata 		= MIN_BUFFER_SIZE;
	handle->activated 		= 1;
	handle->md.maxwait.tv_sec 	= 0;
	handle->md.maxwait.tv_usec 	= 10 * 1000;
	handle->md.poll.tv_sec 		= 0;
	handle->md.poll.tv_usec 	= 50;
	handle->md.qnf_mem_top = NULL;
	handle->md.qnf_mem_bottom = NULL;
	handle->activated = 1;  /* is actived when open_live */
	strcpy(handle->md.name, device);

	/*Configure card on the first time */
	if (qnf_card_config(handle)  < 0)
	{
		printf("[%s,%d]qnf_card_config failed\n", __func__, __LINE__);
		goto fail;
	}

	if (qnf_stream_config(handle) < 0)
	{
		printf("[%s,%d]qnf_stream_config is failed\n", __func__, __LINE__);
		goto fail;
	}

	return (handle);
	/*
	 * Get rid of any link-layer type list we allocated.
	 */
fail:

	if (handle != NULL)
	{
		free(handle);
	}

	return NULL;
}


/*
 * Simply submit all possible qnf names as candidates.
 * pcap_add_if() internally tests each candidate with pcap_open_live(),
 * so any non-existent devices are dropped.
 * For 2.5 try all rx stream names as well.
 */
int 	qnf_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	CALL_TRACK() ;
	char virtualname[20] ;
	sprintf(virtualname, "nac0");
	pcap_add_if(alldevsp, virtualname, 0, virtualname, errbuf);
	return 0;
}

/*
 * Installs the given bpf filter program in the given pcap structure.  There is
 * no attempt to store the filter in kernel memory as that is not supported
 * with qnf cards.
 */
static int qnf_setfilter(pcap_t *p, struct bpf_program *fp)
{
	if (!p)
		{ return -1; }

	if (!fp)
	{
		strncpy(p->errbuf, "setfilter: No filter specified",
		        sizeof(p->errbuf));
		return -1;
	}

	/* Make our private copy of the filter */

	if (install_bpf_program(p, fp) < 0)
		{ return -1; }

	p->md.use_bpf = 0;
	return (0);
}



static int qnf_set_datalink(pcap_t *p, int dlt)
{
	/* not neccessary */
	return (0);
}

static int qnf_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
	/*
	 * Set non-blocking mode on the FD.
	 * XXX - is that necessary?  If not, don't bother calling it,
	 * and have a "qnf_getnonblock()" function that looks at
	 * "p->md.qnf_offset_flags".
	 */
	if (pcap_setnonblock_fd(p, nonblock, errbuf) < 0)
		{ return (-1); }
	else
	{
		uint32_t mindata;
		struct timeval maxwait;
		struct timeval poll;

		/* Amount of data to collect in Bytes before calling callbacks.
		 * Important for efficiency, but can introduce latency
		 * at low packet rates if to_ms not set!
		 */
		if (nonblock)
			{ mindata = 0; }
		else
			{ mindata = 128; }

		maxwait.tv_sec = 0;
		maxwait.tv_usec = 1 * 10;
		poll.tv_sec = 0;
		poll.tv_usec = 1;

		if (nac_set_stream_poll(p->fd, p->md.rx_stream, mindata, &maxwait, &poll) < 0)
		{
			snprintf(errbuf, PCAP_ERRBUF_SIZE, "qnf_set_stream_poll: %s\n", pcap_strerror(errno));
			return -1;
		}
	}

	return (0);
}

/*
* name: qnf_get_datalink
* par : handle of interface
* ret : linktype of handle
* description: return linktype of handle
*/
static int qnf_get_datalink(pcap_t *p)
{
	return p->linktype;
}


/*
* name: qnf_get_stats
* par :handle of interface ; ps : stat of interface
* ret : success 0
* description: return stat of interface
*/
static int
qnf_get_stats(pcap_t *p, struct pcap_stat *ps)
{
	/* This needs to be filled out correctly.  Hopefully a qnfapi call will
		 provide all necessary information.
	*/
	*ps = p->md.stat;
	return 0;
}

#endif
