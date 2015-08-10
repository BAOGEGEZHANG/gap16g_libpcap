#ifndef NACNEW_H
#define NACNEW_H

#include "nacinf.h"

typedef enum nacminor
{
		NACMINORBITS  = 3,
		NACMINORMASK  = (1<<NACMINORBITS)-1,
		NACMINOR_NAC  = 0,
		NACMINOR_MEM  = 1,
		NACMINOR_IOM  = 2,
		NACMINOR_MAX  = 3
									
} nacminor_t;

#define NAC_STREAM_MAX		32
#define NAC_MIN_LATENCY	64

#define NAC_IOC_MAGIC		'i'
#define NAC_IOC_MAXNR		4
#define NACIOCINFO     _IOR(NAC_IOC_MAGIC,  2, nacinf_t)
#define NACIOCDUCK    _IOWR(NAC_IOC_MAGIC, 4, duckinf_t)

#endif

