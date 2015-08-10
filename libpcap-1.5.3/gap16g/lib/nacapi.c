
#include "nacapi.h"

/* gloabl var */
sheep_t *herd;
//uint32_t bufsize_test;
//uint8_t *buf_map;
strategy_list_t strategy_List[1024];

__inline__ unsigned int
reg_read(int nacfd, unsigned int offset )
{
	unsigned int value=0;
	value = *(unsigned int *)(herd[nacfd].iom+offset);
//	printf("[reg_read] addr offset:0x%x, value 0x%x\n",offset,value);
	return value;
}

__inline__ unsigned int
reg_write(int nacfd, unsigned int offset, unsigned int value )
{

	*(unsigned int *)(herd[nacfd].iom + offset) = value;
//	printf("[reg_write] addr offset:0x%x, value 0x%x\n",offset,value);
	return 0;
}


/* dsm ok */
nacinf_t*
nac_info(int nacfd)
{
	if(!herd[nacfd].opened) 
	{
		errno = EBADF;
		return NULL;
	}

	return &(herd[nacfd].nacinf);
}

/* dsm ok */
int
nac_clone(int nacfd, int minor)
{
	regex_t		reg;
	regmatch_t	match;
	char		buf[16];
	char		*fmt[NACMINOR_MAX] = {
				"/dev/nac%c",		// NACMINOR_NAC 
				"/dev/nacmem%c", 	// NACMINOR_MEM 
				"/dev/naciom%c"		// NACMINOR_IOM 
			};
	int		r;

//	printf("minor = %d\n", minor);
	if(minor >= NACMINOR_MAX) 
	{
		errno = ENODEV;
		return -1;
	}
	if(regcomp(&reg, "/dev/nac(iom|mem)*[0-9]", REG_EXTENDED) != 0) 
	{
		errno = EIO; // static regex compilation failed? 
		return -1;
	}
	if((r = regexec(&reg, herd[nacfd].nacname, 1, &match, 0)) !=0) 
	{	
		errno = ENODEV;
		return -1;
	}
	(void)sprintf(buf, fmt[minor], herd[nacfd].nacname[match.rm_eo-1]);
//	printf("buf = %s\n", buf);
	regfree(&reg);

	return open(buf, O_RDWR);
}


/* dsm ok */
int
nac_open(char *nacname)
{
	int	nacfd, i;
	nacinf_t* nacfd_info;
	uint32_t value;
	int j;
	if((nacfd = open(nacname, O_RDWR|O_SYNC)) < 0)
		goto fail;

	if(herd == NULL) 
	{
		int herdsize = sysconf(_SC_OPEN_MAX) * sizeof(sheep_t);
		herd = malloc(herdsize);
		if(herd == NULL)
		{
			/* malloc() sets errno. */
			goto fail;
		}
		memset(herd, 0, herdsize);
		for( i = 0; i < sysconf(_SC_OPEN_MAX); i++)
			herd[i].naciom = -1;
	}
	
	if(nacfd >= sysconf(_SC_OPEN_MAX)) 
	{
		errno = ENFILE; /* XXX */
		fprintf(stderr, "[nac_open] internal error in %s line %u\n", __FILE__, __LINE__);
		goto fail;
	}
	/*
	 * Now fill in the herd structure
	 */
	strcpy(herd[nacfd].nacname, nacname);
	if((herd[nacfd].naciom = nac_clone(nacfd, NACMINOR_IOM)) < 0) 
	{
		/* nac_clone() sets errno. */
		fprintf(stderr, "[nac_open] nac_clone nacfd for naciom: %s\n", strerror(errno));
		goto fail;
	}

	/* Get info, we need to get the iom_size */
	if(ioctl(nacfd, NACIOCINFO, &herd[nacfd].nacinf) < 0)
	{
		/* ioctl() sets errno. */
		goto fail;
	}

	herd[nacfd].opened = 1;
	nacfd_info = nac_info(nacfd);
	
	if (NULL == nacfd_info)
	{
		/* nac_info() sets errno. */
		goto fail;
	}

	if((herd[nacfd].iom = mmap(NULL, nacfd_info->iom_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, herd[nacfd].naciom, 0)) == MAP_FAILED)
	{
		/* mmap() sets errno. */
		goto fail;
	}
/*
	for (i = 0; i < 4; i++)
	{
		nac_i2c_init(nacfd, i);  // config i2c clock prescale register

		nac_mdio_init(nacfd, i);

	}
*/
	return nacfd;

fail:
	if (nacfd > 0)
	{
		/* Zero the entry for the failed nac file descriptor. */
		memset(&herd[nacfd], 0, sizeof(sheep_t));
	}
	return -1;
}

int
nac_close(int nacfd)
{
	unsigned int tmp;
	int i=0;

	if(!herd[nacfd].opened) 
	{
		errno = EBADF;
		return -1;
	}

#if 0
	capture_stop(nacfd);

	for (i=0; i<NUM_STREAM; i++)
	clear_card_buf(nacfd, i);


#endif	
	(void)close(herd[nacfd].naciom);
	
	if (herd[nacfd].iom != NULL) 
	{
		munmap(herd[nacfd].iom, nac_info(nacfd)->iom_size);
		herd[nacfd].iom = NULL;
	}
	
	free(herd[nacfd].pbm);
	memset(&herd[nacfd], 0, sizeof(herd[nacfd]));
	herd[nacfd].naciom = -1;
	return close(nacfd);
}


/****set the time interval in the reg of TIME_INTERVAL****/
/**the unit of time is ms,we changed it with the unit 10ns**/
int
nac_set_timer_interval(int nacfd,uint32_t  time_ms)
{
	if(time_ms <= 0)
	{
		return -1;
	}
	time_ms *= 100000;
	reg_write(nacfd,TIMER_INTERVAL_ADDR, time_ms);
	return 0;
}


int 
bypass_switch_on(int nacfd)
{
	
	reg_write(nacfd,BYPASS_CRL,1);
	return 0;
}

int 
bypass_switch_off(int nacfd)
{
	reg_write(nacfd,BYPASS_CRL,2);
	return 0;
}

int 
capture_start(int nacfd)
{
	uint32_t tmp;
	tmp = reg_read(nacfd, BYPASS_EN_AND_CAP_START);
//	printf("capture start tem is 0x%x\n",tmp);
	tmp = tmp |0x1FF; //
//	printf("capture start write in is 0x%x\n",tmp);
	reg_write(nacfd, BYPASS_EN_AND_CAP_START, tmp);
	return 0;
}

int capture_start_stream(int nacfd, int stream_num)
{
	uint32_t tmp;
	tmp = reg_read(nacfd, BYPASS_EN_AND_CAP_START);
//	printf("capture start tem is 0x%x\n",tmp);
	tmp = tmp |( 0x1 << (stream_num + 1))	;
	tmp = tmp | 0x1;
//	printf("capture start write in is 0x%x\n",tmp);
	reg_write(nacfd, BYPASS_EN_AND_CAP_START, tmp);
	return 0;
}

int capture_stop_stream(int nacfd, int stream_num)
{
	uint32_t tmp;
	tmp = reg_read(nacfd, BYPASS_EN_AND_CAP_START);
//	printf("capture stop tem is 0x%x\n",tmp);
	tmp = tmp & (0xFFFFFFFF & (~ (0x1 << (stream_num + 1))));
	tmp = tmp | 0x1;
//	printf("capture stop write in is 0x%x\n",tmp);
	reg_write(nacfd, BYPASS_EN_AND_CAP_START, tmp);
	return 0;
}

int 
capture_stop(int nacfd)
{
	uint32_t tmp;
	tmp = reg_read(nacfd, BYPASS_EN_AND_CAP_START);
//	printf("capture stop tem is 0x%x\n",tmp);
	tmp = tmp &0xfffffE00;
//	printf("capture stop write in is 0x%x\n",tmp);
	reg_write(nacfd, BYPASS_EN_AND_CAP_START, tmp);
	return 0;
}

 int
nac_statistics_cnt(int nacfd, int port_num, statistics_cnt_t *statistics_cnt_arg)
{
	statistics_cnt_arg->tx_bytes=reg_read(nacfd,PORT0_TX_BYTE_HIGH+port_num*PORT_OFFSET_60);
	statistics_cnt_arg->tx_bytes=( statistics_cnt_arg->tx_bytes<<32);
	statistics_cnt_arg->tx_bytes=(statistics_cnt_arg->tx_bytes)|(reg_read(nacfd,PORT0_TX_BYTE_LOW+port_num*PORT_OFFSET_60));
	statistics_cnt_arg->rx_bytes=reg_read(nacfd,PORT0_RX_BYTE_HIGH+port_num*PORT_OFFSET_60);
	statistics_cnt_arg->rx_bytes=( statistics_cnt_arg->rx_bytes<<32);
	statistics_cnt_arg->rx_bytes=(statistics_cnt_arg->rx_bytes)|(reg_read(nacfd,PORT0_RX_BYTE_LOW+port_num*PORT_OFFSET_60));

	statistics_cnt_arg->tx_frames=reg_read(nacfd,PORT0_TX_FRAME_HIGH+port_num*PORT_OFFSET_60);
	statistics_cnt_arg->tx_frames=( statistics_cnt_arg->tx_frames<<32);
	statistics_cnt_arg->tx_frames=(statistics_cnt_arg->tx_frames)|(reg_read(nacfd,PORT0_TX_FRAME_LOW+port_num*PORT_OFFSET_60));

	statistics_cnt_arg->rx_frames=reg_read(nacfd,PORT0_RX_FRAME_HIGH+port_num*PORT_OFFSET_60);
	statistics_cnt_arg->rx_frames=( statistics_cnt_arg->rx_frames<<32);
	statistics_cnt_arg->rx_frames=(statistics_cnt_arg->rx_frames)|(reg_read(nacfd,PORT0_RX_FRAME_LOW+port_num*PORT_OFFSET_60));

	
}


/******added for the mdio 0624******/
static void
nac_wb_write(int nacfd,uint8_t port, uint8_t addr, uint8_t data)
{
	if( (addr > 4) || (port > 3) )
	{
		printf("[nac_I2C_read]: ERROR port or addr\n");
		return ;
	}
	port = (port < 2) ? (port + 2) : (port - 2);
	reg_write(nacfd,I2C_BASEADDR + port*4, 0x80000000 +  addr * 256 + data);
	usleep(100);
}

static uint8_t
nac_wb_read(int nacfd,uint8_t port, uint8_t addr)
{
	if( (addr > 4) || (port > 3) )
	{
		printf("[nac_I2C_read]: ERROR port or addr\n");
		return ;
	}
	port = (port < 2) ? (port + 2) : (port - 2);
	reg_write(nacfd,I2C_BASEADDR + port*4, addr * 256);
	usleep(100);
	return reg_read(nacfd,I2C_BASEADDR + port*4);
}

static void 
nac_wait_trans_done(int nacfd,uint8_t port)
{
	if(port > 3) 
	{
		printf("[wait_trans_done]: ERROR port\n");
		return ;
	}
	uint8_t q = 0;
	// check tip bit
	q = nac_wb_read(nacfd,port, SR);
	while(q & 0x2)
		q = nac_wb_read(nacfd,port, SR);
}
	
void 
nac_i2c_init(int nacfd,uint8_t port)
{
	if(port > 3) 
	{
		printf("[i2c_init]: ERROR port\n");
		return ;
	}
	nac_wb_write(nacfd,port, CTR,     0x0);  // disable core
	nac_wb_write(nacfd,port, PRER_LO, 0xc7); // load prescaler lo-byte
	nac_wb_write(nacfd,port, PRER_HI, 0x00); // load prescaler hi-byte
	nac_wb_write(nacfd,port, CTR,     0x80); // enable core
}

void 
nac_mdio_init(int nacfd, uint8_t mac)
{
	uint32_t result32 = 0;
	// step 1, set mdio clock
	nac_mac_write(nacfd,mac, 0x340, 0x58); 
//	printf("[0x340] 0x%x\n",nac_mac_read(nacfd,mac, 0x340));
#if 0
	// step 2, restart auto-negotiation
	pcs_write(mac, 0x0, 0x1340);

	// step 3, wait auto-negotiation completed
	result32 = pcs_read(mac, 0x1);
	while (!(result32 & 0x20))
	{
		result32 = pcs_read(mac, 0x1);
	}
	printf("mdio_init %d\n", mac);
#endif
}


uint8_t 
nac_i2c_random_read8(int nacfd, uint8_t port, uint8_t slave_addr, uint8_t mem_addr)
{
	if(port > 3) 
	{
		printf("[i2c_random_read8]: ERROR port\n");
		return ;
	}
	uint8_t result = 0;

	// drive slave address
	nac_wb_write(nacfd,port, TXR, slave_addr + WR); // present slave address, set write-bit
	nac_wb_write(nacfd,port, CR,   0x90 ); // set command (start, write)
	nac_wait_trans_done(nacfd,port);
	
	// send memory address
	nac_wb_write(nacfd,port, TXR,     mem_addr);  // present slave's memory address
	nac_wb_write(nacfd,port, CR,      0x10); // set command (write)
	nac_wait_trans_done(nacfd, port);

	// drive slave address
	nac_wb_write(nacfd,port, TXR, slave_addr + RD ); // present slave's address, set read-bit
	nac_wb_write(nacfd,port, CR,  0x90 ); // set command (start, write)
	nac_wait_trans_done(nacfd,port);

	// read data from slave
	nac_wb_write(nacfd,port, CR, 0x28); // set command (read, nack)
	nac_wait_trans_done(nacfd,port);

	result = nac_wb_read(nacfd,port, RXR);
	nac_wb_write(nacfd,port, CR, 0x40); // set command (stop)
	
	usleep(1);
	//printf("i2c_random_read8 :0x%x\n",result);	
	return result ;
}

void nac_i2c_sequential_read(int nacfd,uint8_t port, uint8_t slave_addr, uint8_t base_addr, uint8_t *pdata, uint8_t count)
{
	if(port > 3) 
	{
		printf("[i2c_sequential_read]: ERROR port\n");
		return ;
	}
	int i = 0;

	// drive slave address
	nac_wb_write(nacfd,port, TXR, slave_addr + WR); // present slave address, set write-bit
	nac_wb_write(nacfd,port, CR,   0x90 ); // set command (start, write)
	nac_wait_trans_done(nacfd,port);
	
	// send memory address
	nac_wb_write(nacfd,port, TXR,     base_addr);  // present slave's memory address
	nac_wb_write(nacfd,port, CR,      0x10); // set command (write)
	nac_wait_trans_done(nacfd,port);

	// drive slave address
	nac_wb_write(nacfd,port, TXR, slave_addr + RD ); // present slave's address, set read-bit
	nac_wb_write(nacfd,port, CR,  0x90 ); // set command (start, write)
	nac_wait_trans_done(nacfd,port);

	for(i = 0; i < count-1; i++)
	{
		// read data from slave
		nac_wb_write(nacfd,port, CR, 0x20); // set command (read, ack)
		nac_wait_trans_done(nacfd,port);
		*(pdata + i)= nac_wb_read(nacfd,port, RXR);
	}
		
	// read the last data from slave
	nac_wb_write(nacfd,port, CR, 0x28); // set command (read, nack)
	nac_wait_trans_done(nacfd,port);
	*(pdata + count - 1)= nac_wb_read(nacfd,port, RXR);

	nac_wb_write(nacfd,port, CR,0x40); // set command (stop)
	usleep(1);
}

uint16_t 
nac_i2c_random_read16(int nacfd,uint8_t port, uint8_t slave_addr, uint8_t mem_addr)
{
	if(port > 3) 
	{
		printf("[i2c_random_read16]: ERROR port\n");
		return ;
	}
	uint16_t result;
	uint8_t data[2] ;
	nac_i2c_sequential_read(nacfd,port, slave_addr, mem_addr, data, 2);
	result = data[0]*256 + data[1];
	return result;
}

void 
nac_i2c_random_write16(int nacfd, uint8_t port, uint8_t slave_addr, uint8_t mem_addr, uint16_t data)
{
	if(port > 3) 
	{
		printf("[i2c_random_write16]: ERROR port\n");
		return ;
	}
	uint8_t data_in;
	
	// drive slave address
	nac_wb_write(nacfd, port, TXR, slave_addr + WR); // present slave address, set write-bit
	nac_wb_write(nacfd, port, CR,   0x90 ); // set command (start, write)
	nac_wait_trans_done(nacfd, port);

	// send memory address
	nac_wb_write(nacfd, port, TXR,     mem_addr);  // present slave's memory address
	nac_wb_write(nacfd, port, CR,      0x10); // set command (write)
	nac_wait_trans_done(nacfd, port);

	data_in = (data & 0xff00) >> 8;
	// send memory content 1
	nac_wb_write(nacfd, port, TXR, data_in);      // present data
	nac_wb_write(nacfd, port, CR,  0x10);         // set command (write)
	nac_wait_trans_done(nacfd, port);

	data_in = (data & 0x00ff);
	// send memory content 2
	nac_wb_write(nacfd, port, TXR, data_in);      // present data
	nac_wb_write(nacfd, port, CR, 0x50);          // set command (stop, write)
	nac_wait_trans_done(nacfd, port);

	usleep(1);
}

void 
nac_mac_write(int nacfd,uint8_t mac, uint16_t addr, uint32_t data)
{
	if (addr > 1023)
	{
		printf("[mac_write]: ERROR addr\n");
		return ;
	}
	if (mac > 3)
	{
		printf("[mac_write]: ERROR mac\n");
		return;
	}

	if(mac < 2)
	{	
		reg_write(nacfd,MAC0_DATA, data);
	}else
	{	
		reg_write(nacfd,MAC1_DATA, data);
	}
	if (mac == 0)
		reg_write(nacfd,MAC0_ADDR, 0x80000000 + addr);
	else if (mac == 1)
		reg_write(nacfd,MAC0_ADDR, 0x80000000 + (1 << 10) + addr);
	else if (mac == 2)
		reg_write(nacfd,MAC1_ADDR, 0x80000000 + addr);
	else
		reg_write(nacfd,MAC1_ADDR, 0x80000000 + (1 << 10) + addr);
	
		
	usleep(100);
}

uint32_t 
nac_mac_read(int nacfd,uint8_t mac, uint16_t addr)
{
	if (addr > 1023)
	{
		printf("[mac_read]: ERROR addr\n");
		return -1;
	}
	if (mac > 3)
	{
		printf("[mac_read]: ERROR mac\n");
		return -1;
	}
	if (mac == 0)
		reg_write(nacfd,MAC0_ADDR, addr);
	else if(mac == 1)//mdio
		reg_write(nacfd,MAC0_ADDR, (1 << 10) + addr);
	else if(mac == 2)
		reg_write(nacfd,MAC1_ADDR, addr);
	else // mdio
		reg_write(nacfd,MAC1_ADDR, (1 << 10) + addr);

	usleep(100);
	if (mac < 2)
		return reg_read(nacfd,MAC0_DATA);
	else
		return reg_read(nacfd,MAC1_DATA);
} 

void 
nac_pcs_write(int nacfd,uint8_t mac, uint8_t addr, uint32_t data)
{
	if (addr > 31)
	{
		printf("[pcs_write]: ERROR addr\n");
		return ;
	}
	if (mac > 3)
	{
		printf("[pcs_write]: ERROR mac\n");
		return;
	}

	if (mac == 0) 
		mac = 1;
	else if(mac == 1)
		mac = 0;
	else if(mac == 2)
		mac = 3;
	else
		mac = 2;

	if(mac<2)
	{
		reg_write(nacfd,MAC0_DATA, data);
	}else
	{
		reg_write(nacfd,MAC1_DATA, data);
	}
	if (mac == 0)
		reg_write(nacfd,MAC0_ADDR, 0x80000000 + (1 << 5) + addr);
	else if (mac == 1)
		reg_write(nacfd,MAC0_ADDR, 0x80000000 + (1 << 10) + (2 << 5) + addr);
	else if (mac == 2)
		reg_write(nacfd,MAC1_ADDR, 0x80000000 + (1 << 5) + addr);
	else
		reg_write(nacfd,MAC1_ADDR, 0x80000000 + (1 << 10) + (2 << 5) + addr);
		
	usleep(100);
}

uint32_t 
nac_pcs_read(int nacfd,uint8_t mac, uint8_t addr)
{
	if (addr > 31)
	{
		printf("[pcs_read]: ERROR addr\n");
		return -1;
	}
	if (mac > 3)
	{
		printf("[pcs_read]: ERROR mac\n");
		return -1;
	}

	if (mac == 0) 
		mac = 1;
	else if(mac == 1)
		mac = 0;
	else if(mac == 2)
		mac = 3;
	else
		mac = 2;

	if (mac == 0)
		reg_write(nacfd,MAC0_ADDR, (1 << 5) + addr);
	else if (mac == 1)
		reg_write(nacfd,MAC0_ADDR, (1 << 10) + (2 << 5) + addr);
	else if (mac == 2)
		reg_write(nacfd,MAC1_ADDR, (1 << 5) + addr);
	else
		reg_write(nacfd,MAC1_ADDR, (1 << 10) + (2 << 5) + addr);
		
	usleep(100);
	if (mac < 2)
		return reg_read(nacfd,MAC0_DATA);
	else
		return reg_read(nacfd,MAC1_DATA);
} 

int
nac_set_pcs_reset(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_pcs_reset]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*set bit:15 with 1  */
	tmp = tmp |0x8000;
	nac_pcs_write( nacfd,  mac,  addr, tmp);
	return 0;
}
 
int
nac_set_pcs_loopback(int nacfd, uint8_t mac, uint8_t  enable)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_pcs_loopback]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	if(enable)
	{
		/*set bit:14 with 1  */
		tmp = tmp |0x4000;
	}else
	{
		/*set bit:14 with 0  */
		tmp = tmp &0xBFFF;
	}
		nac_pcs_write( nacfd,  mac,  addr, tmp);
	return 0;
}

int
nac_set_autoneg_enable(int nacfd, uint8_t mac, uint8_t  enable)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_autoneg_enable]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	//printf("[set_autoneg_enable %d] read = 0x%x\n",mac, tmp);
	if(enable)
	{
		/*set bit:12 with 1  */
		tmp = tmp |0x1000;
	}else
	{
		/*set bit:12 with 0  */
		tmp = tmp &0xEFFF;
	}
	//printf("[set_autoneg_enable %d] read = 0x%x\n",mac, tmp);
	nac_pcs_write( nacfd,  mac,  addr, tmp);
	return 0;
}

int
nac_get_autoneg_enable_status(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_autoneg_enable]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	//printf("[get_autoneg_enable_status %d] read = 0x%x\n",mac, tmp);
	tmp = tmp & 0x1000;

	if(tmp)
	{
		/*enable bit 12: 1  */
		return 0;
	}else
	{
		/*disable bit 12: 0  */
		return -1;
	}

}

int
nac_set_phy_powerdown(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_phy_powerdown]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*set bit:11 with 1  */
	tmp = tmp |0x0800;
	nac_pcs_write( nacfd,  mac,  addr, tmp);
	return 0;
}


int
nac_set_phy_isolate(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_phy_isolate]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*set bit:10 with 1  */
	tmp = tmp |0x0400;
	nac_pcs_write( nacfd,  mac,  addr, tmp);
	return 0;
}

int
nac_set_autoneg_restart(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Control Register is 0*/
	uint8_t  addr = 0;
	if (mac > 3)
	{
		printf("[set_phy_isolate]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*set bit:9 with 1  */
	tmp = tmp |0x0200;
	nac_pcs_write( nacfd,  mac,  addr, tmp);
	return 0;
}

int 
nac_get_autoneg_status(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Status Register is 1*/
	uint8_t  addr = 1;
	if (mac > 3)
	{
		printf("[get_autoneg_status]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*read bit:5   */
	tmp = tmp &0x20;
	if(tmp)
	/*auto negotiation completed*/
		return 0;
	else
	/*auto negotiation not completed yet*/
		return -1;
	
}

int 
nac_get_remote_status(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Status Register is 1*/
	uint8_t  addr = 1;
	if (mac > 3)
	{
		printf("[get_remote_status]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*read bit:4   */
	tmp = tmp &0x10;
	if(tmp)
	/*remote fault condition detetcted */
		return 0;
	else
	/*no remote fault condition detetcted yet*/
		return -1;
	
}

int 
nac_get_link_status(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Status Register is 1*/
	uint8_t  addr = 1;
	if (mac > 3)
	{
		printf("[get_link_status]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	//printf("pcs_read 0x%x\n",tmp);
	/*read bit:2   */
	tmp = tmp &0x4;
	if(tmp)
	/*PHY link is up */
		return 0;
	else
	/*PHY link is down*/
		return -1;
}


int 
nac_get_remote_partner_autoneg_status(int nacfd, uint8_t mac)
{
	uint32_t tmp;
	/*addr of Auto-Negotiation Link Partner Ability Base Register is 5*/
	uint8_t  addr = 5;
	if (mac > 3)
	{
		printf("[get_remote_partner_autoneg_status]: ERROR mac\n");
		return -1;
	}
	tmp = nac_pcs_read(nacfd,  mac, addr);
	/*read bit:13-12   */
	tmp = (tmp &0x3000)>>12;
	switch(tmp)
	{ 
		case NOERROR:
			/*NOERROR 00*/
			printf("No Error");
			return NOERROR;
		case OFFLINE:
			/*OFFLINE 01*/
			printf("Off Line");
			return OFFLINE;
		case LINK_FAILURE:
			/*LINK_FAILURE 10*/
			printf("Link Failure");
			return LINK_FAILURE;
		case AUTO_NEG_ERR:
			/*AUTO_NEG_ERR 11*/
			printf("Autoneg Error");
			return AUTO_NEG_ERR;
		default:
			return -1;

	}
}


uint8_t 
phy_get_module_id(int nacfd, uint8_t port)
{
	uint8_t tmp ;
	uint8_t addr =0;
	
	tmp = nac_i2c_random_read8( nacfd,  port, SFP_MSA_E2PROM, addr);
	switch(tmp)
	{
		case 0x0:
			printf("Unknown or unspecified ");
			break;
		case 0x1:
			printf("GBIC		       ");		
			break;
		case 0x2:
			printf("Module	     soldered  ");
			break;
		 case 0x3:
		 	printf("SFP 	               ");	
			break;
		default:
			printf("*	               ");
			break;
	}
	return tmp;

}


uint8_t 
phy_get_connector_id(int nacfd, uint8_t port)
{
	uint8_t tmp ;
	uint8_t addr =2;
	
	tmp =nac_i2c_random_read8( nacfd,  port, SFP_MSA_E2PROM, addr);
	switch(tmp)
	{
		case 0x0:
			printf("Unknown or unspecified ");
			break;
		case 0x1:
			printf("SC			");		
			break;
		case 0x2:
			printf("Fibre Channel Style 1 copper connector");
			break;
		 case 0x3:
		 	printf("Fibre Channel Style 2 copper connector");	
			break;
		 case 0x4:
		 	printf("BNC/TNC");	
			break;
		 case 0x5:
		 	printf("Fibre Channel coaxial headers");	
			break;
		 case 0x6:
		 	printf("FiberJack");	
			break;
		 case 0x7:
		 	printf("LC");	
			break;
		 case 0x8:
		 	printf("MT-RJ");	
			break;
		 case 0x9:
		 	printf("MU");	
			break;
		 case 0xA:
		 	printf("SG");	
			break;
		 case 0xB:
		 	printf("Optical pigtail");	
			break;
		 case 0x20:
		 	printf("HSSDC II");	
			break;
		 case 0x21:
		 	printf("Copper Pigtail");	
			break;
			
		default:
			break;
	}
	return tmp;

}

void
phy_get_vendor_id(int nacfd , uint8_t port)
{
	char vendor_name[17];
	int i;
	uint8_t addr =20;
	memset(vendor_name, 0 , sizeof(char)*17);
	for(i=0;i<16;i++, addr++)
	{
		vendor_name[i] = nac_i2c_random_read8( nacfd,  port, SFP_MSA_E2PROM, addr);
		//printf("vendor_name[%d]:%c\n ",i,nac_i2c_random_read8( nacfd,  port, SFP_MSA_E2PROM, addr));
	}
	vendor_name[i] = '\0';
	i = (int)vendor_name[0];
	if(i!=-1)
	{
		printf("%s", vendor_name);
	}else
	{
		printf("*        	");
	}
	//return vendor_name;
}

/*Remote Update API*/
/*added by guiyong for remote update */
void set_logic_num(int nacfd,uint64_t sn_logic)
{
	uint16_t temp = 0;	
	uint32_t block_addr = 0;
	/* set configuration register command */
	set_config_register(nacfd, 0xBDDF);
	
	/* clear status register */
	clear_stats_regs(nacfd);
	
	block_unlock(nacfd, 14, 0);
	block_erase(nacfd, 14, 0);
	block_addr = 14*BANK_SIZE_W + 0*0x10000;
	/*set logic serial num*/
	temp =(uint16_t) ( (sn_logic & 0xffff000000000000ull) >> 48);
	word_program(nacfd,block_addr,temp);
	temp =(uint16_t) ( (sn_logic & 0x0000ffff00000000ull) >> 32);
	word_program(nacfd,block_addr+1,temp);
	temp =(uint16_t) ( (sn_logic & 0x00000000ffff0000ull) >> 16);
	word_program(nacfd,block_addr+2,temp);
	temp =(uint16_t) (sn_logic & 0x000000000000ffffull) ;
	word_program(nacfd,block_addr+3,temp);
}


void set_hardware_num(int nacfd,uint64_t no)
{
	uint16_t temp = 0;	
	uint32_t block_addr = 0;
	/* set configuration register command */
	set_config_register(nacfd, 0xBDDF);
	
	/* clear status register */
	clear_stats_regs(nacfd);
	
	block_unlock(nacfd, 14, 1);
	block_erase(nacfd, 14, 1);
	block_addr = 14*BANK_SIZE_W + 1*0x10000;
	/*set logic serial num*/
	temp =(uint16_t) ( (no & 0xffff000000000000ull) >> 48);
	word_program(nacfd,block_addr,temp);
	temp =(uint16_t) ( (no & 0x0000ffff00000000ull) >> 32);
	word_program(nacfd,block_addr+1,temp);
	temp =(uint16_t) ( (no & 0x00000000ffff0000ull) >> 16);
	word_program(nacfd,block_addr+2,temp);
	temp =(uint16_t) (no & 0x000000000000ffffull) ;
	word_program(nacfd,block_addr+3,temp);
}

uint64_t 
read_logic_num(int nacfd)
{
	uint16_t temp = 0;
	uint64_t sn = 0;
	uint32_t block_addr = 0;
	
	/* set configuration register command */
	set_config_register(nacfd, 0xBDDF);
	
	/* clear status register */
	clear_stats_regs(nacfd);
	
	flash_write(nacfd,14*BANK_SIZE_W,0xff);

	block_addr = 14*BANK_SIZE_W + 0*0x10000;
	temp = flash_read(nacfd,block_addr);
	sn = (uint64_t)temp << 48;
	temp = flash_read(nacfd,block_addr+1);
	sn |= (uint64_t)temp << 32;
	temp = flash_read(nacfd,block_addr+2);
	sn |= (uint64_t)temp << 16;
	temp = flash_read(nacfd,block_addr+3);
	sn |= (uint64_t)temp;
	return sn;
}

uint64_t 
read_hardware_num(int nacfd)
{
	uint16_t temp = 0;
	uint64_t sn = 0;
	uint32_t block_addr = 0;
	
	/* set configuration register command */
	set_config_register(nacfd, 0xBDDF);
	
	/* clear status register */
	clear_stats_regs(nacfd);
	
	flash_write(nacfd,14*BANK_SIZE_W,0xff);

	block_addr = 14*BANK_SIZE_W + 1*0x10000;
	temp = flash_read(nacfd,block_addr);
	sn = (uint64_t)temp << 48;
	temp = flash_read(nacfd,block_addr + 1);
	sn |= (uint64_t)temp << 32;
	temp = flash_read(nacfd,block_addr + 2);
	sn |= (uint64_t)temp << 16;
	temp = flash_read(nacfd,block_addr + 3);
	sn |= (uint64_t)temp;
	return sn;
}


int 
nac_judge_serial_num(int nacfd)
{
	uint64_t sn = 0;
	uint8_t sn_tmp = 0;
	sn = read_logic_num(nacfd);
	printf("sn is 0x%016llx\n",sn);
	/*read the bits of 47~40*/
	sn_tmp =(uint8_t)((sn & 0x0000ff0000000000ull)>>40);
	printf("tcam type :0x%x\n",sn_tmp);
	if(sn_tmp == 1)
	{
		/*in this case, TCAM's type is 72100*/
		return 1;
	}else if(sn_tmp == 0)
	{
		/*in this case, TCAM's type is 42100*/
		return 0;
	}else
	{
		/*int this case, serial_num in flush is unnormal*/
		return -1;
	}
}


uint16_t
bit_swap(uint16_t src)
{	
	uint16_t dst = 0, i = 0;	
	for(i = 0; i < 16; i++)		
	dst |= ( (i < 8) ? ((src & (1 << i)) << (15 - i * 2)):((src & (1 << i)) >> (2 * i - 15)) );	
	return dst;
}

void 
flash_write(int nacfd, uint32_t addr, uint16_t data)
{
	uint32_t result32 = 0;
	if (addr > 0x7FFFFF)
	{
		printf("[%s]: addr is out of range(0~0x7FFFFF)\n", __FUNCTION__);
		return;
	}
	result32 = reg_read(nacfd, FLASH_ADDR);
	while(result32){
		result32 = reg_read(nacfd, FLASH_ADDR);
	}
	
	reg_write(nacfd, FLASH_DATA, data);
	reg_write(nacfd, FLASH_ADDR, 0x80000000 + addr);

}

uint16_t 
flash_read(int nacfd, uint32_t addr)
{
	uint32_t result32 = 0;
	if (addr > 0x7FFFFF)
	{
		printf("[%s]: addr is out of range(0~0x7FFFFF)\n", __FUNCTION__);
		return;
	}
	reg_write(nacfd, FLASH_ADDR, addr);
	// wait until data valid
	result32 = reg_read(nacfd, FLASH_ADDR);
	while(!(result32 & 0x1)){
		result32 = reg_read(nacfd, FLASH_ADDR);
	}

	result32 = reg_read(nacfd, FLASH_DATA);
	return (uint16_t)result32;
}

void 
reconfig(int nacfd)
{
	uint32_t result32 = 0;
	result32 = reg_read(nacfd, FLASH_ADDR);
	while(result32){
		result32 = reg_read(nacfd, FLASH_ADDR);
	}

	reg_write(nacfd, FLASH_DATA, 0xC0000000);
}

void 
set_config_register(int nacfd, uint16_t data)
{
	flash_write(nacfd, data, 0x60);
	flash_write(nacfd, data, 0x03);
}

void 
clear_stats_regs(int nacfd)
{
	/* clear status register */
	flash_write(nacfd, 0, 0x50);
}

void 
word_program(int nacfd,uint32_t addr,uint16_t data)
{
	status_reg_t stat_reg ;
	stat_reg.value = 0;
	flash_write( nacfd,  addr, 0x40);
	flash_write( nacfd,  addr, data);
	do
	{ stat_reg.value= flash_read(nacfd, addr);
	} while(stat_reg.SR_t.sr7== 0);
	
	if (stat_reg.SR_t.sr3 == 1)
	{
		printf("[%s]: VPP invalid error\n", __FUNCTION__);
		exit(1);
	}
	if (stat_reg.SR_t.sr4 == 1)
	{
		printf("[%s]: program error\n", __FUNCTION__);
		exit(2);
	}
	if (stat_reg.SR_t.sr1== 1)
	{
		printf("[%s]: program to protect block  error\n", __FUNCTION__);
		exit(3);
	}	
}

void 
blank_check_command(int nacfd,uint32_t blockToCheck)
{
	status_reg_t stat_reg ;
	stat_reg.value = 0;
	flash_write( nacfd, blockToCheck, 0XBC);
	flash_write( nacfd, blockToCheck, 0XCB);
	do
	{
		stat_reg.value= flash_read(nacfd, blockToCheck);
	}while(stat_reg.SR_t.sr7== 0);
	if ((stat_reg.SR_t.sr4 == 1)&&(stat_reg.SR_t.sr5 == 1))
	{
		printf("command sequence error\n");
	}
	if (stat_reg.SR_t.sr5== 1)
	{
		printf("blank check  error\n");
	}
}

void 
buffer_program(int nacfd,uint32_t start_addr,int n,uint16_t * pdata)
{
	if ((n < 0) || (n > 31))
	{
		printf("[%s]: n is out of range\n", __FUNCTION__);
		exit(1);
	}
	int i = 0;
	status_reg_t stat_reg ;
	stat_reg.value = 0;
	uint16_t *pd = pdata;
	uint32_t addr = start_addr;
	do
	{
		flash_write( nacfd, start_addr, 0xE8);
		stat_reg.value= flash_read(nacfd, start_addr);
	}while(stat_reg.SR_t.sr7== 0);

	flash_write(nacfd, start_addr, n);
	
	for(i = 0; i <= n; i++)
	{
		flash_write(nacfd, addr, bit_swap(*pd));
		addr++;
		pd++;
	}

	flash_write(nacfd,start_addr,0xD0);
	do
	{
		stat_reg.value= flash_read(nacfd, start_addr);
	}while(stat_reg.SR_t.sr7== 0);

	if (stat_reg.SR_t.sr3 == 1)
	{
		printf("VPP invalid error\n");
		exit(1);
	}
	if (stat_reg.SR_t.sr4 == 1)
	{
		printf("program error\n");
		exit(2);
	}
	if (stat_reg.SR_t.sr1== 1)
	{
		printf("program to protect block  error\n");
		exit(3);
	}
}

void 
block_program(int nacfd, uint8_t bank, uint8_t block, uint32_t w_size, uint16_t* pdata)
{
	if (w_size == 0)
	{
		printf("[%s]: size is error: %d\n", __FUNCTION__, w_size);
		exit(1);
	}
	uint32_t start_addr = bank * 0x80000 + block * 0x10000;
	uint32_t need_buffer = 0;
	uint32_t last_words = 0;
	uint32_t i = 0;
	
	if ( (w_size % 32) == 0 )
	{
		need_buffer = w_size / 32;
		last_words  = 32;
	}
	else
	{
		need_buffer = w_size / 32 + 1;
		last_words  = w_size % 32;
	}

	uint16_t *pd = pdata; 
	for (i = 0; i < need_buffer - 1; i++)
	{
		buffer_program(nacfd, start_addr, 31, pd);
		pd += 32;
		start_addr += 32;
	}
	buffer_program(nacfd, start_addr, last_words-1, pd);
	printf("bank %d, block %d program successful!\n", bank, block);
}

void 
bank_program(int nacfd, uint8_t bank, uint32_t w_size, uint16_t* pdata)
{
	if (w_size == 0)
	{
		printf("[%s]: size is error\n", __FUNCTION__);
		exit(1);
	}
	uint8_t need_block = 0;
	uint8_t last_block = 0;
	uint32_t last_words = 0;
	int i = 0;

	if ( (w_size % BLOCK_SIZE_W) == 0 )
	{
		need_block = w_size / BLOCK_SIZE_W;
		last_words = BLOCK_SIZE_W;
	}
	else
	{
		need_block = w_size / BLOCK_SIZE_W + 1;
		last_words = w_size % BLOCK_SIZE_W;
	}
	last_block = need_block - 1;
//	printf("[%s]: need block %d\n", __FUNCTION__, need_block);
//	printf("[%s]: last words %d\n", __FUNCTION__, last_words);
//	printf("[%s]: last block %d\n", __FUNCTION__, last_block);

	uint16_t* pd = pdata;
	for(i = 0; i < need_block -1; i++)
	{
		block_program(nacfd, bank, i, BLOCK_SIZE_W, pd);
		pd += BLOCK_SIZE_W;
	}
	block_program(nacfd, bank, last_block, last_words, pd);
}


void 
block_unlock(int nacfd, uint8_t bank, uint8_t block)
{
	uint16_t result16 = 0;
	/* block unlock */
	flash_write(nacfd, bank * 0x80000, 0x60);
	flash_write(nacfd, bank * 0x80000 + block * 0x10000, 0xD0);

	/* read block protect status */
	flash_write(nacfd, bank * 0x80000, 0x90);
	result16 = flash_read(nacfd, bank * 0x80000 + block * 0x10000+2);
	if (result16 != 0)
	{
		printf("block unlock fail\n");
		exit(1);
	}
}

void 
bank_unlock(int nacfd, uint8_t bank)
{
	int i = 0;
	for(i = 0; i < 8; i++)
		block_unlock(nacfd, bank, i);
}

void block_erase(int nacfd, uint8_t bank, uint8_t block)
{
	status_reg_t stat_reg ;
	stat_reg.value = 0;
	flash_write(nacfd, bank * 0x80000, 0x20);
	flash_write(nacfd, bank * 0x80000 + block * 0x10000, 0xD0);
	do
	{
		stat_reg.value= flash_read(nacfd, bank * 0x80000);
	}while(stat_reg.SR_t.sr7== 0);

	if (stat_reg.SR_t.sr3 == 1)
	{
		printf("[%s]: VPP invalid error\n", __FUNCTION__);
		exit(1);
	}
	if ((stat_reg.SR_t.sr4 == 1)&&(stat_reg.SR_t.sr5 == 1))
	{
		printf("[%s]: command sequence error\n", __FUNCTION__);
		exit(2);
	}
	if (stat_reg.SR_t.sr5== 1)
	{
		printf("[%s]: blank check  error\n", __FUNCTION__);
		exit(3);
	}
	if (stat_reg.SR_t.sr1== 1)
	{
		printf("[%s]: program to protect block  error\n", __FUNCTION__);
		exit(4);
	}
}

void 
bank_erase(int nacfd, uint8_t bank)
{
	int i = 0;
	for(i = 0; i < 8; i++)
	{
		block_erase(nacfd, bank, i);
		printf("bank %d, block %d erase successful!\n", bank, i);
	}
}


void 
locking_operation_command(int nacfd, uint32_t addr, lock_stat_t lock_operation)
{
	uint16_t tempt;
	tempt = 0;
	flash_write(nacfd,addr,0X60);
	if (lock_operation == LOCKED)
		flash_write(nacfd,addr,0X01);
	if (lock_operation == UNLOCKED)
		flash_write(nacfd,addr,0XD0);
	if ((lock_operation == LOCKED_AND_LOCKEDDOWN)||(lock_operation == UNLOCKED_AND_LOCKEDDOWN))
		flash_write(nacfd,addr,0X2F);
	flash_write(nacfd,addr,0X90);
	tempt = (lock_stat_t)flash_read(nacfd,addr);
	if (tempt != lock_operation)
		printf("lock operation don't match lock state, error!\n");
	flash_write(nacfd,addr,0XFF);
}

void 
protection_register_program_command(int nacfd, uint32_t addr, uint16_t data)
{
	status_reg_t stat_reg ;
	stat_reg.value = 0;
	flash_write(nacfd,addr,0XC0);
	flash_write(nacfd,addr,data);
	do
	{
		stat_reg.value= flash_read(nacfd, addr);
	}while(stat_reg.SR_t.sr7== 0);
	if (stat_reg.SR_t.sr3 == 1)
	{
		printf("VPP invalid error\n");
	}
	if (stat_reg.SR_t.sr4 == 1)
	{
		printf("program error\n");
	}
	if (stat_reg.SR_t.sr1== 1)
	{
		printf("program to protect block  error\n");
	}
}

void 
buffer_enhanced_factory_program_command(int nacfd,uint32_t start_addr, uint32_t count ,uint16_t data_flow [ ])
{
	int i ;
	uint32_t processed;
	uint32_t remainder;
	status_reg_t stat_reg ;
	stat_reg.value = 0;
	processed = 0;
	remainder = 1;
	remainder = count%32;
	if (remainder != 0)
	{
		printf("count should be the multiply of 32!\n");
		return ;
	}
	flash_write(nacfd,start_addr,0X80);
	flash_write(nacfd,start_addr,0XD0);
	do
	{
		stat_reg.value= flash_read(nacfd, start_addr);
		if (stat_reg.SR_t.sr3 == 1)
		{
			printf("VPP invalid error\n");
		}
		if (stat_reg.SR_t.sr4 == 1)
		{
			printf("program error\n");
		}
		if (stat_reg.SR_t.sr1== 1)
		{
			printf("program to protect block  error\n");
		}
	}while(stat_reg.SR_t.sr7== 1);
	for (processed = 0;processed < (count/32) ;processed++)
	{
		for (i = 0;i < 32;i++)
		{
			flash_write(nacfd,start_addr,data_flow[i]);
		}
		do
		{
			stat_reg.value= flash_read(nacfd, start_addr);
		}while(stat_reg.SR_t.sr0== 1);
	}
	//address unsure
	flash_write(nacfd,start_addr,0XFFFF);
	do
		{
			stat_reg.value= flash_read(nacfd, start_addr);
		}while(stat_reg.SR_t.sr7== 0);
	if (stat_reg.SR_t.sr3 == 1)
	{
		printf("VPP invalid error\n");
	}
	if (stat_reg.SR_t.sr4 == 1)
	{
		printf("program error\n");
	}
	if (stat_reg.SR_t.sr1== 1)
	{
		printf("program to protect block  error\n");
	}
}

int 
compare_file_order(char * path1,char *path2)
{
	FILE *fp1 = NULL;
	FILE *fp2 = NULL;
	uint16_t tempt1 = 0;
	uint16_t tempt2 = 0;
  	long int file_size1 = 0;
  	long int file_size2 = 0;
//	struct timeval tv1,tv2;
//	struct timezone tz;
//	gettimeofday(&tv1,&tz);
	fp1 = fopen(path1,"rb");
	fp2 = fopen(path2,"rb");
	if (fp1 == NULL)
	{
		printf("check usage of %s\n",path1);
	}
	if (fp2 == NULL)
	{
		printf("check usage of %s\n",path2);
	}
	fseek(fp1,0L,SEEK_END);
	file_size1 = ftell(fp1);
	fseek(fp2,0L,SEEK_END);
	file_size2 = ftell(fp2);
	rewind(fp1);
	rewind(fp2);
//	printf("The size of %s is %d\n",path1,file_size1);
//	printf("The size of %s is %d\n",path2,file_size2);
	if (file_size1 != file_size2)
	{
		printf("The sizes of two files are different!\n");
		return -1;
	}
	if ( (file_size1%2) != 0 )
	{
		printf("The size of %s isn't even! \n",path1);
		return -1;
	}
	do
	{
		//printf("current p is %d\n",ftell(fp1) );
		fread(&tempt1,sizeof(uint16_t),1,fp1);
		if (ftell(fp1) <= (file_size1 - 8))
			tempt1 = bit_swap(tempt1);
		
		fread(&tempt2,sizeof(uint16_t),1,fp2);
		if (tempt1 != tempt2)
		{
			printf("%s is different from %s\n",path1,path2);
			return -1;
		}
	}while (ftell(fp1) < file_size1);
//	gettimeofday(&tv2,&tz);
//	printf("total time costed is %ds:%dus",tv2.tv_sec-tv1.tv_sec,tv2.tv_usec-tv1.tv_usec);
//	printf("%s is the same with %s\n",path1,path2);
	fclose(fp1);
	fclose(fp2);
	return 0;
}
/*end of remote update API*/



/*operations for the sem */
int semop_p(int sem_id)
{
    int semid =  sem_id;
    struct sembuf sem_a;
    sem_a.sem_num = 0;
    sem_a.sem_op = -1;
    sem_a.sem_flg = SEM_UNDO;
    if(semop(semid,&sem_a,1) ==- 1)
    {
    	printf("P error\n");
	return 0;
    }else
    return 1;
}

/*V operation*/
int semop_v(int sem_id)
{
    int semid =  sem_id;
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = 1;
    sem_b.sem_flg = SEM_UNDO;
    if(semop(semid,&sem_b,1) == -1)
    {
    	printf("V error\n");
	return 0;
    }else
    return 1;
}

int get_semvalue(int sem_id)
{
    int semid =  sem_id;
    int res =-1;
    res = semctl(semid,0 , GETVAL);

    if(res== -1)
    {
    	perror("semctl");
	exit(EXIT_FAILURE);
    }
    return res;
}

int set_semvalue(int sem_id)
{
    int semid =  sem_id;
    union semun sem_union;
    int value =1;
    sem_union.val =value;
    if(semctl(semid,0,SETVAL,sem_union) == -1)
    {
    	printf("set value error\n");
	return 0;
    }
    return 1;
}

/***************start of bypass API*****************/
/** disbypass and enable count ,pulse interval 1s**/
void bypass_enable_count(int nacfd)
{
	reg_write(nacfd,BYPASS_CRL,2);
	
}
void set_bypass_threshold(int nacfd,uint32_t cnt)
{
	reg_write(nacfd,BYPASS_THRESHOLD,cnt);
}
/**bypass initial :bypass and disable count ,pulse interval 1s**/
void bypass_init(int nacfd)
{
	reg_write(nacfd,BYPASS_CRL,1);
	reg_write(nacfd,BYPASS_THRESHOLD,PULSE_INTERVAL);

}

/*************** end of bypass API*****************/

/****************start of mdio api****************/

/***********************************************************/
//mdio access random write
/***********************************************************/
void ge_mdio_random_write(int nacfd, uint8_t port, uint8_t regad, uint16_t data)
{
	const uint32_t ST = 1;
	const uint32_t op = 1;
	if (regad > 31)
	{
		printf("[%s]: ERROR addr\n", __FUNCTION__);
		return;
	}
	if(port > 3)
	{
		printf("[%s]: ERROR port\n", __FUNCTION__);
		return;
	}
	port = 3 - port;

	reg_write(nacfd, (MDIO_BASE_ADDR + port * 4), (ST << 28) + (op << 26) + (port << 21) + (regad << 16) + data);
	usleep(100);
}

/***********************************************************/
//mdio access random read
/***********************************************************/
uint16_t ge_mdio_random_read(int nacfd, uint8_t port, uint8_t regad)
{
	uint16_t result16 = 0;
	const uint32_t ST = 1;
	const uint32_t op = 2;
	if (regad > 31)
	{
		printf("[%s]: ERROR addr\n", __FUNCTION__);
		return ;
	}
	if(port > 3)
	{
		printf("[%s]: ERROR port\n", __FUNCTION__);
		return;
	}
	port = 3 - port;

	reg_write(nacfd, (MDIO_BASE_ADDR + port * 4), (ST << 28) + (op << 26) + (port << 21) + (regad << 16) + 0);
	usleep(100);

	result16 = (uint16_t)(reg_read(nacfd, (MDIO_BASE_ADDR + port * 4)));
	return result16;	
} 

/****************end of mdio api  ****************/

