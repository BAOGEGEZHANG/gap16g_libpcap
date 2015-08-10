#ifndef NACREG_H
#define NACREG_H


/****************************************************
		board register
		2008.1.13
*****************************************************/
/*MAC register*/
#define MAC0_CNT_REG			0x60100
#define MAC1_CNT_REG			0x60104
#define MAC2_CNT_REG			0x60108
#define MAC3_CNT_REG			0x6010C
/* newly define register */

#define RX_BYTES_LO_REG		0x60288
#define RX_BYTES_HI_REG		0x6028C
#define RX_FRAMES_LO_REG		0x60298
#define RX_FRAMES_HI_REG		0x6029C
#define RX_FCS_ERR_LO_REG		0x602A0
#define RX_FCS_ERR_HI_REG		0x602A4

#define STREAM_REG_DIFF_0x40	0x40

/*for the refresh flowtable*/ 				
#define RFT_DMA_CTL_REG  			0x50300							
#define RFT_HOST_ADDR_LO_REG 		0x50304							
#define RFT_HOST_ADDR_HI_REG 		0x50308							
#define RFT_BUF_BOTTOM_LO_REG		0x5030C							
#define RFT_BUF_BOTTOM_HI_REG		0x50310							
#define RFT_LIMIT_PTR_LO_REG 		0x50314							
#define RFT_LIMIT_PTR_HI_REG 		0x50318							
#define RFT_RECORD_PTR_LO_REG		0x50320							
#define RFT_RECORD_PTR_HI_REG		0x50324
/************flow_control************/
#define BYPASS_EN_AND_CAP_START   		  0x90028
#define TIMER_INTERVAL_ADDR            		  0x60044
#define TAG_SPEED_ADDR    	               	  0x60048
#define TAG_ID_ADDR                             	  0x6004C
//#define LUT_VALUE					 	  0x60050
//#define LUT_ADDR						  0x60054
//added 0512 for guarantee and reservation
#define GUARANTEE_LINK0						  0x6008c
#define RESERVATION_LINK0					  0x60090
#define GUARANTEE_LINK1						  0x60098
#define RESERVATION_LINK1						  0x6009c

/*************bypass***************/
#define BYPASS_CRL 				0x60058
#define BYPASS_THRESHOLD		0x6005C

/************ mdio *************/
#define MDIO_BASE_ADDR          0x600E8

/************ added for isp ******************/
#define FLASH_DATA 0x600E0
#define FLASH_ADDR 0x600E4


#define DMA_CTL_REG 		(0x80000)
#define HOST_ADDR_LO_REG 	(0x80004)
#define HOST_ADDR_HI_REG 	(0x80008)
#define BUF_BOTTOM_LO_REG 	(0x8000C)
#define BUF_BOTTOM_HI_REG 	(0x80010)
#define LIMIT_PTR_LO_REG 	(0x80014)
#define LIMIT_PTR_HI_REG 	(0x80018)
#define RECORD_PTR_LO_REG 	(0x80020)
#define RECORD_PTR_HI_REG 	(0x80024)
#define DROP_CNT_REG 		(0x80028)

#define WR_ACTION(addr)		(((addr) & 0xFF) | (1<<31))
#define ACTION_VALUE_BASE	(0x6012C)
#define NEXT_ACTION_VALUE	(0x4)
#define ACTION_ADDR		(0x6013C)
#define WR_LUT(addr)		(((addr) & 0x7FFF) | (1<<31))
#define LUT_VALUE			(0x60050)
#define LUT_ADDR			(0x60054)



/*************TX_DMA register********************/
#define TX_DMA_CTL_REG 		(0x80200)
#define TX_HOST_ADDR_LO_REG 	(0x80204)
#define TX_HOST_ADDR_HI_REG 	(0x80208)
#define TX_BUF_BOTTOM_LO_REG 	(0x8020C)
#define TX_BUF_BOTTOM_HI_REG 	(0x80210)
#define TX_LIMIT_PTR_LO_REG 	(0x80214)
#define TX_LIMIT_PTR_HI_REG 	(0x80218)
#define TX_RECORD_PTR_LO_REG 	(0x8021C)

/*statistics  cnt of ports*/
/*REG OF PORT0*/
#define PORT0_TX_BYTE_LOW                                    0x60280
#define PORT0_TX_BYTE_HIGH                                   0x60284
#define PORT0_RX_BYTE_LOW                                    0x60288
#define PORT0_RX_BYTE_HIGH                                   0x6028C
#define PORT0_TX_FRAME_LOW                                 0x60290
#define PORT0_TX_FRAME_HIGH                                0x60294
#define PORT0_RX_FRAME_LOW                                 0x60298
#define PORT0_RX_FRAME_HIGH                                0x6029C



#define PORT_OFFSET_60                                          0x60
/*REG OF PORT1*/
#define PORT1_TX_BYTE_LOW                                    0x602E0
#define PORT1_TX_BYTE_HIGH                                   0x602E4
#define PORT1_RX_BYTE_LOW                                    0x602E8
#define PORT1_RX_BYTE_HIGH                                   0x602EC
#define PORT1_TX_FRAME_LOW                                   0x602F0
#define PORT1_TX_FRAME_HIGH                                  0x602F4
#define PORT1_RX_FRAME_LOW                                   0x602F8
#define PORT1_RX_FRAME_HIGH                                  0x602FC

/****added for the mdio 0624**/
#define MAC0_DATA 0x90090
#define MAC1_DATA 0x900a0
#define MAC0_ADDR 0x90098
#define MAC1_ADDR 0x900a8
#define I2C_BASEADDR 0x600c0 
//#define STREAM_BUF_DIFF	64 * 1024 * 1024

/*********MAC FILTER ADDRESS***********/
#define MAC_UNICAST_LO		0x380
#define MAC_UNICAST_HI		0x384
#define MAC_FILTER_EN		0x390

typedef volatile union {

	struct {
#ifdef NAC_BIG_ENDIAN	
		unsigned short reserved0	: 6; /* bit 31 ~ 22 */
		unsigned short cam_data		: 10; /* bit 25 ~ 16 : cam data[10:0] */
		unsigned short reserved1	: 3; /* bit 15 ~ 13 */
		unsigned short cam_addr		: 5; /* bit 12 ~ 8 */
		unsigned short reserved2	: 6; /* bit 7 ~ 2 */
		unsigned short cam_update	: 1; /* bit 1 */
		unsigned short cam_busy		: 1; /* bit 0 */
#else
		unsigned short cam_busy		: 1; /* bit 0 */	
		unsigned short cam_update	: 1; /* bit 1 */	
		unsigned short reserved2	: 6; /* bit 7 ~ 2 */	
		unsigned short cam_addr		: 5; /* bit 12 ~ 8 */
		unsigned short reserved1	: 3; /* bit 15 ~ 13 */	
		unsigned short cam_data		: 10; /* bit 25 ~ 16 : cam data[10:0] */		
		unsigned short reserved0	: 6; /* bit 31 ~ 26 */
#endif		
		}data;	

	unsigned int value;
}cam_data3_t;

typedef volatile union {
	struct {
#ifdef NAC_BIG_ENDIAN
		unsigned short reserved0		: 15; /* bit 31 ~ 17 */
		unsigned short tag_update		: 1; /* bit 16 */
		unsigned short reserved1		: 7; /* bit 15 ~ 9 */
		unsigned short tag_addr			: 5; /* bit 8 ~ 4 */
		unsigned short tag_value		: 4; /* bit 3 ~ 0 */
#else
		unsigned short tag_value		: 4; /* bit 3 ~ 0 */
		unsigned short tag_addr			: 5; /* bit 9 ~ 5 */
		unsigned short reserved1		: 7; /* bit 15 ~ 9 */
		unsigned short tag_update		: 1; /* bit 16 */
		unsigned short reserved0		: 15; /* bit 31 ~ 17 */
#endif
		}data;
	unsigned int value;
}tag_map_t;

typedef volatile union 
{
	struct 
	{
#ifdef NAC_BIG_ENDIAN
		unsigned int reserved0			: 28;
		unsigned short dma_pause_n		: 1;
		unsigned short status_clear		: 1;
		unsigned short addr_load		: 1; 
		unsigned short burst_ready		: 1; 
#else
		unsigned short burst_ready		: 1; 
		unsigned short addr_load		: 1; 
		unsigned short status_clear		: 1;		
		unsigned short dma_pause_n		: 1;
		unsigned int reserved0			: 28; 
#endif
	}data;
	unsigned int value;
}dma_ctl_reg_t;



#endif


