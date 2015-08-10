#ifndef NACTS_H
#define NACTS_H


#if defined (__SVR4) && defined (__sun)
#include <sys/types.h>
#include <sys/conf.h>
#endif

#define DUCK_BASE_ADDR   		0x60060 
#define Duck_Cmd_Read_Running 0x001
#define Duck_Cmd_Read_Int     0x002
#define Duck_Cmd_Go           0x001
#define Duck_Cmd_Stop         0x002
#define Duck_Cmd_Reset        0x004
#define Duck_Cmd_Go_on_Sync   0x008
#define Duck_Cmd_Load_on_Sync 0x080
#define Duck_Cmd_Load_Immed   0x100
#define Duck_Cmd_Host_Latch   0x200
#define Duck_Cmd_Host_Pulse   0x400

#define Duck_Config_Muxin_RS422     0x01
#define Duck_Config_Muxin_Host      0x02
#define Duck_Config_Muxin_Ovrflow   0x04
#define Duck_Config_Muxin_Aux       0x08
#define Duck_Config_Muxout_RS422   0x10
#define Duck_Config_Muxout_Input   0x20
#define Duck_Config_Muxout_Host    0x40
#define Duck_Config_Muxout_Ovrflow 0x80

typedef enum duck_reg {
	cmd   = 0x00,
	conf  = 0x04,
	hload = 0x0c,
	dds   = 0x14,
	hread = 0x18,
	lread = 0x1c,
	hread1 = 0x20,
	lread1 = 0x24,	
} duck_reg_t;

typedef struct nac_duck {
	uint32_t    base;             /* io base address */
	uint32_t    dds;              /* dds value */
	uint32_t    pre_dds;
	uint32_t    sfreq;            /* desired frequency (2^width) */
	uint32_t    width;            /* active duck width */
	uint32_t    cfreq;            /* crystal frequency */

	uint32_t    pulses;           /* counter: pulses */
	uint32_t    single_misses;    /* counter: single pulses missing */
	uint32_t    longest_miss;     /* counter: longest run of missing pulses */
	uint32_t    bad_pulses;       /* counter: rejected ints */
	int32_t     freq_error;       /* current frequency error */
	uint32_t    worst_freq;       /* worst frequency error since clear */
	int32_t     phase_error;      /* current phase error */
	uint32_t    worst_phase;      /* worst phase error since clear */
	time_t      stat_start;       /* time stats cleared at */
	time_t      stat_end;         /* time stats ended */
	uint32_t    sickness;         /* counter: times unhealthy */
	uint32_t    health;           /* boolean: good pulses within 10 secs */
	int32_t     health_threshold; /* in 2^-32s (signed) */
	uint32_t    resyncs;          /* counter: reload highs due to drift */
	uint32_t    started;          /* boolean: duck has been started */
	uint64_t    last;             /* duck time at last interrupt */
	uint64_t    last_tsc;         /* TSC at last interrupt (0 if not available). */
	uint64_t    last_delta;
	uint32_t    sync_state;

#if defined (__linux__)

	struct timer_list	*timer;		/* duck init/health timer */
	wait_queue_head_t	wait;		/* duck wait queue */

#elif defined (_WIN32)

	KTIMER			timer;          /* duck health timer */
	KDPC			timerdpc;       /* dpc associated with the timer */
	BOOLEAN			timer_initialized;	/* tells us if we have to cancel the timer */
	PKEVENT			duckevent;      /* Event object used by applications to synchronize with the DUCK */
	BOOLEAN			nosynch;

#elif defined (__SVR4) && defined (__sun)

	timeout_id_t    timer;			/* duck init/health timer */

#elif defined (__FreeBSD__)

	struct callout_handle timer;    /* duck init/health timer */
	
#elif defined (__APPLE__) && defined(__ppc__)

	/* FIXME: need to have appropriate DUCK types here. */

#else
#error You are compiling on an unsupported platform. 
#endif /* Platform-specific code. */

} nac_duck_t;

struct nac_softc;

#if defined(__linux__) || defined(__FreeBSD__) || (defined (__SVR4) && defined (__sun))

int duck_init(struct nac_softc *sc);

#elif defined(_WIN32)

int duck_init(PVOID pdx);

#endif /* Platform-specific code. */

int duck_ioctl(nac_duck_t *duck, duckinf_t *duckinf);
void duck_intr(struct nac_softc *sc);
nac_duck_t *duck_new(void);
void duck_destroy(struct nac_softc *sc);


#endif /* NACTS_H */
