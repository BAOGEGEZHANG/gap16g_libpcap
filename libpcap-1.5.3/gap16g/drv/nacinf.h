#ifndef NACINF_H
#define NACINF_H

typedef struct nacinf {	
#if 1
	uint32_t		id;			/* NAC device number */
	uint32_t		phy_addr;		/* PCI address of large buffer */
	uint32_t		buf_size;		/* its size */
	uint32_t		iom_size;		/* iom size */
	uint16_t		device_code;		/* PCI device ID */
#endif
#if 0 
	uint32_t		id;		/* NAC device number */
	uint8_t*		phy_addr;	/* PCI address of large buffer */
	unsigned long		buf_size;	/* its size */
	unsigned long		iom_size;	/* iom size */
	uint16_t		device_code;	/* PCI device ID */
#endif
} nacinf_t;

#define Set_Duck_Clear_Stats	0x0001
#define Set_Duck_Health_Thresh	0x0002

#ifdef _WIN64
#pragma pack (1)
#endif
typedef struct duckinf
{
	uint64_t	Last_Ticks;
	uint64_t	Last_TSC;
	time_t		Stat_Start, Stat_End;
	uint32_t	Crystal_Freq;
	uint32_t	Synth_Freq;
	uint32_t	Resyncs;
	uint32_t	Bad_Pulses;
	uint32_t	Worst_Freq_Err, Worst_Phase_Err;
	uint32_t	Health_Thresh;
	uint32_t	Pulses, Single_Pulses_Missing, Longest_Pulse_Missing;
	uint32_t	Health, Sickness;
	int32_t		Freq_Err, Phase_Err;
	uint32_t	Set_Duck_Field;
} duckinf_t;

#endif
