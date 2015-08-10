/***************************************************************************
* file name: nac_stream.c
* author:
* modified by: jarry
* last modify date:  2010-07-08
* describe: this file is the basic lib file contains stream related functions
*           Any modification of stream should be put into this file.
***************************************************************************/

#include "nacapi.h"


/*extern varies*/
extern sheep_t *herd;
/*gloabl varies*/
static uint32_t nacapi_mindata = nac_record_size;
static struct timeval nacapi_maxwait = { 0, 0 };
static struct timespec nacapi_poll    = { 0, 1*1000*1000 };

int
nac_msu_configure(int nacfd, int num_stream)
{
    int i;
    int buf_size = 0;
    unsigned int tmp;
    dma_ctl_reg_t dma_ctl_reg;
    nacinf_t * info;
    uint32_t rft_num = num_stream;

    if ((num_stream <= 0) || (num_stream > 8) || (num_stream == 3))
    {
        printf("[nac_msu_configure]: The number of streams is wrong.\n");
        herd[nacfd].msu_config = 0;
        herd[nacfd].num_stream = 1;
        goto fail;
    }
    info = nac_info(nacfd);
    capture_stop(nacfd);

    /* buffer size of each stream */
    buf_size =(info->buf_size-RFT_SIZE)/num_stream;
    /*configure DMA for the streams*/
    for(i = 0; i < num_stream; i++)
    {
        if ( !(herd[nacfd].stream[i].pbm = (nacpbm_stream_t *) malloc( sizeof( nacpbm_stream_t ))) )
            goto fail;

        /* initialize herd */
        herd[nacfd].stream[i].pbm->dma_buf_start = HOST_ADDR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->dma_buf_end = BUF_BOTTOM_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->dma_ctl_reg = DMA_CTL_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->record_ptr = RECORD_PTR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->limit_ptr= LIMIT_PTR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->drop_cnt = DROP_CNT_REG + i * STREAM_REG_DIFF_0x40;

        herd[nacfd].stream[i].size = buf_size;

        /* config DMA: set DMA start address */
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_start, (info->phy_addr + i * buf_size));
        printf("dma start:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_start));
        /* config DMA: set DMA end address */
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_end, info->phy_addr + buf_size + i * buf_size);
        printf("dma end pointer:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_end));

        dma_ctl_reg.value = 0;
        dma_ctl_reg.data.addr_load = 1;
        dma_ctl_reg.data.status_clear = 1;
        dma_ctl_reg.data.dma_pause_n = 0;       /* dma stop: in case not stop last time */
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_ctl_reg, dma_ctl_reg.value);
    }

    /*configure DMA for the refresh of flowtable*/
    if ( !(herd[nacfd].stream[rft_num].pbm = (nacpbm_stream_t *) malloc( sizeof( nacpbm_stream_t ))) )
        goto fail;
    herd[nacfd].stream[rft_num].pbm->dma_buf_start   = RFT_HOST_ADDR_LO_REG;
    herd[nacfd].stream[rft_num].pbm->dma_buf_end     = RFT_BUF_BOTTOM_LO_REG ;
    herd[nacfd].stream[rft_num].pbm->dma_ctl_reg     = RFT_DMA_CTL_REG;
    herd[nacfd].stream[rft_num].pbm->record_ptr      = RFT_RECORD_PTR_LO_REG;
    herd[nacfd].stream[rft_num].pbm->limit_ptr   = RFT_LIMIT_PTR_LO_REG;
    herd[nacfd].stream[rft_num].size = RFT_SIZE;
    /*stop DMA*/
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.dma_pause_n = 0;       /* dma stop: in case not stop last time */
    reg_write(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_ctl_reg, dma_ctl_reg.value);
    /*addr load*/
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.addr_load = 1;
    reg_write(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

    reg_write(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_buf_start, (info->phy_addr + rft_num* buf_size));
    reg_write(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_buf_end, info->phy_addr + rft_num * buf_size+RFT_SIZE);
    printf("dma start:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_buf_start));
    printf("dma end pointer:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_buf_end));
    /*limit move to top*/
    reg_write(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->limit_ptr, (info->phy_addr + rft_num* buf_size));
    /*start DMA*/
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.dma_pause_n = 1;
    reg_write(nacfd, (unsigned int)herd[nacfd].stream[rft_num].pbm->dma_ctl_reg, dma_ctl_reg.value);


    herd[nacfd].num_stream = num_stream;
    herd[nacfd].msu_config= 1;

    return 0;

fail:

    return -1;

}


int nac_get_bufsize(int nacfd, int stream_num)
{
    return herd[nacfd].stream[stream_num].size;
}

uint32_t nac_get_drop_cnt(int nacfd, int stream_num)
{
    return reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->drop_cnt);
}

uint8_t* nac_get_next_record(int nacfd, int stream_num)
{
    stream_t *stream = &(herd[nacfd].stream[stream_num]);
    const uint8_t *buf = stream->buf;
    uint32_t rlen;
    uint8_t *next_record;
    uint8_t *top;

    if(!herd[nacfd].opened)
    {
        errno = EBADF;
        return NULL;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return NULL;
    }

    if(stream->is_first_time)
    {
        stream->offset = 0;
        stream->is_first_time = 0;
        stream->free_space = 0;
        stream->processed = 0;
    }

    next_record = (uint8_t*)buf + stream->offset;

    if((stream->free_space < nac_record_size)||
            ((stream->processed + nac_record_size) > stream->extra_window_size))
    {
        top = nac_advance_stream(nacfd, stream_num, &next_record);
        printf("top is 0x%x\n",top);
        if(top == NULL)
            return NULL;
        stream->processed = 0;
        stream->free_space = top - next_record;
        if(stream->free_space < nac_record_size)
        {
            errno = EAGAIN;
            return NULL;
        }
    }

    rlen = ntohs(((erf_record_t*)next_record)->rlen);
    printf("rlen is %d\n",rlen);
    if((stream->free_space < rlen)||
            ((stream->processed + rlen) > stream->extra_window_size))
    {
        top = nac_advance_stream(nacfd, stream_num, &next_record);
        if(top == NULL)
            return NULL;
        stream->processed = 0;
        stream->free_space = top - next_record;
        if(stream->free_space < rlen)
        {
            errno = EAGAIN;
            return NULL;
        }
    }

    /* set for next time */
    stream->offset = (uintptr_t)next_record + (uintptr_t)rlen - (uintptr_t)buf;
    stream->free_space -= rlen;
    stream->processed += rlen;

    return next_record;
}
/*api for muti-processes*/
int
nac_parse_name(const char* name, char* buffer, int buflen, int* stream_number)
{
    int device_number = 0;
    int result;
    const char* device;
    const char* stream;
    char tokbuf[16];
    assert(NULL != buffer);
    assert(NULL != name);
    assert(NULL != stream_number);
    assert(0 != buflen);

    /* Check parameters. */
    if ((NULL == name) || (NULL == buffer) || (NULL == stream_number) || (buflen < 16))
    {
        errno = EINVAL;
        return -1;
    }

    /* Initialise results. */
    buffer[0] = '\0';
    *stream_number = 0;

    /* Support specifications in the forms /dev/nac0:3, nac0:3, 0:3 */
    device = NULL;
    if ((0 != strcmp("", name)) && (0 != strlen(name)))
    {
        strncpy(tokbuf, name, 16);
        device = strtok(tokbuf, ":\n");
    }

    if ((NULL == device) || (0 == strlen(device)))
    {
        strncpy(buffer, "/dev/nac0", buflen);
        return 0;
    }

    /* 'device' contains everything up to any ':' */
    result = 0;
    if (device[0] == '/')
    {
        /* Compare against /dev/nacN */
        if (NULL == strstr(device, "/dev/nac"))
        {
            result = -1;
        }
    }
    else if (device[0] == 'd')
    {
        /* Compare against nacN */
        if (NULL == strstr(device, "nac"))
        {
            result = -1;
        }
    }

    if (0 == result)
    {
        /* Look for device number.  Handle up to two digits. */
        char device_char = device[strlen(device) - 1];

        if (isdigit(device_char))
        {
            device_number = (int) (device_char - '0');

            /* Is there a second (tens) digit? */
            if (strlen(device) >= 2)
            {
                device_char = device[strlen(device) - 2];

                if (isdigit(device_char))
                {
                    device_number += 10 * ((int) (device_char - '0'));
                }
            }


            snprintf(buffer, buflen, "/dev/nac%u", device_number);
        }
        else
        {
            result = -1;
        }
    }

    if (-1 == result)
    {
        /* Localized error handling without resorting to goto statements. */
        errno = EINVAL;
        return -1;
    }

    /* See if there was a stream number. */
    stream = strtok(NULL, " \t:\n");
    if (NULL != stream)
    {
        /* There was an explicit stream number. */
        if (isdigit(stream[0]))
        {
            *stream_number = (int) atoi(stream);
        }
    }

    return 0;
}

int nac_link_config(int nacfd, int num_stream)
{
    herd[nacfd].num_stream = num_stream;
    return 0;
}

int
nac_proc_init(int nacfd, int num_stream)
{
    int i;
    int buf_size = 0;
    unsigned int tmp;
    int rft_num = NUM_STREAM;
    dma_ctl_reg_t dma_ctl_reg;
    nacinf_t * info = nac_info(nacfd);
    if ((num_stream <= 0) || (num_stream > NUM_STREAM) || (num_stream == 3))
    {
        printf("[nac_msu_configure]: The number of streams is wrong.\n");
        herd[nacfd].msu_config = 0;
        herd[nacfd].num_stream = 1;
        return -1;
    }
    /* clear capture control register, stop capture */
    capture_stop(nacfd);

    /* clear card buffer */
//  reg_write(nacfd, CAPTURE_CTL_REG, 0x2);
    for(i=0; i<num_stream; i++)
    {
        clear_card_buf(nacfd,i);
    }
    /* buffer size of each stream */
    buf_size = ((info->buf_size)-RFT_SIZE)/num_stream;
    for(i = 0; i < num_stream; i++)
    {
        /* config DMA: set DMA start address */
        reg_write(nacfd, HOST_ADDR_LO_REG + + i * STREAM_REG_DIFF_0x40, info->phy_addr + i * buf_size);
        printf("dma buf start  pointer:\t\t0x%x\n",reg_read(nacfd, HOST_ADDR_LO_REG + i * STREAM_REG_DIFF_0x40));
        /* config DMA: set DMA end address */
        reg_write(nacfd,BUF_BOTTOM_LO_REG + + i * STREAM_REG_DIFF_0x40, info->phy_addr + buf_size + i * buf_size);
        printf("dma buf bottom pointer:\t\t0x%x\n",reg_read(nacfd, BUF_BOTTOM_LO_REG + i * STREAM_REG_DIFF_0x40));
        /* dma stop: in case not stop last time */
        dma_ctl_reg.value = 0;
        dma_ctl_reg.data.addr_load = 1;
        dma_ctl_reg.data.status_clear = 1;
        dma_ctl_reg.data.dma_pause_n = 0;
        reg_write(nacfd, (unsigned int)(DMA_CTL_REG  + i * STREAM_REG_DIFF_0x40), dma_ctl_reg.value);
    }


    /*configure DMA for refreshing flowtable*/
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.dma_pause_n = 0;       /* dma stop: in case not stop last time */
    reg_write(nacfd, RFT_DMA_CTL_REG, dma_ctl_reg.value);
    /*addr load*/
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.addr_load = 1;
    reg_write(nacfd, RFT_DMA_CTL_REG, dma_ctl_reg.value);

    reg_write(nacfd, RFT_HOST_ADDR_LO_REG, (info->phy_addr + rft_num* buf_size));
    reg_write(nacfd, RFT_BUF_BOTTOM_LO_REG, info->phy_addr + rft_num * buf_size+RFT_SIZE);
    printf("rft dma start:\t\t0x%x\n",reg_read(nacfd, RFT_HOST_ADDR_LO_REG));
    printf("rft dma end pointer:\t\t0x%x\n",reg_read(nacfd, RFT_BUF_BOTTOM_LO_REG));
    /*limit move to top*/
    reg_write(nacfd, RFT_LIMIT_PTR_LO_REG, (info->phy_addr + rft_num* buf_size));
    /*start DMA*/
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.dma_pause_n = 1;
    reg_write(nacfd, RFT_DMA_CTL_REG, dma_ctl_reg.value);


    for(i=0; i<num_stream; i++)
    {
        clear_card_buf(nacfd,i);
    }
    herd[nacfd].num_stream = num_stream;
    return 0;
}




int
nac_proc_config(int nacfd, int num_stream, int stream_num)
{
    dma_ctl_reg_t dma_ctl_reg;
    int buf_size = 0;
    nacinf_t* info = nac_info(nacfd);
    /* buffer size of each stream */
    buf_size = ((info->buf_size)-RFT_SIZE)/num_stream;
    if ( !(herd[nacfd].stream[stream_num].pbm = (nacpbm_stream_t *) malloc( sizeof( nacpbm_stream_t ))) )
        return -1;

    /* initialize herd */
    herd[nacfd].stream[stream_num].pbm->dma_buf_start = HOST_ADDR_LO_REG +stream_num * STREAM_REG_DIFF_0x40;
    herd[nacfd].stream[stream_num].pbm->dma_buf_end = BUF_BOTTOM_LO_REG + stream_num* STREAM_REG_DIFF_0x40;
    herd[nacfd].stream[stream_num].pbm->dma_ctl_reg = DMA_CTL_REG + stream_num * STREAM_REG_DIFF_0x40;
    herd[nacfd].stream[stream_num].pbm->record_ptr = RECORD_PTR_LO_REG + stream_num * STREAM_REG_DIFF_0x40;
    herd[nacfd].stream[stream_num].pbm->limit_ptr= LIMIT_PTR_LO_REG + stream_num* STREAM_REG_DIFF_0x40;
    herd[nacfd].stream[stream_num].pbm->drop_cnt = DROP_CNT_REG + stream_num* STREAM_REG_DIFF_0x40;

    herd[nacfd].stream[stream_num].size = buf_size;

    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.addr_load = 1;
    dma_ctl_reg.data.status_clear = 1;
    dma_ctl_reg.data.dma_pause_n = 0;       /* dma stop: in case not stop last time */
    reg_write(nacfd, (unsigned int)herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

    return 0;
}


int
nac_proc_rft_config(int nacfd,int num_stream)
{
    int i;
    int buf_size = 0;
    dma_ctl_reg_t dma_ctl_reg;
    uint32_t rft_num = num_stream;
    nacinf_t * info = nac_info(nacfd);
    buf_size = ((info->buf_size)-RFT_SIZE)/num_stream;
    /*configure DMA for the refresh of flowtable*/
    if ( !(herd[nacfd].stream[rft_num].pbm = (nacpbm_stream_t *) malloc( sizeof( nacpbm_stream_t ))) )
        goto fail;
    herd[nacfd].stream[rft_num].pbm->dma_buf_start   = RFT_HOST_ADDR_LO_REG;
    herd[nacfd].stream[rft_num].pbm->dma_buf_end     = RFT_BUF_BOTTOM_LO_REG ;
    herd[nacfd].stream[rft_num].pbm->dma_ctl_reg     = RFT_DMA_CTL_REG;
    herd[nacfd].stream[rft_num].pbm->record_ptr      = RFT_RECORD_PTR_LO_REG;
    herd[nacfd].stream[rft_num].pbm->limit_ptr   = RFT_LIMIT_PTR_LO_REG;
    herd[nacfd].stream[rft_num].size = RFT_SIZE;
    return 0;
fail:

    return -1;
}

int
nac_proc_close(int nacfd)
{
    nacinf_t * info = nac_info(nacfd);
    (void)close(herd[nacfd].naciom);
    if (herd[nacfd].iom != NULL)
    {
        munmap(herd[nacfd].iom, info->iom_size);
        herd[nacfd].iom = NULL;
    }
    free(herd[nacfd].pbm);
    memset(&herd[nacfd], 0, sizeof(herd[nacfd]));
    herd[nacfd].naciom = -1;
    return close(nacfd);
}
int
nac_set_stream_poll(int nacfd, int stream_num, uint32_t mindata, struct timeval *maxwait, struct timeval *poll)
{
    stream_t    *stream = &(herd[nacfd].stream[stream_num]);

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }

    stream->mindata = mindata;
    stream->maxwait = *maxwait;
    NAC_TIMEVAL_TO_TIMESPEC(poll, &stream->poll);

    return 0;
}



int
nac_inline_msu_configure(int nacfd, int num_stream,int flags)
{
    int i;
    int buf_size = 0;
    unsigned int tmp;
    dma_ctl_reg_t dma_ctl_reg;
    uint32_t rft_num = (NUM_STREAM * 2);
    nacinf_t * info;

    if ((num_stream <= 0) || (num_stream > NUM_STREAM))
    {
        printf("[nac_inlie_msu_configure]: The number of streams is wrong.\n");
        herd[nacfd].msu_config = 0;
        herd[nacfd].num_stream = 1;
        goto fail;
    }
    //overlap
    info = nac_info(nacfd);
    if (info == NULL)
        printf("buf size %d.\n", 0);
    if (OVERLAP_FWD == flags)
    {
        /* buffer size of each stream */
        buf_size = (info->buf_size)/(NUM_STREAM);
        printf("Over lap mode bufsize %dMB\n",buf_size >> 20);
    }
    //copy
    else
    {
        printf("buf size %d.\n", info->buf_size);
        buf_size = (info->buf_size)/(NUM_STREAM*2);
        printf("copy mode bufsize %dMB\n",buf_size >> 20);
    }
    printf("Capture stop.\n");
    capture_stop(nacfd);
//  reg_write(nacfd,RX_TIMEOUT,0x30d40); // initial timeout value of receive  dma engine to 1ms (clock period 200MHz)
    printf("TX stream config.\n");
    /*configure  DMA for the RX streams*/
    for(i = 0; i < num_stream; i++)
    {
        if ( !(herd[nacfd].stream[i].pbm = (nacpbm_stream_t *) malloc( sizeof( nacpbm_stream_t ))) )
            goto fail;

        /* initialize herd */
        herd[nacfd].stream[i].pbm->dma_buf_start = HOST_ADDR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->dma_buf_end = BUF_BOTTOM_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->dma_ctl_reg = DMA_CTL_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->record_ptr = RECORD_PTR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->limit_ptr= LIMIT_PTR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].pbm->drop_cnt = DROP_CNT_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i].size = buf_size;
        /*dma stop*/
        dma_ctl_reg.value = 0;
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_ctl_reg, dma_ctl_reg.value);
        usleep(100);

        /* config DMA: set DMA start address */
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_start, (info->phy_addr + i * buf_size));
        printf("rx dma start:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_start));
        /* config DMA: set DMA end address */
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_end, info->phy_addr + buf_size + i * buf_size);
        printf("rx dma end pointer:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_end));

        /*addr load, buf clear*/
        dma_ctl_reg.value = 0;
        dma_ctl_reg.data.addr_load = 1;
        dma_ctl_reg.data.status_clear = 1;
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_ctl_reg, dma_ctl_reg.value);
    }
    printf("RX stream config.\n");
    /*configure  DMA for the TX streams*/
    for(i = 0; i < num_stream; i++)
    {
        if ( !(herd[nacfd].stream[i + NUM_STREAM].pbm = (nacpbm_stream_t *) malloc( sizeof( nacpbm_stream_t ))) )
            goto fail;

        /* initialize herd */
        herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_start = TX_HOST_ADDR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_end = TX_BUF_BOTTOM_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i + NUM_STREAM].pbm->dma_ctl_reg = TX_DMA_CTL_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i + NUM_STREAM].pbm->record_ptr = TX_RECORD_PTR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i + NUM_STREAM].pbm->limit_ptr= TX_LIMIT_PTR_LO_REG + i * STREAM_REG_DIFF_0x40;
        herd[nacfd].stream[i + NUM_STREAM].size = buf_size;
        /*dma stop*/
        dma_ctl_reg.value = 0;
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_ctl_reg, dma_ctl_reg.value);
        usleep(100);
        if (OVERLAP_FWD == flags)
        {

            /* config DMA: set DMA start address */
            reg_write(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_start, (info->phy_addr + i * buf_size));
            printf("tx dma start:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_start));
            /* config DMA: set DMA end address */
            reg_write(nacfd, (unsigned int)herd[nacfd].stream[i].pbm->dma_buf_end, info->phy_addr + buf_size + i * buf_size);
            printf("tx dma end pointer:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_end));

        }
        else
        {
            /* config DMA: set DMA start address */
            reg_write(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_start, (info->phy_addr + (i + NUM_STREAM) * buf_size));
            printf("tx dma start:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_start));
            /* config DMA: set DMA end address */
            reg_write(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_end, info->phy_addr + buf_size + (i + NUM_STREAM) * buf_size);
            printf("tx dma end pointer:\t\t0x%x\n",reg_read(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_buf_end));
        }

        /*addr load, buf clear*/
        dma_ctl_reg.value = 0;
        dma_ctl_reg.data.addr_load = 1;
        dma_ctl_reg.data.status_clear = 1;
        reg_write(nacfd, (unsigned int)herd[nacfd].stream[i + NUM_STREAM].pbm->dma_ctl_reg, dma_ctl_reg.value);
    }
    //capture start
    //capture_start(nacfd);
    return 0;

fail:
    printf("[nac_inlie_msu_configure]we are exiting!\n");
    exit(-1);

}


int
clear_card_buf(int nacfd,int stream_num)
{
    uint32_t tmp;
    //int stream_num =  stream_num;
    if (stream_num >= NUM_STREAM)
    {
        fprintf(stderr,"stream_num[%d] wrong,range be (0~7)\n",stream_num);
        return -1;
    }
    tmp = reg_read(nacfd, DMA_CTL_REG + stream_num * STREAM_REG_DIFF_0x40);
    tmp = tmp |0x4;
    reg_write(nacfd,DMA_CTL_REG + stream_num * STREAM_REG_DIFF_0x40,tmp);

    return 0;
}

/* dsm ok */
int
nac_attach_stream(int nacfd, int stream_num, uint32_t flags, uint32_t extra_window_size)
{
    void *sp, *ep;
    int aux;

    void *p;
    int  buf_size = 0;
    nacpbm_stream_t *pbm=0;
    int return_val = -1;
    nacinf_t* info = nac_info(nacfd);
    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }
    /* flags currently ignored */
    flags = flags;

    // Store buf_size as an stream parameter
    buf_size = herd[nacfd].stream[stream_num].size;

    // Set stream polling parameters
    herd[nacfd].stream[stream_num].mindata = nacapi_mindata;
    herd[nacfd].stream[stream_num].maxwait = nacapi_maxwait;
    herd[nacfd].stream[stream_num].poll    = nacapi_poll;

    if (extra_window_size == 0)
    {
        extra_window_size = buf_size;
    }

    if ((int)extra_window_size > buf_size)
    {
        errno=EINVAL;
        return_val = (int) MAP_FAILED;
        goto fail;
    }

    // Do the mmap
    if((sp = mmap(NULL, buf_size + extra_window_size, PROT_READ | PROT_WRITE,
                  MAP_ANON|MAP_SHARED, -1, 0)) == MAP_FAILED)
    {
        return_val = (int) MAP_FAILED;
        goto fail;
    }

    /*
     * Now map the real buffer, 1st round.
     */

    if((p = mmap(sp, buf_size, PROT_READ | PROT_WRITE,
                 MAP_FIXED|MAP_SHARED, nacfd,  buf_size*stream_num)) == MAP_FAILED)
    {
        return_val = (int) MAP_FAILED;
        goto fail;
    }

    if((ep = mmap(sp+buf_size, extra_window_size, PROT_READ|PROT_WRITE,
                  MAP_FIXED|MAP_SHARED, nacfd, buf_size*stream_num)) == MAP_FAILED)
    {
        return_val = (int) MAP_FAILED;
        goto fail;
    }

    // In order to unmap this second mmap, we need to store the its size
    herd[nacfd].stream[stream_num].extra_window_size = extra_window_size;

    // Store the pointer to the memory hole space (stream)
    herd[nacfd].stream[stream_num].buf = p;
    printf("rx[%d] buf is 0x%x\n",stream_num,herd[nacfd].stream[stream_num].buf);
    // Set initial record offset
    herd[nacfd].stream[stream_num].offset = 0;
    //set initial transimit free space
    herd[nacfd].stream[stream_num].free_space = 0;
    //set initial  is the stream's first call
    herd[nacfd].stream[stream_num].is_first_time = 1;
    // Store stream starting physical address
    herd[nacfd].stream[stream_num].phyaddr = info->phy_addr + stream_num * buf_size;

    // If we arrive this point, all has been successful
    herd[nacfd].stream[stream_num].attached = 1;
    herd[nacfd].stream[stream_num].started  = 0;

    return 0;

fail:

    return return_val; /* Usually -1, sometimes (int) MAP_FAILED. */
}

void
nac_clear_buf(int nacfd, int stream_num)
{
    dma_ctl_reg_t dma_ctl_reg;
    uint32_t dma_ctl_reg_addr = herd[nacfd].stream[stream_num].pbm->dma_ctl_reg;

    // stop DMA first
    dma_ctl_reg.value = 0;
    reg_write(nacfd, dma_ctl_reg_addr, dma_ctl_reg.value);
    sleep(1);
    dma_ctl_reg.data.status_clear =1;
    reg_write(nacfd, dma_ctl_reg_addr, dma_ctl_reg.value);
    sleep(1);
    dma_ctl_reg.value = 0;
    reg_write(nacfd, dma_ctl_reg_addr, dma_ctl_reg.value);
}

int
nac_start_stream(int nacfd, int stream_num)
{
    dma_ctl_reg_t dma_ctl_reg;
    stream_t *stream = &(herd[nacfd].stream[stream_num]);
    int buf_size = herd[nacfd].stream[stream_num].size;
    uint32_t limit_ptr = herd[nacfd].stream[stream_num].pbm->limit_ptr;
    uint32_t phy_addr = herd[nacfd].stream[stream_num].phyaddr;
    uint32_t dma_ctl_reg_addr = herd[nacfd].stream[stream_num].pbm->dma_ctl_reg;

    if(!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }

    if(stream->started)
    {
        errno = EBUSY;
        return -1;
    }

    /* write limit pointer */
    reg_write(nacfd, (unsigned int) limit_ptr, (unsigned int) phy_addr + buf_size);
    printf("rx stream[%d] start : limit ptr is 0x%x\n",stream_num, reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->limit_ptr));
    printf("rx stream[%d] start : record ptr is 0x%x\n",stream_num, reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->record_ptr));

    /* start dma */
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.dma_pause_n = 1;
    reg_write(nacfd, dma_ctl_reg_addr, dma_ctl_reg.value);

    stream->started = 1;

    return 0;
}

int
nac_stop_stream(int nacfd, int stream_num)
{
    stream_t *stream = &(herd[nacfd].stream[stream_num]);

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }

    if(!stream->started)
    {
        errno = EINVAL;
        return -1;
    }

    dma_ctl_reg_t dma_ctl_reg;
    /* dma  pause */
    dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
    dma_ctl_reg.data.dma_pause_n = 0;
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);
    stream->started = 0;
    return 0;
}


uint8_t*
nac_advance_stream(int nacfd, int stream_num, uint8_t ** bottom)
{
    struct timeval now;
    struct timeval expire;

    stream_t *stream = &(herd[nacfd].stream[stream_num]);
    uint32_t mindata = stream->mindata;
    const uint32_t size = stream->size;
    const uint32_t phyaddr = (int) stream->phyaddr;
    uint8_t* buf = stream->buf;
    uint8_t* offset;
    uint8_t* record_ptr_addr;
    uint8_t* limit_ptr_addr;

    uint8_t* bottom_align;

    dma_ctl_reg_t dma_ctl_reg;

#define BOTTOM_POINTER (*bottom)

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return NULL;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return NULL;
    }
//  printf("nac_advance start\n");
    /* set timer */
    memset(&expire, 0, sizeof(expire));
    if (timerisset(&herd[nacfd].stream[stream_num].maxwait))
    {
        (void)gettimeofday(&now, NULL);
        expire = now;
        timeradd(&expire, &herd[nacfd].stream[stream_num].maxwait, &expire);
    }

    /* printf("--------------------------------------------------\n");*/
    if(BOTTOM_POINTER == NULL)
        BOTTOM_POINTER = buf;

    if(BOTTOM_POINTER > (buf + size))
        BOTTOM_POINTER -= size;

//  printf("[%d] buf ptr %p\n", stream_num,buf);
//  printf("1.bottom ptr %p\n", BOTTOM_POINTER-buf);
#ifdef platform_64
    //bottom_align =  (((unsigned long)BOTTOM_POINTER & 0x7ful) != 0)?(((unsigned long)BOTTOM_POINTER-0x80ul) & 0xFFFFFFFFFFFFFF80ul):BOTTOM_POINTER;
    bottom_align = (unsigned long)BOTTOM_POINTER &0xffffffffffffff80;
#else
    //bottom_align =  (((unsigned long)BOTTOM_POINTER & 0x7ful) != 0)?(((unsigned long)BOTTOM_POINTER-0x80ul) & 0xFFFFFF80ul):BOTTOM_POINTER;
    bottom_align = (unsigned long)BOTTOM_POINTER &0xffffff80;
#endif

    /* read record pointer */
    record_ptr_addr = (uint8_t*)reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->record_ptr);

    offset = (void*)((int)record_ptr_addr - phyaddr+ buf);
    /* dma  pause */
    dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
    dma_ctl_reg.data.dma_pause_n = 0;
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

//  printf("rx bottom = 0x%x\n, buf = 0x%x\n",bottom_align,buf);
    /* write limit pointer */
    limit_ptr_addr = (uint8_t*)phyaddr + WRSAFE(nacfd, stream_num, (bottom_align-buf));

    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->limit_ptr, (unsigned int)limit_ptr_addr);



    /* clear dma pause */
    dma_ctl_reg.data.dma_pause_n = 1;
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

    /* make sure >= mindata */
    while( ((uint32_t)(offset >= bottom_align) ? (uint32_t)(offset - bottom_align) :  (uint32_t)((uint8_t*)size - bottom_align + offset)) < mindata )
    {
        if (timerisset(&herd[nacfd].stream[stream_num].maxwait) && timercmp(&now, &expire, >))
            break;

        if (herd[nacfd].stream[stream_num].poll.tv_sec != 0 ||
                herd[nacfd].stream[stream_num].poll.tv_nsec != 0)
        {
            nanosleep(&herd[nacfd].stream[stream_num].poll, NULL);
        }

        record_ptr_addr = (void*) reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->record_ptr) ;
        offset = record_ptr_addr - (uint8_t*)phyaddr + buf;
        //printf("if < mindata record ptr is 0x%x\n",record_ptr_addr - phyaddr);
        if (timerisset(&herd[nacfd].stream[stream_num].maxwait))
            (void)gettimeofday(&now, NULL);
//      printf("nac_advance loop\n");
    }

//  printf("5.record ptr value: 0x%x\n", record_ptr_addr-phyaddr);
//  printf("******************************\n");
    if(offset < bottom_align)
        offset += size;

    if(offset > buf + size + stream->extra_window_size)
        offset = buf + size + stream->extra_window_size;


    // add TRUSTY_WINDOW
//    if((offset - BOTTOM_POINTER) > TRUSTY_WINDOW)
//        offset -= TRUSTY_WINDOW;

    //in case limit pointer equals record pointer
    if ((offset - bottom_align) >= SAFETY_WINDOW)
        offset -= SAFETY_WINDOW;
//  printf("[%d] top ptr 0x%x\n", stream_num,offset);
    return offset;

}



int
nac_detach_stream(int nacfd, int stream_num)
{
    int error = 0;
    dma_ctl_reg_t dma_ctl_reg;
    stream_t * stream = &(herd[nacfd].stream[stream_num]);

#if defined(_WIN32)
    ULONG BytesTransfered;
#endif /* _WIN32 */


    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if (herd[nacfd].stream[stream_num].buf != NULL)
    {


        /* dma  pause */
        dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
        dma_ctl_reg.data.dma_pause_n = 0;
        reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

        if(stream->pbm)
            free(stream->pbm);

#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__ppc__))

        munmap(stream->buf, stream->size + stream->extra_window_size);
#elif defined(_WIN32)

        /* XXX what is appropriate here? Do we need to "unmap"? */
#endif /* _WIN32 */
    }

    stream->started = 0;
    stream->attached = 0;

    return error;
}


int
nac_tx_attach_stream(int nacfd, int stream_num, uint32_t flags, uint32_t extra_window_size)
{
    void *sp, *ep;
    int aux;

    void *p;
    int  buf_size = 0;
    nacpbm_stream_t *pbm=0;
    int return_val = -1;
    nacinf_t* info = nac_info(nacfd);
    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }
    if ( stream_num < NUM_STREAM )
    {
        printf("[nac_tx_attach_stream]: The number of streams is wrong.\n");
        return -1;
    }

    // Store buf_size as an stream parameter
    buf_size = herd[nacfd].stream[stream_num].size;
    // Set stream polling parameters
    herd[nacfd].stream[stream_num].mindata = nacapi_mindata;
    herd[nacfd].stream[stream_num].maxwait = nacapi_maxwait;
    herd[nacfd].stream[stream_num].poll    = nacapi_poll;
    if (extra_window_size == 0)
    {
        extra_window_size = buf_size;
    }

    if ((int)extra_window_size > buf_size)
    {
        errno=EINVAL;
        return_val = (int) MAP_FAILED;
        goto fail;
    }
    //overlap condition
    if (OVERLAP_FWD == flags)
    {
        // In order to unmap this second mmap, we need to store the its size
        herd[nacfd].stream[stream_num].extra_window_size = extra_window_size;
        // Store stream starting physical address
        herd[nacfd].stream[stream_num].phyaddr = herd[nacfd].stream[stream_num].phyaddr;
        // Store the pointer to the memory hole space  of  receive stream
        herd[nacfd].stream[stream_num].buf = herd[nacfd].stream[stream_num].buf;
        printf("rx buf is 0x%x\n",herd[nacfd].stream[stream_num].buf);
    }
    //copy confiton or default.
    else
    {
        // Do the mmap
        if((sp = mmap(NULL, buf_size + extra_window_size, PROT_READ | PROT_WRITE,
                      MAP_ANON|MAP_SHARED, -1, 0)) == MAP_FAILED)
        {
            return_val = (int) MAP_FAILED;
            printf("tx attach failed\n");
            goto fail;
        }

        /*
         * Now map the real buffer, 1st round.
         */

        if((p = mmap(sp, buf_size, PROT_READ | PROT_WRITE,
                     MAP_FIXED|MAP_SHARED, nacfd,  buf_size*stream_num)) == MAP_FAILED)
        {
            return_val = (int) MAP_FAILED;
            printf("tx attach failed again\n");
            goto fail;
        }

        if((ep = mmap(sp+buf_size, extra_window_size, PROT_READ|PROT_WRITE,
                      MAP_FIXED|MAP_SHARED, nacfd, buf_size*stream_num)) == MAP_FAILED)
        {
            return_val = (int) MAP_FAILED;
            goto fail;
        }

        // In order to unmap this second mmap, we need to store the its size
        herd[nacfd].stream[stream_num].extra_window_size = extra_window_size;
        // Store stream starting physical address
        herd[nacfd].stream[stream_num].phyaddr = info->phy_addr + stream_num * buf_size;
        printf("[%s] tx [%d] phyaddr 0x%x\n",__FUNCTION__,stream_num,herd[nacfd].stream[stream_num].phyaddr);
        // Store the pointer to the memory hole space (stream)
        herd[nacfd].stream[stream_num].buf = p;

        printf("[%s] tx[%d] buf is 0x%x\n",__FUNCTION__,stream_num,herd[nacfd].stream[stream_num].buf);
    }
    // Set initial record offset
    herd[nacfd].stream[stream_num].offset = 0;
    //set initial transimit free space
    herd[nacfd].stream[stream_num].free_space = 0;
    //set initial  is the stream's first call
    herd[nacfd].stream[stream_num].is_first_time = 1;
    // If we arrive this point, all has been successful
    herd[nacfd].stream[stream_num].attached = 1;
    herd[nacfd].stream[stream_num].started  = 0;

    return 0;

fail:

    return return_val; /* Usually -1, sometimes (int) MAP_FAILED. */
}

/* set stream poll parameters */
int
nac_tx_set_stream_poll(int nacfd, int stream_num, uint32_t mindata, struct timeval *maxwait, struct timeval *poll,int flags)
{
    stream_t    *stream = &(herd[nacfd].stream[stream_num]);

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }
    if ( stream_num < NUM_STREAM )
    {
        printf("[nac_tx_set_stream_poll]: The number of streams is wrong.\n");
        return -1;
    }
    stream->mindata = mindata;
    stream->maxwait = *maxwait;
    NAC_TIMEVAL_TO_TIMESPEC(poll, &stream->poll);

    return 0;
}


int nac_tx_start_stream(int nacfd,int stream_num,int flags)
{
    dma_ctl_reg_t dma_ctl_reg;
    stream_t *stream = &(herd[nacfd].stream[stream_num]);
    uint32_t limit_ptr = herd[nacfd].stream[stream_num].pbm->limit_ptr;
    uint32_t phy_addr = herd[nacfd].stream[stream_num].phyaddr;
    uint32_t dma_ctl_reg_addr = herd[nacfd].stream[stream_num].pbm->dma_ctl_reg;

    if(!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }

    if(stream->started)
    {
        errno = EBUSY;
        return -1;
    }
    if ( stream_num < NUM_STREAM )
    {
        printf("[nac_tx_attach_stream]: The number of streams is wrong.\n");
        return -1;
    }
    /* care!transmit stream write limit pointer to host address. */
    reg_write(nacfd, (unsigned int) limit_ptr, (unsigned int) phy_addr);
    printf("tx stream[%d] start : limit ptr is 0x%x\n",stream_num, reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->limit_ptr));
    printf("tx stream[%d] start : record ptr is 0x%x\n",stream_num, reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->record_ptr));
    /* start dma */
    dma_ctl_reg.value = 0;
    dma_ctl_reg.data.dma_pause_n = 1;
    reg_write(nacfd, dma_ctl_reg_addr, dma_ctl_reg.value);

    stream->started = 1;

    return 0;
}

int nac_tx_stop_stream(int nacfd,int stream_num,int flags)
{
    stream_t *stream = &(herd[nacfd].stream[stream_num]);
    unsigned int rcd ,limit;

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }

    if(!stream->started)
    {
        errno = EINVAL;
        return -1;
    }
    if ( stream_num < NUM_STREAM )
    {
        printf("[nac_tx_stop_stream]: The number of streams is wrong.\n");
        return -1;
    }
    rcd = reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->record_ptr);
    limit = reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->limit_ptr);
    while (rcd != limit)
    {
        printf("we are waiting transmit stream[%d] to stop\n",stream_num);
        rcd = reg_read(nacfd,herd[nacfd].stream[stream_num].pbm->record_ptr);
        printf("rcd is %x , limit is %x", rcd ,limit );
        sleep(1);
    }
    dma_ctl_reg_t dma_ctl_reg;
    /* dma  pause */
    dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
    dma_ctl_reg.data.dma_pause_n = 0;
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);
    stream->started = 0;
    return 0;


}

int nac_tx_detach_stream(int nacfd,int stream_num,int flags)
{
    int error = 0;
    dma_ctl_reg_t dma_ctl_reg;
    stream_t * stream = &(herd[nacfd].stream[stream_num]);
    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }
    if ( stream_num < NUM_STREAM )
    {
        printf("[nac_tx_detach_stream]: The number of streams is wrong.\n");
        return -1;
    }

    if (herd[nacfd].stream[stream_num].buf != NULL)
    {
        /* dma  pause */
        dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
        dma_ctl_reg.data.dma_pause_n = 0;
        reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

        if(stream->pbm)
            free(stream->pbm);
        //copy condition ,we should munmap the buffer of transmit stream.
        if (OVERLAP_FWD != flags)
            munmap(stream->buf, stream->size + stream->extra_window_size);
    }

    stream->started = 0;
    stream->attached = 0;

    return error;
}

/*****************************************************************************/
/* This function is intended for inline forwarding of packets.It does the same as             */
/*nac_advance_stream  but also keep track of the recorder pointer of transmit stream.         */
/* Function returns the pointer where received packet start.                                          */
/*****************************************************************************/
uint8_t * nac_inline_advance_stream(int nacfd,int stream_num, uint8_t ** bottom)
{
    struct timeval now;
    struct timeval expire;

    stream_t *stream = &(herd[nacfd].stream[stream_num]);
    stream_t *tx_stream = &(herd[nacfd].stream[stream_num + NUM_STREAM]);
    uint32_t mindata = stream->mindata;
    const uint32_t size = stream->size;
    const uint32_t phyaddr = (int) stream->phyaddr;
    uint8_t* buf = stream->buf;
    uint8_t* offset;
    uint8_t* record_ptr_addr;
    uint8_t* limit_ptr_addr;
    uint8_t* tx_recorder_align;
    dma_ctl_reg_t dma_ctl_reg;
    unsigned int tx_record_phy_addr = reg_read(nacfd,tx_stream->pbm->record_ptr);
    uint8_t* tx_record_ptr_addr;
#define BOTTOM_POINTER (*bottom)

    if (stream_num >= NUM_STREAM)
    {
        fprintf(stderr,"stream_num[%d] wrong,range be (0~7)\n",stream_num);
        return NULL;
    }

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return NULL;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return NULL;
    }
    tx_record_ptr_addr = (uint8_t*)(tx_record_phy_addr - phyaddr+ buf);
    /* set timer */
    memset(&expire, 0, sizeof(expire));
    if (timerisset(&herd[nacfd].stream[stream_num].maxwait))
    {
        (void)gettimeofday(&now, NULL);
        expire = now;
        timeradd(&expire, &herd[nacfd].stream[stream_num].maxwait, &expire);
    }

    /* printf("--------------------------------------------------\n");*/
    if(BOTTOM_POINTER == NULL)
        BOTTOM_POINTER = buf;

    if(BOTTOM_POINTER > (buf + size))
        BOTTOM_POINTER -= size;

//  printf("[%d] buf ptr %p\n", stream_num,buf);
//  printf("1.bottom ptr %p\n", tx_recorder_align-buf);
#ifdef platform_64
    tx_recorder_align = (unsigned long)tx_record_ptr_addr &0xffffffffffffff80;
#else
    tx_recorder_align = (unsigned long)tx_record_ptr_addr &0xffffff80;
#endif
//  printf("2. tx_recorder_align is %p\n",tx_recorder_align-buf );

    /* read record pointer */
    record_ptr_addr = (uint8_t*)reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->record_ptr);

    offset = (uint8_t*)((int)record_ptr_addr - phyaddr+ buf);
//  printf("3.record ptr is 0x%x\n",record_ptr_addr-phyaddr );
    /* dma  pause */
    dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
    dma_ctl_reg.data.dma_pause_n = 0;
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

    /* write limit pointer */
    limit_ptr_addr = (uint8_t*)phyaddr + WRSAFE(nacfd, stream_num, (tx_recorder_align-buf));

    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->limit_ptr, (unsigned int)limit_ptr_addr);
//  printf("4.limit ptr value: 0x%x\n", limit_ptr_addr-phyaddr);

    /* clear dma pause */
    dma_ctl_reg.data.dma_pause_n = 1;
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

    /* make sure >= mindata */
    while( ((uint32_t)(offset >= BOTTOM_POINTER) ? (uint32_t)(offset - BOTTOM_POINTER) :  (uint32_t)((uint8_t*)size - BOTTOM_POINTER + offset)) < mindata )
    {
        if (timerisset(&herd[nacfd].stream[stream_num].maxwait) && timercmp(&now, &expire, >))
            break;

        if (herd[nacfd].stream[stream_num].poll.tv_sec != 0 ||
                herd[nacfd].stream[stream_num].poll.tv_nsec != 0)
        {
            nanosleep(&herd[nacfd].stream[stream_num].poll, NULL);
        }

        record_ptr_addr = (void*) reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->record_ptr) ;
        offset = record_ptr_addr - (uint8_t*)phyaddr + buf;
        //printf("if < mindata record ptr is 0x%x\n",record_ptr_addr - phyaddr);
        if (timerisset(&herd[nacfd].stream[stream_num].maxwait))
            (void)gettimeofday(&now, NULL);
    }

//  printf("5.record ptr value: 0x%x\n", record_ptr_addr-phyaddr);
//  printf("******************************\n");
    if(offset < BOTTOM_POINTER)
        offset += size;

    if(offset > buf + size + stream->extra_window_size)
        offset = buf + size + stream->extra_window_size;

//  printf("[%d] top ptr 0x%x\n", stream_num,offset);
    return offset;

}

/*****************************************************************************/
/* COPY 'size' bytes from 'orig' to 'stream_num' in 'nacfd' for tx.          */
/* This is not a zero-copy function, so it should be avoided when possible.  */
/* If the copy overpasses the buffer limit, split in two copies (transparent)*/
/* Function returns the number of bytes written.                             */
/*****************************************************************************/
int
nac_tx_stream_copy_bytes(int nacfd, int stream_num, uint8_t * orig, uint32_t size)
{
    int remaining_block;        /* bytes remaining to the end of buf */
    stream_t * stream = &(herd[nacfd].stream[stream_num]);
    volatile nacpbm_stream_t *pbm = stream->pbm;
    erf_header_t* erf_header;
    int written;
    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return -1;
    }

    if (size > herd[nacfd].stream[stream_num].size)
    {
        errno = EINVAL;
        return -1;
    }
    // Is the stream a read stream or a write stream?
    // We need a write stream
    // 0~(NUM_STREAM-1)  numbers are read stream
    // NUM_STREAM~(NUM_STREAM*2-1)  numbers are write streams

    if ((stream_num  < NUM_STREAM) || (stream_num  > (NUM_STREAM*2 - 1)))
    {
        errno = ENOTTY;
        return -1;
    }
    //set orig  to read stream's start addr when first time;
    if (orig == NULL)
    {
        orig = herd[nacfd].stream[stream_num - NUM_STREAM].buf;
    }

    // Is there enougth space to commit that amount of bytes?
    // Wait until space is available.
    while (stream->free_space < (int32_t)size)
    {
        stream->last_bus_top = reg_read(nacfd,pbm->record_ptr);
        stream->last_bus_bottom = reg_read(nacfd,pbm->limit_ptr);
        stream->free_space   =  stream->last_bus_top - (stream->last_bus_bottom + SAFETY_WINDOW );
        if (stream->free_space < 0 )
        {
            stream->free_space += stream->size;
        }
        nanosleep(&(stream->poll),NULL);
    }
    written = size;

    // Decrease free space
    stream->free_space -= written;

    // Can we do only one memcopy or have to split in two?
    remaining_block = stream->size - stream->offset;
    if (remaining_block >= written)
    {
        // Just one memcpy
        memcpy (stream->buf + stream->offset, orig, written);
    }
    else
    {
        // Two memcpy's
        memcpy (stream->buf + stream->offset, orig, remaining_block);
        memcpy (stream->buf, (uint8_t *)orig + remaining_block, written - remaining_block);
    }

    // Advance offset in the number of bytes to commit
    stream->offset += written;

    // Keep offset inside buffer boundaries
    if (stream->offset >= stream->size)
        stream->offset -= stream->size;

    // Advance pointer in Burst Manager (PBM)
    // We keep offset inside buffer boundaries, so it is not
    // necessary to make another check
    stream->last_bus_bottom = stream->phyaddr + stream->offset;
    reg_write(nacfd,pbm->limit_ptr,stream->last_bus_bottom);

    // All OK
    return written;
}


/*****************************************************************************/
/* Waits for the transmit stream to have at least 'size' bytes free to write */
/* in. It returns a pointer where the user can write his ERF records. After  */
/* writing the records, they must still be committed before they will be sent*/
/*****************************************************************************/
uint8_t*
nac_tx_get_stream_space (int nacfd,int stream_num,uint32_t size)
{
    stream_t * stream = &(herd[nacfd].stream[stream_num]);
    volatile nacpbm_stream_t * pbm = stream->pbm;   /* pointer to bus manager            */

    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return NULL;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return NULL;
    }

    if (size == 0)
    {
        errno = EINVAL;
        return NULL;
    }
    // Is the stream a read stream or a write stream?
    // We need a write stream
    // 0~(NUM_STREAM-1)  numbers are read stream
    // NUM_STREAM~(NUM_STREAM*2-1)  numbers are write streams
    if ((stream_num  < NUM_STREAM) || (stream_num  > (NUM_STREAM*2 - 1)))
    {
        errno = ENOTTY;
        return NULL;
    }
    // Wait for free space to write
    stream->last_bus_bottom = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->limit_ptr);
    while (stream->free_space <((int32_t)size+2048))
    {
        stream->last_bus_top = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->record_ptr);
        stream->free_space =  stream->last_bus_top - stream->last_bus_bottom;
        if (stream->free_space <= 0 )
        {
            stream->free_space += stream->size;
        }
        nanosleep(&stream->poll, NULL);
//      printf("[%s] record is 0x%x\n",__FUNCTION__,stream->last_bus_top);

    }

    // printf("[%s] limit = 0x%x, record= 0x%x\n", __FUNCTION__,stream->last_bus_bottom,stream->last_bus_top);

    // Decrease free space available
    stream->free_space -= size;

    /*  2013.9.14 remove this because commit bytes can also change this offset
        stream->offset = stream->last_bus_bottom - stream->phyaddr;
        if(stream->offset >= stream->size)
            stream->offset -= stream->size;
    */
    // Return pointer where the program can start writing
    return (stream->buf + stream->offset);
}


/*****************************************************************************/
/* This function 'commits' a certain amount of bytes into the stream buffer. */
/* The normal use of this function is after writing a record in the stream,  */
/* so we need to tell the card how many bytes we have written.               */
/* The function returns a pointer to the next available position to write    */
/* in the stream buffer                                                      */
/* Attention: the number of bytes you commit cannot overpass the extra       */
/* window you used in 'dag_attach'                                           */
/*****************************************************************************/
uint8_t*
nac_tx_stream_commit_bytes (int nacfd, int stream_num, uint32_t size)
{
    stream_t * stream = &(herd[nacfd].stream[stream_num]);
    volatile nacpbm_stream_t * pbm = stream->pbm;
    dma_ctl_reg_t dma_ctl_reg;
    int i=0;
#if 0
    for(i=0; i<size; i++)
    {
        if(i%16==0)
            printf("%02d  ",i/16);
        printf("%02x ",*(uint8_t *)(stream->buf + stream->offset+i));
        if(i%16==15)
            printf("\n");
    }
#endif
    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return NULL;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return NULL;
    }

    if (size > herd[nacfd].stream[stream_num].size)
    {
        errno = EINVAL;
        return NULL;
    }
    // Is the stream a read stream or a write stream?
    // We need a write stream
    // 0~(NUM_STREAM-1)  numbers are read stream
    // NUM_STREAM~(NUM_STREAM*2-1)  numbers are write streams
    if ((stream_num  < NUM_STREAM) || (stream_num  > (NUM_STREAM*2 - 1)))
    {
        errno = ENOTTY;
        return NULL;
    }
    // Advance offset in the number of bytes to commit
    stream->offset += size;


    // Keep offset inside buffer boundaries
    if (stream->offset > stream->size)
        stream->offset -= stream->size;
    // Advance pointer in Burst Manager (PBM)
    // We keep offset inside buffer boundaries, so it is not
    // necessary to make another check
    // Move the limit pointer at the end of the record (always)

    stream->last_bus_bottom = stream->phyaddr+ stream->offset;
    /* dma  pause */
//  dma_ctl_reg.value = reg_read(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg);
//  dma_ctl_reg.data.dma_pause_n = 0;
//  reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);


    /* write limit pointer */
    reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->limit_ptr, stream->last_bus_bottom);

//  fprintf(stdout,"[%s] set limit = 0x%x  last record = 0x%x \n",__FUNCTION__,stream->last_bus_bottom,stream->last_bus_top);
//  fflush(stdout);

    /* clear dma pause */
//  dma_ctl_reg.data.dma_pause_n = 1;
//  reg_write(nacfd, herd[nacfd].stream[stream_num].pbm->dma_ctl_reg, dma_ctl_reg.value);

//  printf("[%s] set limit to 0x%x\n",__FUNCTION__,stream->last_bus_bottom);
    // All OK
    return (stream->buf + stream->offset);
}

/*****************************************************************************/
/* This function 'copy' a packet  into the transmit stream buffer with.*/
/*  the stream_num given by user. It return the pointer to new free space of transmit stream buffer*/

/*****************************************************************************/
uint8_t* nac_tx_memcpy(uint8_t ** orig_ptr, uint8_t * bottom,int out_port)
{
    uint8_t *p = bottom;
#define cp_org  (*orig_ptr)
    uint16_t rlen;
    erf_record_t * erf_hdr;
    uint8_t number = (uint8_t)out_port;
    if ((NULL == p) || (NULL == cp_org))
    {
        printf("nac_tx_memcpy: input args is NULL!\n");
        return NULL;
    }
    if (number > 3)
    {
        printf("nac_tx_memcpy: input out_port is out of range(0~3)!\n");
        return NULL;
    }
    erf_hdr = (erf_record_t *)p;
    rlen = ntohs(erf_hdr->rlen);
    //set transmit port(out_port locate in bit 4~5)
    *(p + 15) &= 0xCF; //clear out_port
    *(p + 15) |= (number << 4); //set out_port
    //copy an packet from where bottom pointer.
    memcpy(cp_org,p,rlen);
    cp_org += rlen;
    return cp_org;

}

void nacutil_tsc32_read(uint32_t* tsc)
{
    __asm__ __volatile__("rdtsc" : "=a" (*tsc) : : "edx");
}


/* this function get the cpu clk cycles past in one ms*/
uint32_t get_cpu_tick_one_ms()
{
    uint32_t tsc_before,tsc_after;
    uint32_t tsc_one_ms;

    struct timespec time_to_sleep;

    /*first determine cpu speed*/
    time_to_sleep.tv_sec = 0;
    time_to_sleep.tv_nsec = 20*1000000;
    nanosleep(&time_to_sleep,NULL); /*dummy sleep for 20ms, let this interrupt pass*/
    time_to_sleep.tv_nsec = 100*1000000;  /* sleep 200ms, usually takes 20ms */

    nacutil_tsc32_read(&tsc_before);
    nanosleep(&time_to_sleep,NULL);
    nacutil_tsc32_read(&tsc_after);

    tsc_one_ms = (tsc_after - tsc_before)/100;
    printf("cpu speed is %uMhz \n",tsc_one_ms/1000);

    return tsc_one_ms;
}

/************************Refresh QDR API***************************************/
uint8_t*
nac_rft_init(int nacfd)
{
    uint32_t rft_num = (NUM_STREAM * 2);
    dma_ctl_reg_t dma_ctl_reg;
    stream_t *stream = &(herd[nacfd].stream[rft_num]);

    if(!herd[nacfd].opened)
    {
        errno = EBADF;
        return NULL;
    }

    if(!stream->attached)
    {
        errno = EINVAL;
        return NULL;
    }

    return herd[nacfd].stream[rft_num].buf;
}

int
nac_rft_start(int nacfd)
{
    nacinf_t * info = nac_info(nacfd);
    uint32_t rft_num = (NUM_STREAM * 2);
    dma_ctl_reg_t dma_ctl_reg;
    uint32_t limit_ptr = herd[nacfd].stream[rft_num].pbm->limit_ptr;
    uint32_t phy_addr = herd[nacfd].stream[rft_num].phyaddr;
    uint32_t buf_size = ((info->buf_size)-RFT_SIZE) / rft_num;
    uint32_t dma_ctl_reg_addr = herd[nacfd].stream[rft_num].pbm->dma_ctl_reg;
    uint32_t dma_start = info->phy_addr + rft_num* buf_size;
    uint32_t dma_mid = info->phy_addr + rft_num* buf_size+(RFT_SIZE/2);
    uint32_t dma_end = info->phy_addr + rft_num* buf_size+RFT_SIZE;
    uint32_t record = 0;
    uint32_t limit  = 0;
    reg_write(nacfd, (unsigned int)limit_ptr, (unsigned int)dma_mid);
    record = reg_read(nacfd, RFT_RECORD_PTR_LO_REG);
    limit  = reg_read(nacfd, RFT_LIMIT_PTR_LO_REG);
    while(limit == record)
    {
        record = reg_read(nacfd, RFT_RECORD_PTR_LO_REG);
        limit  = reg_read(nacfd, RFT_LIMIT_PTR_LO_REG);
        usleep(500);
    }
    reg_write(nacfd, (unsigned int)limit_ptr, (unsigned int)dma_end);
    record = reg_read(nacfd, RFT_RECORD_PTR_LO_REG);
    limit  = reg_read(nacfd, RFT_LIMIT_PTR_LO_REG);
    while(limit != record)
    {
        record = reg_read(nacfd, RFT_RECORD_PTR_LO_REG);
        limit  = reg_read(nacfd, RFT_LIMIT_PTR_LO_REG);
        usleep(500);
    }

    usleep(10000);
    herd[nacfd].stream[rft_num].started=1;
    return 0;
}

int
nac_write_flowtable(uint8_t * addr,uint32_t flowtable_entry_num, uint16_t policy_id, uint16_t hash2)
{
    uint8_t *flowtable_addr = addr;
    uint8_t *p ;
    uint16_t tmp;
    if(policy_id>=STRATEGY_LIST_MAX)
    {
        printf("wrong policy_id, rang(0 ~ %d)\n",STRATEGY_LIST_MAX-1);
        return -1;
    }
    if(flowtable_entry_num>=FLOWTABLE_MAX_NUM)
    {
        printf("wrong flowtable_entry_num, rang(0 ~ 2^22)\n");
        return -1;
    }
    if(hash2 > 0x3f)
    {
        printf("hash2 error, range 0~0x3f");
        return -1;
    }
    tmp = ((hash2&0x3f)<<10)|policy_id;
    p = (uint8_t *)&tmp;
    memcpy((uint8_t *)(flowtable_addr+(flowtable_entry_num*2)) , (uint8_t *)(p+1) , 1 );
    memcpy((uint8_t *)(flowtable_addr+(flowtable_entry_num*2)+1) , (uint8_t *)(p) , 1 );
    return 0;
}

int
nac_rft_attach_stream(int nacfd)
{

    void *sp, *ep;
    void *p;
    int  buf_size = 0;
    nacpbm_stream_t *pbm=0;
    nacinf_t * info;
    int return_val = -1;
    uint32_t rft_num = (NUM_STREAM * 2);
    if (!herd[nacfd].opened)
    {
        errno = EBADF;
        return -1;
    }
    info = nac_info(nacfd);

    /* get buffer size */
    buf_size = ((info->buf_size)-RFT_SIZE) / rft_num;
    // Store buf_size as an stream parameter
    herd[nacfd].stream[rft_num].size = RFT_SIZE;

    // Set stream polling parameters
    herd[nacfd].stream[rft_num].mindata = nacapi_mindata;
    herd[nacfd].stream[rft_num].maxwait = nacapi_maxwait;
    herd[nacfd].stream[rft_num].poll    = nacapi_poll;


    // Do the mmap
    if((sp = mmap(NULL, RFT_SIZE, PROT_READ | PROT_WRITE,
                  MAP_ANON|MAP_SHARED, -1, 0)) == MAP_FAILED)
    {
        return_val = (int) MAP_FAILED;
        goto fail;
    }

    /*
     * Now map the real buffer, 1st round.
     */

    if((p = mmap(sp, RFT_SIZE, PROT_READ | PROT_WRITE,
                 MAP_FIXED|MAP_SHARED, nacfd,  buf_size * rft_num)) == MAP_FAILED)
    {
        return_val = (int) MAP_FAILED;
        goto fail;
    }
    printf("RFT stream offset is %x\n",buf_size * rft_num);
    // Store the pointer to the memory hole space (stream)
    herd[nacfd].stream[rft_num].buf = p;
    printf("RFT stream attach :buf is 0x%x  \n",herd[nacfd].stream[rft_num].buf);
    herd[nacfd].stream[rft_num].extra_window_size =0;
    // Set initial record offset
    herd[nacfd].stream[rft_num].offset = 0;

    // Store stream starting physical address
    herd[nacfd].stream[rft_num].phyaddr = info->phy_addr + rft_num * buf_size;

    // If we arrive this point, all has been successful
    herd[nacfd].stream[rft_num].attached = 1;
    herd[nacfd].stream[rft_num].started  = 0;
    printf("RFT stream attached\n");
    return 0;

fail:

    return return_val; /* Usually -1, sometimes (int) MAP_FAILED. */
}
