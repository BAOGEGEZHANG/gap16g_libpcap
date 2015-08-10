#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pagemap.h>

//#define KERNEL_26X

//#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#ifdef KERNEL_26X
#include <linux/moduleparam.h>
#endif

//#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#ifdef KERNEL_26X
#include <linux/autoconf.h> /* retrieve the CONFIG_* macros */
#else 
#include <linux/config.h> /* retrieve the CONFIG_* macros */
#endif

#ifdef KERNEL_26X
#include <linux/moduleparam.h>
#endif

#include "nacmem.h"
#include "nacpci.h"
#ifdef KERNEL_26X
EXPORT_SYMBOL(nac_mem_malloc);
EXPORT_SYMBOL(nac_mem_free);
#endif



/*
 * Global parameters. Could be supplied at compile time command line.
 * Maximum number of NAC boards supported by nacmem.
 */
#ifndef	NNACS
#define	NNACS	16
#endif /* NNACS */


MODULE_AUTHOR("NAC Team");
MODULE_DESCRIPTION("memory allocation module for pci express network capture cards");
MODULE_SUPPORTED_DEVICE("nac_pcie_board");

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

enum { MBYTE = (1024*1024), KBYTE = (1024) };

#ifdef KERNEL_26X
static int nnacs;		module_param(nnacs, int, 0);

// size is unsigned !!!!  
static unsigned int	size[NNACS]; 	//module_param_array(size,uint,NULL,0);

static char	ds[16], *dsize = ds;	module_param_string(dsize, ds, sizeof(ds), 0);
static uint32_t	align = 1*MBYTE;	module_param(align, int, 0);
static char	da[16], *dalign = da;	module_param_string(dalign, da, sizeof(da), 0);
static int 	gfporder = 12;		module_param(gfporder, int, 0);
#else
static int	nnacs;			MODULE_PARM(nnacs, "i");

// size is unsigned now warning 
static unsigned int	size[NNACS];	MODULE_PARM(size, "1-16u");

static char	ds[16], *dsize = ds;	MODULE_PARM(dsize, "1-15s");
static uint32_t	align = 1*MBYTE;	MODULE_PARM(align, "i");
static char	da[16], *dalign = da;	MODULE_PARM(dalign, "1-15s");
static int 	gfporder = 12;		MODULE_PARM(gfporder, "i");
				/* really depends on NR_MEM_LISTS in mm/page_alloc.c */
#endif


MODULE_PARM_DESC(nnacs, "Number of NAC cards present. Default 0 means autodetect");
MODULE_PARM_DESC(size, "Deprecated");
MODULE_PARM_DESC(dsize, "Memory to allocate per card. KB, MB, GB suffixes, default 32MB");
MODULE_PARM_DESC(align, "Deprecated");
MODULE_PARM_DESC(dalign, "Buffer alignment. KB, MB, GB suffixes, default 1MB");
MODULE_PARM_DESC(gfporder, "Largest allocation order to try, default 12");

#define DEBUG
#ifdef DEBUG
# define PCIE_DEBUG(MSG) printk MSG
#else
# define PCIE_DEBUG(MSG)
#endif

#define	PTABSIZE	1024

static unsigned long	nac_mem_ptable[PTABSIZE];
static unsigned long	nac_mem_vaddr[NNACS];	/* contigous areas of memory allocated for cards */
static int32_t  nac_mem_inuse[NNACS];	/* allocation flag for those areas */
//memset(nac_mem_inuse, 0, sizeof(nac_mem_inuse));

#define	HTABSIZE	8192
static unsigned long	nac_mem_htable[HTABSIZE];

static void		nac_mem(void);
static void		nac_mem_cleanup(void);
static void		nac_mem_update_nnacs(void);
static void		nac_mem_alloc_ptable(void);
static void		nac_mem_sort_ptable(void);
static void		nac_mem_free_ptable(void);



static unsigned long	nac_mem_findcontig(unsigned int csize, int calign);
static void		nac_mem_freecontig(unsigned int csize,unsigned long vaddr);

int
nac_mem_init(void)
{
	/*
	 * Specifying dsize has priority over size, we don't intend to
	 * document size=<N>, since dsize is the more general mechanism.
	 */
	int nac = 0;
	unsigned int i_size = NAC_MEM_SIZE;
	/* delete input memomry size from outside ,wo don't need;
	if(dsize[0] != '\0') {
		char	*more;

		size = simple_strtoul(dsize, &more, 0);
		switch(*more) {
		  case '\0':
			break;
		  case 'M':
		  case 'm':
			size *= MBYTE;
			break;
		  case 'K':
		  case 'k':
			size *= KBYTE;
			break;
		  default:
			printk("nac_mem:  character '%c' unrecongnized!\n",*more);
			break;
		}
		if((*more != '\0') && (*++more != '\0'))
			printk("nac_mem: too many  character '%c'!\n", *more);
		PCIE_DEBUG(("nac_mem: size is %d\n", size));
	}*/
	if(dalign[0] != '\0') {
		char	*more;

		align = simple_strtoul(dalign, &more, 0);
		
		switch(*more) {
		  case '\0':
			break;
		  case 'M':
		  case 'm':
			align *= MBYTE;
			break;
		  case 'K':
		  case 'k':
			align *= KBYTE;
			break;
		  default:
			printk("nac_mem:  character '%c' unrecognized!\n",*more);
			
			break;
		}
		if((*more != '\0') && (*++more != '\0'))
			printk("nac_mem: too many character '%c' !\n", *more);
		PCIE_DEBUG(("nac_mem: align is %d\n", align));
	}
	if(nnacs == 0){
		nac_mem_update_nnacs();
	}
	if(nnacs == 0){
		nnacs = 1;
	}
	if(nnacs > NNACS) {
//		PCIE_DEBUG("nacmem: only supporting %d out of %d nacs\n", NNACS, nnacs);
		nnacs = NNACS;
	}
#ifndef PLATFORM64
	i_size /= nnacs;//divide NAC_MEM_SIZE into nnacs pieces for every card;
	PCIE_DEBUG(("nac_mem:current  platform32 i_size is %uMB\n",i_size>>20));
#endif
	for(nac = 0;nac < nnacs;nac++)
		{
			size[nac] = i_size;	
			PCIE_DEBUG(("nac_mem: size[%d] is %uMB\n",nac,size[nac]>>20));
		}
	nac_mem_alloc_ptable();
	nac_mem_sort_ptable();
	nac_mem();

	nac_mem_free_ptable();
	PCIE_DEBUG(("nac_mem: init done\n"));
	return 0;
}

void
nac_mem_exit(void)
{
	PCIE_DEBUG(("nac_mem: cleanup module\n"));
	nac_mem_cleanup();
}


#define	ATTR_UNUSED __attribute__((unused))
static spinlock_t	nac_mem_lock ATTR_UNUSED = SPIN_LOCK_UNLOCKED;

void *
nac_mem_malloc(size_t *msize, uint32_t mflags)
{
	unsigned long	sflags;
	int i;
	void		*retval = NULL;

	PCIE_DEBUG(("nac_mem: nac_mem_malloc msize inital value %u\n", (uint32_t)*msize));
	spin_lock_irqsave(&nac_mem_lock, sflags);
	
	for(i = 0; i < nnacs; i++) {
		if(!nac_mem_inuse[i])
			break;
	}
	if(i == nnacs)
		goto done;	/*  no memory */
	retval = (void *)nac_mem_vaddr[i];
	if(*msize > size[i]) {

		printk("nac_mem: mem malloc %u larger than supported size %u\n", (unsigned)(*msize), size);
                *msize = size[i];
        } else if (*msize == 0)
                /*
                 * Driver is allowing us to autodetect the allocated memory
                 */
                *msize = size[i];
  PCIE_DEBUG(("nac_mem: mem malloc size for nac[%d] %uMB\n",i,(uint32_t)(*msize)>>20));            
	if(retval == NULL)
		goto done;	/* no memory */
	nac_mem_inuse[i]++;
#ifndef KERNEL_26X
	MOD_INC_USE_COUNT;	/* prevents unloading */
#endif
done:
	spin_unlock_irqrestore(&nac_mem_lock, sflags);
	PCIE_DEBUG(("nac_mem: nac mem malloc returning %p\n", retval));
	return retval;
}

void
nac_mem_free(const void *objp)
{
	unsigned long	sflags;
	int i;

	printk("nac_mem: mem free 0x%p\n", objp);
	spin_lock_irqsave(&nac_mem_lock, sflags);
	for(i = 0; i < nnacs; i++) {
		if((void *)nac_mem_vaddr[i] == objp)
			break;
	}
	if(i == nnacs) {
		
		goto done;
	}
	nac_mem_inuse[i]--;
#ifndef KERNEL_26X
	MOD_DEC_USE_COUNT;
#endif
done:
	spin_unlock_irqrestore(&nac_mem_lock, sflags);
}

static void
nac_mem(void)
{
	int nac;
	struct page     *pstart, *pend;
	//find the appropriate mem size
	for(nac = 0; nac < nnacs; nac++) {
		while( (nac_mem_vaddr[nac] = nac_mem_findcontig(size[nac], align)) == 0 ){
			size[nac] /= 2;
			PCIE_DEBUG(("nac_mem: new size for nac%d is %uMB\n",nac,size[nac]/1024/1024));
			if( size[nac] <= NAC_MIN_SIZE) {
				printk("nac_mem: cannot allocate memory for card #%d!\n" , nac);
				return;
			}
		}
	//PCIE_DEBUG(("nac_mem: memory of size %u for nac card #%d at 0x%08lx\n", size,
	//	nac, nac_mem_vaddr[nac]));
	//PCIE_DEBUG(("nac_mem: memory of size %u for nac card #%d at 0x%08lx\n", size,
	//	nac, virt_to_phys(nac_mem_vaddr[nac])));
		/* now mark the pages as reserved; otherwise remap_page_range doesn't do what we want */
	pend = virt_to_page(nac_mem_vaddr[nac] + size[nac] - 1);
	PCIE_DEBUG(("nac_mem: nac[%d] size %uMB setbit pstart %p to pend %p\n",nac,size[nac]>>20,virt_to_page(nac_mem_vaddr[nac]),pend));
	for (pstart = virt_to_page(nac_mem_vaddr[nac]); pstart <= pend; pstart++)
		set_bit(PG_reserved, &((pstart)->flags));

	}
	PCIE_DEBUG(("nac_mem: reserve mem done!\n"));
}

static void
nac_mem_cleanup(void)
{
	int nac;
	struct page     *pstart, *pend;

	PCIE_DEBUG(("nac_mem: cleanup mem!\n"));
	for (nac = 0; nac < nnacs; nac++)
		if(nac_mem_vaddr[nac]) {
			/* undo marking the pages as reserved */
			pend = virt_to_page(nac_mem_vaddr[nac] + size[nac] - 1);
			PCIE_DEBUG(("nac_mem: nac[%d]size %uMB clearbit pstart %p to pend %p\n",nac,size[nac]>>20,virt_to_page(nac_mem_vaddr[nac]),pend));
			for (pstart = virt_to_page(nac_mem_vaddr[nac]); pstart <= pend; pstart++)
				clear_bit(PG_reserved, &((pstart)->flags));
			nac_mem_freecontig(size[nac],nac_mem_vaddr[nac]);
	        }
	PCIE_DEBUG(("nac_mem: cleanup reserved mem done!\n"));
}


static void
nac_mem_alloc_ptable(void)
{
	int	order = gfporder;
	int	i = 0, j = 0;
	unsigned long	page;

	PCIE_DEBUG(("nac_mem: allocate page table\n"));

	/*
	 * We actually need to allocate all the available pages from order
	 * before analyzing as we may get old pages returned when skipping
	 * over discontiguity.
	 */
	while((page = __get_free_pages(GFP_ATOMIC
//#ifdef GFP_DMA32
				       | GFP_DMA32
//#endif
#ifdef __GFP_NOWARN
				       | __GFP_NOWARN
#endif
				       , order)) == 0) {
		if(order-- == 0)
			printk("nac_mem: error , no mem to allocate!\n");
	}
	/* determined order here */
	PCIE_DEBUG(("nac_mem: order is %u, %luKB\n", order, (PAGE_SIZE >> (10 - order))));
	gfporder = order;

	if(virt_to_phys((void*)page)<0xfffffffful)
		nac_mem_ptable[i++] = page;
	else
		nac_mem_htable[j++] = page;
	for( ; i < PTABSIZE ; ) {
		if((page = __get_free_pages(GFP_ATOMIC
#ifdef GFP_DMA32
					    | GFP_DMA32
#endif
#ifdef __GFP_NOWARN
					    | __GFP_NOWARN
#endif
					    , order)) == 0)
			break;
		if(virt_to_phys((void*)page)<0xfffffffful)
		{
			nac_mem_ptable[i++] = page;
			//PCIE_DEBUG(("nac_mem_ptable[%d]: 0x%lx\n", i-1, virt_to_phys((void *)page)));
		}
		else
			nac_mem_htable[j++] = page;
	}

	//PCIE_DEBUG(("nac_mem: %d slots, %ldMB total\n", i, (i*(PAGE_SIZE >> (10 - order)) )/1024));

	//PCIE_DEBUG(("nac_mem: %d high slots, %ldMB total\n", j, (j*(PAGE_SIZE >> (10 - order)) )/1024 ));

	PCIE_DEBUG(("nac_mem: allocate ptable done\n"));
}

/*
**desending sort ptable 
*/
static void
nac_mem_sort_ptable(void)
{
	int unsorted, i;
	unsigned long tmp;

	do {
		unsorted = 0;
		for (i=1; i<PTABSIZE; i++)
			if (nac_mem_ptable[i] > nac_mem_ptable[i-1]) {
				tmp = nac_mem_ptable[i-1];
				nac_mem_ptable[i-1] = nac_mem_ptable[i];
				nac_mem_ptable[i] = tmp;
				unsorted = 1;
			}
	} while (unsorted);
}

static void
nac_mem_free_ptable(void)
{
	int i;
	uint32_t order = gfporder;

	PCIE_DEBUG(("nac_mem: free ptable begin\n"));

	for( i = 0 ; i < PTABSIZE ; i++) {
		if(nac_mem_ptable[i] == 0) {
			/*
			 * This piece is either been taken away by nac_mem or we
			 * are actually at the end of the list. Don't bother.
			 */
			continue;
		}
		free_pages(nac_mem_ptable[i], order);
		nac_mem_ptable[i] = 0;	/* just to be safe */
	}
	/*
	 * Now must also free htable
	 */
	for( i = 0 ; i < HTABSIZE ; i++) {
		if(nac_mem_htable[i] == 0)
			continue;
		free_pages(nac_mem_htable[i], order);
		nac_mem_htable[i] = 0;	/* just to be safe */
	}

	PCIE_DEBUG(("nac_mem: ptable free done\n"));
}

static unsigned long
nac_mem_findcontig(unsigned int csize, int calign)
{
	int		i, j;
	int		psize = (PAGE_SIZE << gfporder);	/* size of one piece */
	static int 	cstart = 0;				/* start of contigous area */
	unsigned long	retval;

	PCIE_DEBUG(("nac_mem: find contigous area begin\n"));

	calign--;
	if(nac_mem_ptable[cstart] == 0) {
		/*
		 * Since cstart is static, this may now happen.
		 * End of the story 0.
		 */
		PCIE_DEBUG(("nac_mem: failed to find contigous area, error 1\n"));
		return 0;
	}
	for(i = cstart+1 ; i < PTABSIZE ; i++) {
		if((((i-cstart)*psize) >= csize) && ((nac_mem_ptable[i-1] & calign) == 0)) {
			/*
			 * Done! Cool.
			 */
			//PCIE_DEBUG(("nac_mem: found at slots [%d-%d] mem [0x%lx-0x%lx]\n",
			//		cstart, i-1, nac_mem_ptable[cstart], nac_mem_ptable[i-1]));
			retval = nac_mem_ptable[i-1];	/* preserve return value before clearing */
			/*
			 * Skip the first ones we don't need because of alignment
			 */
			cstart = i - (csize / psize);
			//PCIE_DEBUG(("nac_mem: really need [%d-%d] mem [0x%lx-0x%lx]\n",
			//		cstart, i-1, nac_mem_ptable[cstart], nac_mem_ptable[i-1]));
			/*
			 * Deleting used entries from table, so they won't
			 * be freed by freeptable().
			 */
			for(j = cstart ; j < i ; j++)
				nac_mem_ptable[j] = 0;
			cstart = i;	/* need you next time around here (be care cstart is  local static variable) */
			return retval;
		}
		if(nac_mem_ptable[i] == 0) {
			/*
			 * That's the end of the story, sorry.
			 */
			//PCIE_DEBUG(("nac_mem: findcontig failed 1\n"));
			//if failed restart from 0
			cstart = 0;
			return 0;
		}
		if((nac_mem_ptable[i-1] - nac_mem_ptable[i]) != psize) {
			/*
			 * Discontigouty. Start over.
			 */
			cstart = i;
		}
	}
	/*
	 * Another end.
	 */
	PCIE_DEBUG(("nac_mem: failed to find contigous area, error  2\n"));
	return 0;
}


static void
nac_mem_freecontig(unsigned int csize,unsigned long vaddr)
{
	int		psize = (PAGE_SIZE << gfporder);	/* size of one piece */
	unsigned long	vend = vaddr + csize;

	PCIE_DEBUG(("nac_mem: free contig  area at 0x%lu\n", vaddr));
	for( ; vaddr < vend ; vaddr += psize)
		free_pages(vaddr, gfporder);
}

static void
nac_mem_update_nnacs(void)
{
	struct pci_dev		*from;
	struct pci_device_id	*id = nac_pci_tbl;

	while(id->vendor) {
		from=NULL;
		while( (from = pci_find_device(id->vendor, id->device, from)) )
			nnacs++;
		id++;
	}
}

module_init(nac_mem_init);
module_exit(nac_mem_exit);
























