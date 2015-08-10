/*
 * Linux
 */
#if defined(__linux__)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/msr.h>
#include "nacdrv.h"

/* Debugging macros. */
#undef DD
#ifdef DEBUG
#  define	DD(...)	printk(KERN_DEBUG __VA_ARGS__)
#else
#  define	DD(...) if(0){printk(KERN_DEBUG __VA_ARGS__);}
#endif
# define	DA(...)	printk(KERN_INFO __VA_ARGS__)

#define	DN(...) if(0){printk(__VA_ARGS__);}

/* Utility macros. */
# define	DUCKR(X,R)		(readl((u_char *)X->mmio + X->duck->base + R))
# define	DUCKW(X,R,Y)	(writel(Y,(u_char *)X->mmio + X->duck->base + R))
# define	nac_gettime(X)		do_gettimeofday(X)
# define	pci_get_device(X)	(X->device)

#endif


#include "nacts.h"
#define ABS(X)((uint64_t)(((X)<0ll)?-(X):(X)))
#define cshift 3


static void
duck_clear_stats(nac_duck_t *duck)
{
	struct timeval tv;

	//DN("duck_clear_stats\n");
	/* Stats */
	duck->bad_pulses = 0;
	duck->worst_freq = 0;
	duck->worst_phase = 0;
	duck->pulses = 0;
	duck->single_misses = 0;
	duck->longest_miss = 0;
	duck->sickness = 0;
	duck->resyncs = 0;
	nac_gettime(&tv);
	duck->stat_start = tv.tv_sec;
	duck->stat_end = tv.tv_sec;
}

static int
duck_set(nac_softc_t *sc, int reset)
{
	nac_duck_t * duck = sc->duck;

        /* we are in 100MHz mode, no other choice */
	duck->cfreq = 100000000 << cshift;
	duck->dds = 0xabcc7712;	
	duck->pre_dds = 0xabcc7712;
	duck->sfreq = 67108864;
	duck->width = 26;	

	// duck->base = result[0].addr; 
	// we fix duck base address at 0x60030;
	//duck->base = 0x60030 ;
	duck->base =DUCK_BASE_ADDR;
	//DD("nac%u: duck->base = 0x%08x\n", sc->unit, (uint32_t) duck->base);

	/* set the dds on the actual card */
	DUCKW(sc, dds, duck->dds);
	DUCKW(sc, cmd, Duck_Cmd_Stop);
#if 0
	if (reset) {	
		if (sc->unit == 0x1)
		{
			//slave card
			DUCKW(sc, conf, Duck_Config_Muxin_RS422);
			duck->health_threshold = 0xa00;
		}	
		else
		{
			//master card free run
			DUCKW(sc, conf, Duck_Config_Muxout_Ovrflow);
			duck->health_threshold = 0xa00;
			//duck->health_threshold = 0xc800; /* health threshold, +/- 11.92ns */
		}
	}
#endif

	DUCKW(sc, conf, Duck_Config_Muxin_Ovrflow);
	duck->health_threshold = 0xc800;

	duck->started=0;

	return 0;
}

static void
duck_start(nac_softc_t *sc)
{
	struct timeval tv;
	uint32_t start, read;

	//DN("duck_start entry...\n");
	nac_gettime(&tv);
	start = tv.tv_sec;
	if(tv.tv_usec > 500000)
		start += 1;
//DA("nac%u: starting clock at %ld.%06lu (%u)\n", sc->unit, tv.tv_sec, tv.tv_usec, start);
	
	DUCKW(sc, hload, start);
	read = DUCKR(sc, hload);

//DA("read back: %u\n", read);

	//DN("nac: duck_start %p\n", sc);
	/* Load immediately - note this is relatively coarse */
	DUCKW(sc, cmd, (Duck_Cmd_Load_Immed|Duck_Cmd_Go));
	sc->duck->started=1;
}

/* we provide another way to read timestamp values every 1s, 
     if needed use duck_tsreg_read() function */
#if 1
static void
duck_health_timeout(unsigned long data)
{
	nac_softc_t *sc = (nac_softc_t *)data;
	
	if(sc->duck->health) {
		//DA("nac%u: clock input unhealthy - unsynchronised for over 10 seconds\n", sc->unit);
		sc->duck->sickness++;
	}
	sc->duck->health = 0;
}
#endif

#if 0
static void
duck_tsreg_read(unsigned long data)
{
	nac_softc_t *sc = (nac_softc_t *)data;	
	
	uint32_t card1_high, card1_low, card2_high, card2_low, duck_dds1;
	uint64_t now;
	
	if(sc->unit == 0x1)
	{	
		// hread1 & lread1 are interrupt latched
		card1_high = DUCKR(sc, hread1);
        	card1_low  = DUCKR(sc, lread1);
		now = ((int64_t)card1_high << 32) + card1_low;
		duck_dds1 = DUCKR(sc, dds1);
		DN("Slave Card: 0x%08X%08X DDS: 0x%08X DDS1: 0x%08X\n",(uint32_t)(now>>32),(uint32_t)(now&0xffffffff),sc->duck->dds,duck_dds1);
	}
	else
	{
		card2_high = DUCKR(sc, hread1);
		card2_low  = DUCKR(sc, lread1);
		now = ((int64_t)card2_high << 32) + card2_low;
		DN("Master Card: 0x%08X%08X\n",(uint32_t)(now>>32),(uint32_t)(now&0xffffffff));
	}	
	
	sc->duck->timer->expires = jiffies + HZ;
	add_timer(sc->duck->timer);	
}	
#endif

static void
duck_init_timeout(unsigned long data)
{
	nac_softc_t *sc = (nac_softc_t *)data;
	nac_duck_t *duck = sc->duck;
	struct timeval tv;
	
	//DN("nac: duck_timeout\n");
	nac_gettime(&tv);
	if(tv.tv_usec >= (1000*1000-1000*1000/HZ)) {
		/*
		 * The next tick will be at or beyond the second
		 * wrap. Keep spin waiting rather than being late
		 * by waiting for another timer expiry.
		 */
		/* next second is now */
		duck_start(sc);
		/* from now on this slot is occupied for sickness monitoring */

#if defined(__linux__)

		duck->timer->function = duck_health_timeout;
//		duck->timer->function = duck_tsreg_read;
		duck->timer->expires = jiffies + HZ;
		add_timer(duck->timer);
		wake_up_interruptible(&duck->wait);
	} else {
		/* keep waiting */
		duck->timer->expires = jiffies + 1;
		add_timer(duck->timer);

#endif /* Platform-specific code. */
	}
}

int
duck_init(nac_softc_t *sc)
{
	nac_duck_t *duck = sc->duck;

	/* This one is to set a card after module insert, xilinx load, or reset */
	
	//DN("nac: init_duck duck: %p timer: %p\n", (void*) duck, &duck->timer);

	/* Program in config */
	if(duck_set(sc, 1))
		return -1;

	duck_clear_stats(duck);
	duck->last=0;
	duck->sync_state = -1;
	duck->health=0;
	
	/*
	 * Synchronize DUCK init as close as possible to next second "pulse".
	 */
#if defined(__linux__)

	del_timer_sync(duck->timer);	/* the health watchdog might be running */
	
	duck->timer->data = (unsigned long)sc;
	duck->timer->expires = jiffies + 1;		/* soon */
	duck->timer->function = duck_init_timeout;	/* function */
	add_timer(duck->timer);

	/*
	 * The actual initialization is called from timeout to make
	 * it as precise as possible. Just wait here until finished.
	 */
	wait_event_interruptible(duck->wait, duck->started);
	//interruptible_sleep_on(&duck->wait);
	return 0;

#endif /* Platform-specific code. */
}

void
duck_intr(nac_softc_t *sc)
{
	/* common variable */
	nac_duck_t * duck = sc->duck;
	uint8_t   syn_mode = 0;    /* 2 => Muxin_Ovrflow, 1 => Muxin_RS422, 0 => others */
	uint32_t high, low;
	/* syn to pc variable */
	int64_t now1, hostnow1, delta1, err1 = 0, offset1 = 0, temp_freq1 = 0, want1;
	struct timeval ts;
	/* our algorithm */
	uint32_t dds_new, tmp, offset;
	uint64_t now,delta;
	

	if(!duck)
		return;
	if(!duck->base)
		return;

	/* Only read the config mode at the begin */
	if (DUCKR(sc, conf) & Duck_Config_Muxin_Ovrflow)
		syn_mode = 2;
	else if (DUCKR(sc, conf) & Duck_Config_Muxin_RS422)
		syn_mode = 1;
	else
		syn_mode = 0;

	/* the following codes are executed only when synchronize to host */
	/* they are from ref code */
	if (syn_mode == 2)
	{	
		// we can use tsc for much more accuracy time 	
#if 0
		rdtscll(tsc_now);
#else
		nac_gettime(&ts);
		hostnow1 = ((int64_t)ts.tv_sec << 32) + ((int64_t)ts.tv_usec << 32) / 1000 / 1000;
		if (ts.tv_usec > 500000)
			ts.tv_sec++;
#endif
		// hread & lread are read latched, they are used when synchronize to host
		high = DUCKR(sc, hread);
		low  = DUCKR(sc, lread);
		now1  = ((uint64_t)high << 32) + low;	

		duck->stat_end = ts.tv_sec;
		duck->pulses++;
	
		delta1 = now1 - hostnow1;

	//	DA("nac%d now1: 0x%08X%08X hostnow1: 0x%08X%08X delta1: 0x%08X%08X\n",sc->unit,(uint32_t)(now1>>32),(uint32_t)(now1&0xffffffff),(uint32_t)(hostnow1>>32),(uint32_t)(hostnow1&0xffffffffll),(uint32_t)(delta1>>32),(uint32_t)(delta1&0xffffffff));

		if ((delta1 > 0xffffffffll) || (delta1 < -0xffffffffll))
		{
			//DA("nac%d: clock phase error > 1s, resetting nac clock.\n", sc->unit);
			/*DA("nac%d: clock system %d.%09u nac %d.%09u delta %d.%09ds\n",
				sc->unit,
				(uint32_t)(hostnow1>>32), (uint32_t)(((hostnow1&0xffffffff)*1000*1000*1000)>>32),
				(uint32_t)(now1>>32), (uint32_t)(((now1&0xffffffff)*1000*1000*1000)>>32),
				(int32_t)(delta1>>32), (int32_t)(((delta1&0xffffffff)*1000*1000*1000)>>32) );*/
			
			duck->resyncs++;
			duck->sickness++;
			duck->health = 0;

			/* reset duck config, but don't change mux settings or clear stats */
			duck_set(sc, 0);
			/* Start duck immediately */
			duck_start(sc);
			return ;
		}

		/* for overflow mode, synchronize nac to host pc clock */
		now1 = hostnow1;  /* this is not so accurate, as acctually the card has past 1s */
		duck->health_threshold = 0xc800;

		delta1 = (now1 - duck->last);
		if ((duck->last != 0) && (delta1 != 0))
		{
			/* delta1 wavelength within ~1% */
			if ((delta1 < 0x101000000ll) && (delta1 > 0xff000000ll))
			{
				delta1 = 0xffffffffffffffffll / delta1;

				/* err is wavelength error */
				err1 = delta1 - 0x100000000ll;
				duck->freq_error = (int32_t)err1;
				if (duck->worst_freq < ABS(err1))
					duck->worst_freq = ABS(err1);

				/* offset is phase error from second boundary */
				if ((now1 & 0xffffffffll) < 0x80000000ll)
					offset1 = now1 & 0xffffffffll;
				else	
					offset1 = 0 - (0x100000000ll - (now1 & 0xffffffffll));

				offset1 = -offset1;

				duck->phase_error = (int32_t)offset1;

				if (duck->worst_phase < ABS(offset1))
					duck->worst_phase = ABS(offset1);

				/* temp_freq is the crystal freq over the last second */
				temp_freq1 = ((delta1 << duck->width) /duck->dds);
				if (ABS(temp_freq1 - (duck->cfreq >> cshift)) < 1000000 )
				{
					duck->cfreq = (duck->cfreq - (duck->cfreq >> cshift)) + (temp_freq1);
				}
				else
				{
					//DA("nac%d: clock input - Crystal Freq Est Bad!\n", sc->unit);
					duck->bad_pulses++;
				}

				if ((offset1 < duck->health_threshold) && (offset1 > -duck->health_threshold))
				{
					if (duck->health == 0)
						//DA("nac%d: clock input healthy - synchronization successful.\n", sc->unit);
					duck->health = 1;
					mod_timer(duck->timer, jiffies + 10*HZ + HZ/2);
				}

				want1 = 0x100000000ll - (offset1 >> 1);
			}	
			else
			{
				//DA("nac%d: clock input - missing pulses.\n", sc->unit);
				want1 = 0x100000000ll;
			}

			/* calculate desired dds rate given desired freq and crystal freq */
			duck->dds = (uint32_t)(((want1 << (duck->width+cshift))/duck->cfreq));
			DUCKW(sc, dds, duck->dds);

			if ( !((offset1 < duck->health_threshold) && (offset1 > -duck->health_threshold)) ) {
			/*	DA("nac%d: clock freq err: %lldppb phase err: %lldns crystal est: %dHz\n",
			   		sc->unit,
			   		(err1*1000*1000*1000)>>32,
			   		(offset1*1000*1000*1000)>>32,
			   		duck->cfreq >> cshift);	*/					
			} 
			else 
			{
				/*DA("nac%d: clock freq err: %lldppb phase err: %lldns crystal: %lldHz smoothed: %dHz want: %lldns dds: 0x%08x\n",
			   		sc->unit,
			   		(err1*1000*1000*1000)>>32,
			   		(offset1*1000*1000*1000)>>32,
			   		temp_freq1,
			   		duck->cfreq >> cshift,
			   		(want1*1000*1000*1000)>>32,
			   		duck->dds);*/			
			}
		}
		
		duck->last = now1;
	}
	/* the following are our algorithm */
	else if (syn_mode == 1)
	{
		// hread1 & lread1 are interrupt latched
		high = DUCKR(sc, hread1);
		low = DUCKR(sc, lread1);
		now = ((uint64_t)high << 32) + low;
	
		/* the following code cope with the situation that: seconds should be incremented, but it doesn't */	
		/* New FPGA Version doesn't have this problem */
		if (now < duck->last)
			now = now + 0x100000000ll;
		else
		{
			delta = ((now - duck->last)>>6);
			/* delta < 0x80000000ll */
			if (delta < 0x2000000ll)
				now = now + 0x100000000ll;
		}	
	
		if ((now&0xffffffffll) < 0x80000000ll)
			offset = now & 0xffffffffll;
		else
			offset = ((uint32_t)0xffffffff - (now & 0xffffffff));
		offset = (offset >> 6);
	
		delta = ((now - duck->last)>>6);
		//DA("nac%d now: 0x%08X%08X delta: 0x%08X%08X offset: 0x%08X dds: 0x%08X\n",sc->unit,(uint32_t)(now>>32),(uint32_t)(now&0xffffffff),(uint32_t)(delta>>32),(uint32_t)(delta&0xffffffff), offset, duck->dds);

		//if we are the first time,do not change dds 
		if (duck->sync_state == -1) 
		{
			duck->last = now;
			duck->last_delta = delta&0x3ffffff;
			duck->sync_state = 0;
		}
		//if we are the second time, do some calculation
		else if (duck->sync_state == 0)
		{
			if (duck->last_delta < 0x2000000ll)
				dds_new = ((0x4000000ll*3 - duck->last_delta-2*delta)*duck->dds) /delta;
			else
				dds_new = ((0x4000000ll*4 - duck->last_delta - 2*delta)*duck->dds) /delta;
		
			duck->dds = dds_new;		
			DUCKW(sc, dds, duck->dds);
			duck->last = now;
			duck->last_delta = delta;
			duck->sync_state = 1;
		}
		//if we are the third time
		else if (duck->sync_state == 1)
		{	
			dds_new = (0x4000000ll*duck->pre_dds /delta) & 0xffffffff;
              		duck->dds = dds_new;
			duck->pre_dds = dds_new;
              	DUCKW(sc, dds, duck->dds);
			duck->last = now;
			duck->last_delta = delta;
			duck->sync_state = 2;
		}
		//if we are the fourth time
		else if (duck->sync_state == 2)
		{
			//this time delta can not be used;
			duck->last = now;
			duck->sync_state = 3;
		}
		// sync stage after startup
		else
		{
			// we can implement some filte here		
			tmp = (delta*duck->dds) / duck->pre_dds;
			if (tmp > 0x4000000ll)
			{
				tmp = tmp - 0x4000000ll;
				if ((now&0xffffffffll) < 0x80000000ll)
					dds_new = ((0x4000000ll - offset - tmp)*duck->pre_dds)/delta;
				else
					dds_new = ((0x4000000ll + offset - tmp)*duck->pre_dds)/delta;
			}	
			else
			{	
				tmp = 0x4000000ll - tmp;
				if ((now&0xffffffffll) < 0x80000000ll)
					dds_new = ((0x4000000ll - offset + tmp)*duck->pre_dds)/delta;
				else
					dds_new = ((0x4000000ll + offset + tmp)*duck->pre_dds)/delta;
			}
			duck->pre_dds = duck->dds;

			/* implement a dds filter */
			if(dds_new > duck->dds)
			{	
				tmp = dds_new - duck->dds;
				if (tmp > 0x00000400)
					dds_new = duck->dds + 0x400;
			}	
			else
			{	
				tmp = duck->dds - dds_new;
				if (tmp > 0x00000400)
					dds_new = duck->dds - 0x400;
			}
		
			duck->dds = dds_new;
			DUCKW(sc, dds, duck->dds);
			duck->last = now;
			duck->last_delta = delta;
			duck->sync_state = 3;	
		}

		/* update stats */
		duck->pulses ++;
	}
	
#if defined(__linux__)
	wake_up_interruptible(&duck->wait); /* Wake DAGIOCPPSWAIT */
#endif /* Platform-specific code. */
}

int
duck_ioctl(nac_duck_t *duck, duckinf_t *duckinf)
{
	if(duckinf->Set_Duck_Field&Set_Duck_Clear_Stats) {
		duck_clear_stats(duck);
		duckinf->Set_Duck_Field &= ~Set_Duck_Clear_Stats;
	}

	if(duckinf->Set_Duck_Field&Set_Duck_Health_Thresh) {
		duck->health_threshold = (duckinf->Health_Thresh>>(32-duck->width))<<(32-duck->width);
		duckinf->Set_Duck_Field &= ~Set_Duck_Health_Thresh;
	}

	duckinf->Crystal_Freq		= duck->cfreq >> cshift;
	duckinf->Synth_Freq		= duck->sfreq;
	duckinf->Last_Ticks		= duck->last;
	duckinf->Resyncs		= duck->resyncs;
	duckinf->Bad_Pulses		= duck->bad_pulses;
	duckinf->Worst_Freq_Err		= duck->worst_freq;
	duckinf->Worst_Phase_Err	= duck->worst_phase;
	duckinf->Health_Thresh		= duck->health_threshold;
	duckinf->Pulses			= duck->pulses;
	duckinf->Single_Pulses_Missing	= duck->single_misses;
	duckinf->Longest_Pulse_Missing	= duck->longest_miss;
	duckinf->Health			= duck->health;
	duckinf->Sickness		= duck->sickness;
	duckinf->Freq_Err		= duck->freq_error;
	duckinf->Phase_Err		= duck->phase_error;
	duckinf->Stat_Start		= duck->stat_start;
	duckinf->Stat_End		= duck->stat_end;
	duckinf->Last_TSC		= duck->last_tsc;

	return 0;
}

nac_duck_t*
duck_new(void)
{
	nac_duck_t *duck;

#if defined(__linux__)

	duck = kmalloc(sizeof(nac_duck_t), GFP_KERNEL);

	if (duck) {
		memset(duck, 0, sizeof(nac_duck_t));

		/* Initialise DUCK wait queue */
		init_waitqueue_head(&duck->wait);
		
		/* allocate memory for duck timer */
		duck->timer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);
		if (duck->timer)
			init_timer(duck->timer);
	}

#endif /* Platform-specific code. */

	return duck;
}

void
duck_destroy(struct nac_softc *sc)
{
#if defined(__linux__)

	/* the health watchdog might be running */
	if (sc->duck->timer) {
		del_timer_sync(sc->duck->timer);
		kfree(sc->duck->timer);
	}
	kfree(sc->duck);

#endif /* Platform-specific code. */
	
	sc->duck = NULL;
}
