
#ifndef NACAPI_H
#define NACAPI_H

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "nactypes.h"
#include "nacreg.h"
#include "nac_stream.h"


#define NUM_STREAM		8

#define KB 								(1*1024)
#define MB 								(1*1024*1024)

#define RFT_SIZE  				8*MB
#define STRATEGY_LIST_MAX 1024
#define FLOWTABLE_MAX_NUM 4194304
#define ENTRY_VALID				0x1
#define RESERVED 					0x0
#define GUARANTEED 				0x1
#define LIMITED 					0x2
#define PULSE_INTERVAL		100000000	// 1sec
#define WRITE_INTERVAL		500000		//0.5sec

#define nac_record_size   16

/*Remote Update */
#define BANK_SIZE_W (512 * 1024) // 512K words
#define BLOCK_SIZE_W (64 * 1024)
#define BLOCK_SIZE_B (128 * 1024)

#define PRER_LO 0x00     /* Low byte clock prescaler register  */	
#define PRER_HI 0x01     /* High byte clock prescaler register */	
#define CTR     0x02     /* Control register                   */	

/* ----- Write-only registers                                  */										
#define TXR     0x03     /* Transmit byte register             */	
#define CR      0x04     /* Command register                   */	
/* ----- Read-only registers                                   */										
#define RXR     0x03     /* Receive byte register              */
#define SR      0x04     /* Status register                    */
#define RD	    0x1
#define WR	    						0x0
#define NOERROR 						0x0
#define OFFLINE 						0x1
#define LINK_FAILURE  			0x2
#define AUTO_NEG_ERR 				0x3

#define SFP_MSA_E2PROM      0xA0
#define SFP_SGMII_PHY       0xAC
#define SFP_FINISAR_DIAG    0xA2

/* global varable */
uint32_t	iom_va;
#define NAC_TIMEVAL_TO_TIMESPEC(tv, ts)			\
	do 																				\
	{																					\
		(ts)->tv_sec = (tv)->tv_sec;						\
		(ts)->tv_nsec = (tv)->tv_usec * 1000;		\
	} while (0)
 
#define NAC_TIMESPEC_TO_TIMEVAL(tv, ts)			\
	do 																				\
	{																					\
		(tv)->tv_sec = (ts)->tv_sec;						\
		(tv)->tv_usec = (ts)->tv_nsec / 1000;		\
	} while (0)

/*******************************/
#define NAC_IOC_MAGIC  								'i'
#define NACIOCINFO     								_IOR(NAC_IOC_MAGIC,  2, nacinf_t)

#define MAX_DATA											(4*MB)
#define extra_window 									(8*MB)
#define TRUSTY_WINDOW									(9*KB)
#define SAFETY_WINDOW 								(128)
#define WRSAFE(FD,STR,X)    					(((X-SAFETY_WINDOW) <= 0) ? (herd[FD].stream[STR].size) : (X-SAFETY_WINDOW))
#define WD_SAFE(X, WINDOW_SIZE, SIZE) (((X) >= (WINDOW_SIZE)) ? ((X) - (WINDOW_SIZE)) : ((X) - (WINDOW_SIZE) + (SIZE)))
#define VDR_LEN(start, end, size) 		(((end - start) >= 0)? (end - start) : (end - start + size))


__inline__ unsigned int reg_read(int nacfd, unsigned int offset );
__inline__ unsigned int reg_write(int nacfd, unsigned int offset, unsigned int value );

int nac_open(char * nacname);
int nac_close(int nacfd);
int nac_clone(int nacfd, int minor);
nacinf_t* nac_info(int nacfd);

/* API functions declaration */
int capture_start(int nacfd);
int capture_stop(int nacfd);
int capture_start_stream(int nacfd, int stream_num);
int capture_stop_stream(int nacfd, int stream_num);

/****************************************************************************/
int nac_statistics_cnt(int nacfd, int port_num, statistics_cnt_t *statistics_cnt_arg);
void nac_i2c_init(int nacfd,uint8_t port);
void nac_mdio_init(int nacfd, uint8_t mac);
uint8_t nac_i2c_random_read8(int nacfd, uint8_t port, uint8_t slave_addr, uint8_t mem_addr);
void nac_i2c_sequential_read(int nacfd,uint8_t port, uint8_t slave_addr, uint8_t base_addr, uint8_t *pdata, uint8_t count);
uint16_t nac_i2c_random_read16(int nacfd,uint8_t port, uint8_t slave_addr, uint8_t mem_addr);
void nac_i2c_random_write16(int nacfd, uint8_t port, uint8_t slave_addr, uint8_t mem_addr, uint16_t data);
void nac_mac_write(int nacfd,uint8_t mac, uint16_t addr, uint32_t data);
uint32_t nac_mac_read(int nacfd,uint8_t mac, uint16_t addr);
void nac_pcs_write(int nacfd,uint8_t mac, uint8_t addr, uint32_t data);
uint32_t nac_pcs_read(int nacfd,uint8_t mac, uint8_t addr);
int nac_set_pcs_reset(int nacfd, uint8_t mac);
int nac_set_pcs_loopback(int nacfd, uint8_t mac, uint8_t  enable);
int nac_set_autoneg_enable(int nacfd, uint8_t mac, uint8_t  enable);
int nac_get_autoneg_enable_status(int nacfd, uint8_t mac);
int nac_set_phy_powerdown(int nacfd, uint8_t mac);
int nac_set_phy_isolate(int nacfd, uint8_t mac);
int nac_set_autoneg_restart(int nacfd, uint8_t mac);
int nac_get_autoneg_status(int nacfd, uint8_t mac);
int nac_get_remote_status(int nacfd, uint8_t mac);
int nac_get_link_status(int nacfd, uint8_t mac);
int nac_get_remote_partner_autoneg_status(int nacfd, uint8_t mac);
uint8_t phy_get_module_id(int nacfd, uint8_t port);
uint8_t phy_get_connector_id(int nacfd, uint8_t port);
void phy_get_vendor_id(int nacfd , uint8_t port);

/********************Remote Update API***********************/
void set_logic_num(int nacfd,uint64_t sn_logic);
void set_hardware_num(int nacfd,uint64_t no);
uint64_t read_logic_num(int nacfd);
uint64_t read_hardware_num(int nacfd);
uint16_t bit_swap(uint16_t value);
void flash_write(int nacfd, uint32_t addr, uint16_t data);
uint16_t flash_read(int nacfd, uint32_t addr);
void reconfig(int nacfd);
void set_config_register(int nacfd, uint16_t data);
void clear_stats_regs(int nacfd);
void block_unlock(int nacfd, uint8_t bank, uint8_t block); 
void bank_unlock(int nacfd, uint8_t bank);
void block_erase(int nacfd,uint8_t bank, uint8_t block);
void bank_erase(int nacfd, uint8_t bank);
void word_program(int nacfd,uint32_t addr,uint16_t data);
void buffer_program(int nacfd,uint32_t start_addr,int n,uint16_t* pdata);
void block_program(int nacfd, uint8_t bank, uint8_t block, uint32_t w_size, uint16_t* pdata);
void bank_program(int nacfd, uint8_t bank, uint32_t w_size, uint16_t* pdata);
void blank_check_command(int nacfd,uint32_t blockToCheck);
void locking_operation_command(int nacfd,uint32_t addr,lock_stat_t lock_operation);
void protection_register_program_command(int nacfd,uint32_t addr,uint16_t data);
void buffer_enhanced_factory_program_command(int nacfd,uint32_t start_addr , uint32_t count,uint16_t data_flow[]);
int 	compare_file_order(char * path1,char *path2);

/********************api for sem******************************/
int semop_p(int sem_id);
int semop_v(int sem_id);
int get_semvalue(int sem_id);
int set_semvalue(int sem_id);
/*************************bypass*******************************/
void bypass_enable_count(int nacfd);
void set_bypass_threshold(int nacfd,uint32_t cnt);
void bypass_init(int nacfd);/*void * pulse(void *unUsed);*/
int bypass_switch_on(int nacfd);
int bypass_switch_off(int nacfd);

/************************mdio api ******************************/
void ge_mdio_random_write(int nacfd, uint8_t port, uint8_t regad, uint16_t data);
uint16_t ge_mdio_random_read(int nacfd, uint8_t port, uint8_t regad);

#endif
