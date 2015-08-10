#ifndef NAC_STREAM_H
#define NAC_STREAM_H

#define RX_TIMEOUT 0x600a8

enum fwd_type{OVERLAP_FWD = 0,COPY_FWD = 1}; //host foward mode

int nac_msu_configure(int nacfd, int num_stream);
int nac_configure(int nacfd);
int nac_attach_stream(int nacfd, int stream_num, uint32_t flags, uint32_t extra_window_size);
int nac_detach_stream(int nacfd, int stream_num);

int nac_start_stream(int nacfd, int stream_num);
int nac_stop_stream(int nacfd, int stream_num);
int nac_set_stream_poll(int nacfd, int stream_num, uint32_t mindata, struct timeval * maxwait, struct timeval * poll);
uint8_t * nac_advance_stream(int nacfd, int stream_num, uint8_t ** bottom);
int nac_get_bufsize(int nacfd, int stream_num);
uint32_t nac_get_drop_cnt(int nacfd, int stream_num);
int nac_rft_start(int nacfd);
uint8_t* nac_rft_init(int nacfd);
int nac_rft_attach_stream(int nacfd);

int clear_card_buf(int nacfd,int stream_num);
int nac_tx_attach_stream(int nacfd, int stream_num, uint32_t flags, uint32_t extra_window_size);
int nac_tx_set_stream_poll(int nacfd, int stream_num, uint32_t mindata, struct timeval *maxwait, struct timeval *poll,int flags);
int nac_tx_start_stream(int nacfd,int stream_num,int flags);
int nac_tx_stop_stream(int nacfd,int stream_num,int flags);
int nac_tx_detach_stream(int nacfd,int stream_num,int flags);
uint8_t * nac_inline_advance_stream(int nacfd,int stream_num, uint8_t ** bottom);
int  nac_inline_msu_configure(int nacfd, int num_stream,int flags);
int nac_tx_stream_copy_bytes(int nacfd, int stream_num, uint8_t * orig, uint32_t size);
uint8_t* nac_tx_get_stream_space (int nacfd,int stream_num,uint32_t size);
uint8_t* nac_tx_stream_commit_bytes (int nacfd, int stream_num, uint32_t size);
uint8_t* nac_tx_memcpy(uint8_t ** orig_ptr, uint8_t * bottom,int out_port);
void nacutil_tsc32_read(uint32_t* tsc);
/* this function get the cpu clk cycles past in one ms*/
uint32_t get_cpu_tick_one_ms();

/***********************api for muti-processes*****************************************************/
int nac_parse_name(const char* name, char* buffer, int buflen, int* stream_number);
int nac_proc_init(int nacfd, int num_stream);
int nac_proc_config(int nacfd, int num_stream, int stream_num);
int nac_proc_close(int nacfd);
int nac_proc_rft_config(int nacfd,int num_stream);
int nac_link_config(int nacfd, int num_stream);

#endif

