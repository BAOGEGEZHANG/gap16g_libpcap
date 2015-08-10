#ifndef NACTYPE_H
#define NACTYPE_H

#include <inttypes.h>
#include <time.h>
#include <stdlib.h>


#define NAC_STREAM_MAX 32
#define NAC_REG_MAX_ENTRIES 32

typedef struct nac_reg
{
#if defined(__ppc__) || defined(__POWERPC__)

	/* Big-endian layout. */
	uint32_t  addr    : 16;
	uint32_t  module  : 8;
	uint32_t  version : 4;
	uint32_t  flags   : 4;
	
#else

	/* Little-endian layout. */
	uint32_t  addr    : 16;
	uint32_t  module  : 8;
	uint32_t  flags   : 4;
	uint32_t  version : 4;
	
#endif /* Endianness. */
} nac_reg_t;


typedef struct host_policy_arg{
	uint8_t outport;
	uint8_t is_drop;
	uint8_t is_hash ;
	uint8_t stream_num ;
}host_policy_arg_t;

typedef enum nacminor
{
	NACMINORBITS  = 4,
	NACMINORMASK  = (1<<NACMINORBITS)-1,
	NACMINOR_NAC  = 0,
	NACMINOR_MEM  = 1,
	NACMINOR_IOM  = 2,
	NACMINOR_ARM  = 3,
	NACMINOR_MAX  = 4
} nacminor_t;


typedef struct nacpbm_global 
{
	volatile uint32_t * status;		// Control / Status
	volatile uint32_t * burst_threshold;	// Burst threshold
	volatile uint32_t * burst_timeout;	// Burst timeout
	uint8_t  version;			// Burst manager version
} nacpbm_global_t;


typedef struct nacpbm_stream 
{
	volatile uint32_t * status;		// Control / Status
	volatile uint32_t dma_buf_start;		// Mem hole base address
	volatile uint32_t dma_buf_end;		// Mem hole size
	volatile uint32_t record_ptr;		// Record pointer
	volatile uint32_t limit_ptr;		// Limit pointer
	volatile uint32_t * safetynet_cnt;	// At limit event pointer
	volatile uint32_t drop_cnt;		// Drop counter
	volatile uint32_t dma_ctl_reg;
} nacpbm_stream_t;


typedef struct 
{
	uint8_t            *buf;               /* large buffer                      */
	uint32_t           size;              /* Size of the stream's memory hole  */
	uint32_t           offset;            /* Offset to current record          */
	uint32_t           processed;         /* Amount of data processed so far   */
	nacpbm_stream_t   *pbm;               /* Pointer to stream burst manager   */
	uint32_t           phyaddr;           /* Pointer to physical base address  */
	uint32_t           mindata;
	struct timeval maxwait;
	struct timespec poll;
	uint32_t           extra_window_size; /* size of the second mmap         */
	int32_t            free_space;        /* free/used space                 */
	uint32_t          last_bus_top;          /* last read value of record ptr   */
	uint32_t          last_bus_bottom;       /* last written value of limit ptr */
	uint32_t           is_first_time:1;   /* is the stream's first call?     */
	uint32_t           attached:1;        /* is the stream attached?         */
	uint32_t           started:1;         /* is the stream started?          */
} stream_t;


typedef struct nacinf 
{
	uint32_t		id;				/* NAC device number */
	uint32_t		phy_addr;		/* PCI address of large buffer */
	uint32_t		buf_size;		/* its size */
	uint32_t		iom_size;		/* iom size */
	uint16_t		device_code;	/* PCI device ID */
#if defined(_WIN32)
	uint32_t		bus_num;		/* PCI bus number */
	uint16_t		dev_num;		/* PCI device number */
	uint16_t		fun_num;		/* Function number within a PCI device */
	uint32_t		ui_num;			/* User-perceived slot number */
#endif
}  nacinf_t;

	
typedef struct sheep 
{
#if defined(_WIN32)
	HANDLE      handle; 						/* Windows version doesn't rely on file descriptors
	                     							 * for sheep handling, so it needs to store the handle 
	                    								 * to the open device	                     */
#endif
	char			nacname[32];   				/* NAC card device name              */
	int			naciom;         					/* IOM file descriptor, cannot be 0  */
	stream_t		stream[NAC_STREAM_MAX]; 	/* Stream properties         */
	uint8_t*		iom;           					/* IO memory pointer                 */
	nacinf_t		nacinf;
	uint32_t		brokencuraddr;  				/* fix for ECMs and Dag4.1s          */
	uint32_t		byteswap;       				/* endianness for 3.4/3.51ecm        */
	nac_reg_t		regs[NAC_REG_MAX_ENTRIES]; 		/* register enum cache    */
	nacpbm_global_t *pbm;           				/* Pointer to global burst manager   */
	uint8_t		num_rx_streams; 			/* Number of receive streams         */
	uint8_t		num_tx_streams; 			/* Number of transmit streams        */
	uint32_t		has_arm:1;     			 	/* indicates arm is present          */
	uint32_t		has_tx:1;
	uint32_t		implicit_detach:1;
	uint32_t		opened:1;
	uint32_t		cap_length;
	uint32_t		msu_config:1;
	uint32_t		num_stream;
} sheep_t;
/*
typedef struct erf_record {
	uint32_t          ts[2];
	uint8_t           type;
	uint8_t           flag;
	uint16_t          rlen;
	uint16_t          lctr;
	uint16_t          wlen;
	uint8_t           dst_mac[6];
	uint8_t           src_mac[6];
	uint16_t          ip_type;
	uint16_t          ip_ver;
} erf_record_t;
*/

typedef volatile union {
	struct {
		uint32_t		  port_id		:4;
		uint32_t  	  	  card_id	:4;
		uint32_t	 	  vid		:16;
		uint32_t		  in_port		:2;
		uint32_t		  reserved	:2;
		uint32_t		  forward	:1;
		uint32_t		  boardcast	:1;
		uint32_t		  drop		:1;
		uint32_t		  vld		:1;
		}data;
	uint32_t value;
}pkt_info_arg_t;

typedef struct erf_header{
	uint32_t		ts[2];
	uint16_t		rlen;
	uint16_t		wlen;
	pkt_info_arg_t hash_fwd_arg;
}erf_header_t;

typedef struct erf_record {
	uint32_t          ts[2];
	uint8_t           type;
	uint8_t           flags;
	uint16_t          rlen;
	uint16_t          lctr;
	uint16_t          wlen;
	uint32_t          hdlc;
} erf_record_t;


struct err_info
{
	uint8_t 	err_number;
	uint8_t* 	bottom;
	uint8_t* 	top;
	uint8_t* 	current;
};

typedef struct nac_rule{
	uint8_t        inch;
	uint8_t        inch_mask;
	unsigned char proto;
	unsigned char proto_mask;
	unsigned int src;
	unsigned int src_mask;
	unsigned int dst;
	unsigned int dst_mask;
	unsigned short sport;
	unsigned short sport_mask;
	unsigned short dport;
	unsigned short dport_mask;
} nac_rule_t;

typedef struct nac_action {
	unsigned short cmd;
	unsigned short outch;
} nac_action_t;


typedef struct statistics_cnt
{
	uint64_t tx_bytes;
	uint64_t rx_bytes;
	uint64_t tx_frames;
	uint64_t rx_frames;
} statistics_cnt_t;

typedef struct strategy_list
{
	uint8_t type;
	uint32_t speed ;
}strategy_list_t;

union semun
{
    int val ;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo  *__buf;	
};

typedef struct com_arg
{
    char cmd[10];
    int stream_num;
}com_arg_t;

/*Remote Update API*/
/**************************************************************************/
typedef volatile union status_reg{
	struct {
#ifdef NAC_BIG_ENDIAN	
		uint16_t sr15	:1;	//bit15
		uint16_t sr14	:1;
		uint16_t sr13	:1;
		uint16_t sr12	:1;
		uint16_t sr11	:1;
		uint16_t sr10	:1;
		uint16_t sr9	:1;
		uint16_t sr8	:1;
		uint16_t sr7	:1;
		uint16_t sr6	:1;
		uint16_t sr5	:1;
		uint16_t sr4	:1;
		uint16_t sr3	:1;
		uint16_t sr2	:1;
		uint16_t sr1	:1;
		uint16_t sr0	:1;	//bit0
#else
		uint16_t sr0	:1;	//bit0
		uint16_t sr1	:1;
		uint16_t sr2	:1;
		uint16_t sr3	:1;
		uint16_t sr4	:1;
		uint16_t sr5	:1;
		uint16_t sr6	:1;
		uint16_t sr7	:1;
		uint16_t sr8	:1;
		uint16_t sr9	:1;
		uint16_t sr10	:1;
		uint16_t sr11	:1;
		uint16_t sr12	:1;
		uint16_t sr13	:1;
		uint16_t sr14	:1;
		uint16_t sr15	:1;	//bit16
#endif
		}SR_t;
	uint16_t value;
} status_reg_t ;

typedef struct addr_and_data{
	uint32_t addr;
	uint16_t data;
}addr_and_data_t;

typedef enum{
	UNLOCKED = 0X0000,LOCKED = 0X0001,UNLOCKED_AND_LOCKEDDOWN = 0X0002,LOCKED_AND_LOCKEDDOWN = 0X0003
}lock_stat_t;
#endif
