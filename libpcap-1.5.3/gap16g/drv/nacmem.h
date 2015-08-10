#ifndef NAC_MEM_H
#define NAC_MEM_H

#ifndef NAC_MEM_SIZE
#define NAC_MEM_SIZE (256*1024*1024)
#endif
#ifndef NAC_MIN_SIZE	
#define NAC_MIN_SIZE	(32*1024*1024)
#endif
void	*nac_mem_malloc(size_t *size, uint32_t flags);
void	nac_mem_free(const void *objp);


#endif 



























