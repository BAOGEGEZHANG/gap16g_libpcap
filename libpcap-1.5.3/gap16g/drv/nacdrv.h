#ifndef NACDRV_H
#define NACDRV_H

//#define KERNEL_26X

#ifdef KERNEL_26X
# include <linux/cdev.h>
#else
# include <linux/devfs_fs_kernel.h>
#endif

#include "nacnew.h"

typedef struct pciconfig {
	uint32_t	ba0;	/* base address 0 */
	uint32_t	ba1;	/* base address 1 */
	uint32_t	ba2;	/* base address 2 */
	uint32_t	com;	/* command */
	uint8_t	lin;	/* interrupt line */
	uint8_t	lat;	/* latency timer */
} pciconfig_t;

struct nac_duck;

typedef struct nac_lock {
	pid_t	pid;	/* pid of the process/thread */
	pid_t	tgid;	/* thread group Id */
} nac_lock_t;

typedef struct nac_softc {
	int		unit;
	int		flags;
	struct semaphore	lock_sem;
	struct 		pci_dev  *device;	/* PCI device */
	nac_lock_t	lock[NAC_STREAM_MAX];
	uint8_t         *mmio;
	uint32_t         mmio_size;
	uint8_t         *mmbuf;
	uint8_t         *buf;
	uint32_t         buf_size;
	uint32_t         irq;
	uint32_t         monseq;
# ifdef KERNEL_26X
	struct cdev     cdev_nac, cdev_mem, cdev_iom;
# else
	devfs_handle_t	devfs_nac, devfs_mem, devfs_iom;
# endif
	pciconfig_t	pciconfig;
       struct nac_duck *duck;
} nac_softc_t;

int nac_pciconfig_restore(nac_softc_t *sc);


#endif /* NACDRV_H */

