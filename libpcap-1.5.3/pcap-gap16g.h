/*
 * pcap-qnf.c: Packet capture interface for Endace QNF card.
 *
 * The functionality of this code attempts to mimic that of pcap-linux as much
 * as possible.  This code is only needed when compiling in the QNF card code
 * at the same time as another type of device.
 *
 * Author: Richard Littin, Sean Irvine ({richard,sean}@reeltwo.com)
 *
 * @(#) $Header: /tcpdump/master/libpcap/pcap-qnf.h,v 1.3.4.1 2005/07/07 06:56:04 guy Exp $ (LBL)
 */
#ifndef  QNF_HAVE_API
#define QNF_HAVE_API


#include "gap16g/include/nac_stream.h"
#include "gap16g/include/nacapi.h"
#include "gap16g/include/nacreg.h"
#include "gap16g/include/nactypes.h"

pcap_t *qnf_open_live(const char *device, int snaplen, int promisc, int to_ms, char *ebuf);
int qnf_platform_finddevs(pcap_if_t **devlistp, char *errbuf);

#endif
