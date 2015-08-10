#include <linux/pci.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
//#include <linux/devfs_fs_kernel.h>
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#ifdef KERNEL_26X
#include <linux/cdev.h>
#endif
#include <asm/io.h>
#include <asm/uaccess.h> 
#include "nacdrv.h"
#include "nacts.h"
#include "nacmem.h"
#include "nacpci.h"
#include "nacnew.h"

#define DEBUG
#undef DD
#ifdef DEBUG
#  define DD(MSG) 	printk MSG
#else
#  define DD(MSG)	
#endif

#define DA(MSG)		printk MSG
#define DN(MSG)	

const char* const knacReleaseVersion = "1.0";
#ifdef KERNEL_26X
#define NACUNIT(X)	(iminor(X)&NACMINORMASK)
#define NACMINOR(X)	(iminor(X)>>NACMINORBITS)
#else
#define	NACUNIT(X)	(MINOR(X->i_rdev)&NACMINORMASK)
#define	NACMINOR(X)	(MINOR(X->i_rdev)>>NACMINORBITS)
#endif

#define NAC_MAX_BOARDS	(1<<NACMINORBITS)

#define nac_iowrite(X,Y) 	iowrite32(X, sc->mmio + Y)
#define nac_ioread(X) 	 	ioread32(sc->mmio + X)

enum {
	NACF_ATTACHED	= 0x0001,
};

/* Driver global array of data structures holding state information per card */
static nac_softc_t	nac_array[NAC_MAX_BOARDS];

#ifdef KERNEL_26X
static dev_t	nac_dev;
#else
static int	nac_major;
#endif

extern struct file_operations	nac_fops;
extern struct file_operations	nac_mem_fops;
extern struct file_operations	nac_iom_fops;

struct file_operations	*nac_fops_array[NACMINOR_MAX] = {
	&nac_fops,
	&nac_mem_fops,
	&nac_iom_fops
};

static int
nac_find_unattached(void)
{
	int c;
	for(c=0; c<NAC_MAX_BOARDS; c++)
		if (!(nac_array[c].flags & NACF_ATTACHED))
			return c;
	return -1;
}

/*
 * PCI configuration save/restore, as configured by BIOS on system startup
 */
static void
nac_pciconfig_save(nac_softc_t *sc)
{
	pci_read_config_dword(sc->device, PCI_BASE_ADDRESS_0,
			      &sc->pciconfig.ba0);
	pci_read_config_dword(sc->device, PCI_BASE_ADDRESS_1,
			      &sc->pciconfig.ba1);
	pci_read_config_dword(sc->device, PCI_BASE_ADDRESS_2,
			      &sc->pciconfig.ba2);
	pci_read_config_dword(sc->device, PCI_COMMAND,
			      &sc->pciconfig.com);
	pci_read_config_byte(sc->device, PCI_INTERRUPT_LINE,
			     &sc->pciconfig.lin);
	pci_read_config_byte(sc->device, PCI_LATENCY_TIMER,
			     &sc->pciconfig.lat);

	DD((KERN_DEBUG "nac%d: ba0 0x%x\n", sc->unit, sc->pciconfig.ba0));
	DD((KERN_DEBUG "nac%d: ba1 0x%x\n", sc->unit, sc->pciconfig.ba1));
	DD((KERN_DEBUG "nac%d: ba2 0x%x\n", sc->unit, sc->pciconfig.ba2));
	DD((KERN_DEBUG "nac%d: com 0x%x\n", sc->unit, sc->pciconfig.com));
	DD((KERN_DEBUG "nac%d: lin 0x%x\n", sc->unit, sc->pciconfig.lin));
	DD((KERN_DEBUG "nac%d: lat 0x%x\n", sc->unit, sc->pciconfig.lat));
}

int
nac_pciconfig_restore(nac_softc_t *sc)
{
	pci_write_config_dword(sc->device, PCI_BASE_ADDRESS_0,
			       sc->pciconfig.ba0);
	pci_write_config_dword(sc->device, PCI_BASE_ADDRESS_1,
			       sc->pciconfig.ba1);
	pci_write_config_dword(sc->device, PCI_BASE_ADDRESS_2,
			       sc->pciconfig.ba2);
	pci_write_config_dword(sc->device, PCI_COMMAND,
			       sc->pciconfig.com);

	pci_write_config_byte(sc->device, PCI_INTERRUPT_LINE,
			      sc->pciconfig.lin);
	pci_write_config_byte(sc->device, PCI_LATENCY_TIMER,
			      sc->pciconfig.lat);
	return 0;
}


#ifdef KERNEL_26X
static irqreturn_t
#else
#define IRQ_NONE
#define IRQ_HANDLED
static void
#endif


nac_interrupt(int irq, void *dev, struct pt_regs *regs)
{
       nac_softc_t *sc = dev;

       /* right now we have only one source of interupt */
 
//	duck_intr(sc);   

	return IRQ_HANDLED;
}

static int
nac_open(struct inode *inode, struct file *file)
{
	int		unit = NACUNIT(inode);
	nacminor_t	minor = NACMINOR(inode);
	nac_softc_t	*sc;

	//DD(("nac_open i_rdev %d unit %d minor %d priv %p\n", inode->i_rdev, unit, minor, file->private_data));
	/* if file->private_data is not valid, then we are not using devfs.
	 * This means that we must decode the minor number to attach the
	 * correct fops for the entry point.
	 */
	if(!file->private_data && minor) {
		/* check minor number is valid */
		if (minor >= NACMINOR_MAX) {
			DA( (KERN_ERR "nac%u: open: illegal minor %u\n", unit, minor) );
			return -ENODEV;
		}

		file->f_op = nac_fops_array[minor];
	}

	sc = (nac_softc_t *)file->private_data;

	/* if file->private_data is not valid, then we are not using devfs.
	 * This means we need to check that the unit number is valid, and
	 * fill in private_data for other methods
	 */
	if (!sc) {
		/* Unit spans too high */
		if(unit > NAC_MAX_BOARDS ) {
			DA( (KERN_WARNING "nac%u: open: unit number too high, does not exist\n", unit) );
			return -ENODEV;
		}
		
		/* Unit in valid range, but card may has been removed */
		if(!(nac_array[unit].flags & NACF_ATTACHED)) {
			DA( (KERN_WARNING "nac%u: open: unit does not exist\n", unit) );
			return -ENODEV;
		}
		
		file->private_data = &nac_array[unit];
		sc = (nac_softc_t *)file->private_data;
	}

#ifndef KERNEL_26X
	MOD_INC_USE_COUNT;
#endif
	return 0;
}

static int
nac_release(struct inode *inode, struct file * file)
{
	nac_softc_t	*sc = (nac_softc_t *)file->private_data;
	int		c;

	/*
	 * If the device is locked, and the process releasing the device has the lock,
	 * then it has not exited correctly. Reset the card and clear the lock.
	 */
	for(c=0;c<NAC_STREAM_MAX;c++)
		if((sc->lock[c].tgid == current->tgid) && (NACMINOR(inode) == NACMINOR_NAC)) {
			DA((KERN_WARNING "nac%u: PID %d (TGID %d) released stream %d while still locked, resetting\n", sc->unit, current->pid, current->tgid, c));
#if 0
			nac_halt(sc); /* XXX How to reset/halt one stream? */
#endif
			sc->lock[c].pid = 0;
			sc->lock[c].tgid = 0;
		}
#ifndef KERNEL_26X
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

static ssize_t
nac_mem_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
	nac_softc_t	*sc = fp->private_data;
	size_t		remain;
	
	if(sc->mmbuf == NULL)
		return -ENXIO;
	if(*ppos >= sc->buf_size)
		return 0;
	if((*ppos + count) > sc->buf_size)
		count = sc->buf_size - *ppos;
	if(count == 0)
		return 0;
	remain = copy_to_user(buf, &sc->mmbuf[*ppos], count);
	*ppos += count - remain;
	if(remain == count)
		return -EINVAL;
	else
		return count-remain;
}

static ssize_t
nac_iom_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
	nac_softc_t	*sc = fp->private_data;
	size_t		remain;

	if(*ppos > sc->mmio_size)
		return 0;
	if((*ppos + count) > sc->mmio_size)
		count = sc->mmio_size - *ppos;
	if(count == 0)
		return 0;
	if((*ppos & 0x03) != 0)
		return -EFAULT;		/* only support word aligned access */
	if((count & 0x03) != 0)
		return -EFAULT;		/* only support word sized access */
	remain = copy_to_user(buf, sc->mmio + *ppos, count);
	*ppos += count - remain;
	if(remain == count)
		return -EINVAL;
	else
		return count-remain;
}


static ssize_t
nac_mem_write(struct file *fp, const char *buf, size_t count, loff_t *ppos)
{
	nac_softc_t	*sc = fp->private_data;
	size_t		remain;

	if(sc->mmbuf == NULL)
		return -ENXIO;
	if(*ppos >= sc->buf_size)
		return -ENOSPC;
	if((*ppos + count) > sc->buf_size)
		count = sc->buf_size - *ppos;
	if(count == 0)
		return -ENOSPC;
	remain = copy_from_user(&sc->mmbuf[*ppos], buf, count);
	*ppos += count - remain;
	if(remain == count)
		return -EINVAL;
	else
		return count-remain;
}

static ssize_t
nac_iom_write(struct file *fp, const char *buf, size_t count, loff_t *ppos)
{
	nac_softc_t	*sc = fp->private_data;
	size_t		remain;

	if(*ppos > sc->mmio_size)
		return 0;
	if((*ppos + count) > sc->mmio_size)
		count = sc->mmio_size - *ppos;
	if(count == 0)
		return 0;
	if((*ppos & 0x03) != 0)
		return -EFAULT;		/* only support word aligned access */
	if((count & 0x03) != 0)
		return -EFAULT;		/* only support word sized access */
	remain = copy_from_user(sc->mmio + *ppos, buf, count);
	*ppos += count - remain;
	if(remain == count)
		return -EINVAL;
	else
		return count-remain;
}

static int
nac_ioctl(struct inode *inode, struct file *fp, u_int cmd, u_long arg)
{
	nac_softc_t	*sc = fp->private_data;	
	int		error=0;
	nacinf_t	info;
	duckinf_t duckinf;

	//DD( (KERN_DEBUG "nac%d: IOCTL 0x%08x\n", sc->unit, cmd) );

	if (_IOC_TYPE(cmd) != NAC_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > NAC_IOC_MAXNR) return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		error = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		error = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
	if (error) return -EFAULT;

	switch(cmd) {
	case NACIOCINFO:
			//DD(("zzl 1\n"));
		if (sc->device)
			info.device_code = sc->device->device;
		else
			info.device_code = 0;
		info.id = sc->unit;
		info.phy_addr = (uint32_t)(sc->buf);
		info.buf_size = sc->buf_size;
		info.iom_size = sc->mmio_size;
		if(copy_to_user((void*)arg, &info, sizeof(nacinf_t)))
		{
			error = -EFAULT;
			//DD(("zzl 2\n"));
		}
		break;
		
	case NACIOCDUCK:

		if(copy_from_user(&duckinf, (void*)arg, sizeof(duckinf_t))) {
			error = -EFAULT;
			break;
		}
		 
		error = duck_ioctl(sc->duck, &duckinf);
		if(copy_to_user((void*)arg, &duckinf, sizeof(duckinf_t)))
			error = -EFAULT;

		break;
		
	default:
		error = -ENOTTY;
		//DD(("zzl 3\n"));
		break;
	}
	return error;
}

static int
nac_mmap(struct file *fp, struct vm_area_struct * vma)
{
	nac_softc_t	*sc = fp->private_data;
	loff_t		off, physical, vsize, psize;

	off = (off_t)vma->vm_pgoff << PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;

	if(sc->buf == NULL) {
		DA( (KERN_WARNING "nac%u: mmap: no memory assigned to this nac\n", sc->unit) );
		return -ENOMEM;
	}
	physical = (unsigned long)sc->buf + off;
	psize = sc->buf_size - off;
	
	if (off & (PAGE_SIZE-1))
		return -ENXIO; /* needs aligned offsets */
	if (vsize > psize)
		return -EINVAL; /* spans too high */
	
        /*
	 * Don't dump addresses that are not real memory to a core file.
	 */
	if (off >= __pa(high_memory) || (fp->f_flags & O_SYNC))
		vma->vm_flags |= VM_IO;

	vma->vm_flags |= VM_RESERVED;
	vma->vm_flags |= VM_DONTEXPAND;

//        vma->vm_page_prot=pgprot_noncached(vma->vm_page_prot);

	/* do the remap */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10))
	if (remap_pfn_range(
		    vma,
		    vma->vm_start,
		    physical >> PAGE_SHIFT,
		    vsize,
		    vma->vm_page_prot)) {
		DA( (KERN_WARNING "nac%u: Failed remap!\n", sc->unit) );
		return -EAGAIN;
	}
#else
	if (remap_page_range(
		    vma,
		    vma->vm_start, physical,
		    vsize, vma->vm_page_prot)) {
		DA( (KERN_WARNING "nac%u: Failed remap!\n", sc->unit) );
		return -EAGAIN;
	}
#endif
	return 0;
}

static int
nac_iom_mmap(struct file *fp, struct vm_area_struct * vma)
{
	nac_softc_t	*sc = fp->private_data;
	loff_t		off, physical, vsize, psize;

	off = (off_t)vma->vm_pgoff << PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;

	if(sc->buf == NULL) {
		DA( (KERN_WARNING "nac%u: mmap: no IO memory assigned to this nac\n", sc->unit) );
		return -ENOMEM;
	}
	physical = (uint32_t)pci_resource_start(sc->device, 0) + off;
	psize = pci_resource_len(sc->device, 0) - off;
	
	if (off & (PAGE_SIZE-1))
		return -ENXIO; /* needs aligned offsets */
	if (vsize > psize)
		return -EINVAL; /* spans too high */
	
        /*
	 * Don't dump addresses that are not real memory to a core file.
	 */
	if (off >= __pa(high_memory) || (fp->f_flags & O_SYNC))
		vma->vm_flags |= VM_IO;

	vma->vm_flags |= VM_RESERVED;

	/* do the remap */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10))
	if (remap_pfn_range(
		    vma,
		    vma->vm_start,
		    physical >> PAGE_SHIFT,
		    vsize,
		    vma->vm_page_prot)) {
		DA( (KERN_WARNING "nac%u: Failed remap!\n", sc->unit) );
		return -EAGAIN;
	}
#else
	if (remap_page_range(
		    vma,
		    vma->vm_start, physical,
		    vsize, vma->vm_page_prot)) {
		DA( (KERN_WARNING "nac%d: Failed IO remap!\n", sc->unit) );
		return -EAGAIN;
	}
#endif
	return 0;
}

int __devinit
nac_init_one(struct pci_dev *dev, const struct pci_device_id *ent)
{
	int		rc;
	int		unit, error;
	nac_softc_t	*sc=NULL;
	unsigned long	iom;
	uint32_t	iom_size;
	size_t		hole_size;
	char		rev;
	int		c;
	uint8_t		lat;
#ifndef KERNEL_26X
	char		devname[8];
#endif
	
	int             irq_flags;
	
//	irq_flags = SA_SHIRQ;
	
	DD( (KERN_DEBUG "nac: nac_init_one entry\n") );

	unit = nac_find_unattached();
	DD( (KERN_DEBUG "nac: discovered unit %d\n", unit) );

	if (unit < 0) {
		DA( (KERN_WARNING "Too many NAC devices loaded (>%d)\n", NAC_MAX_BOARDS) );
		return -ENOMEM;
	}
	sc = &nac_array[unit];
	sc->unit = unit;
	sc->flags |= NACF_ATTACHED;
	sc->device = dev;
#ifdef KERNEL_26X
	dev->dev.driver_data = sc;
#else
	dev->driver_data = sc;
#endif
	for(c=0;c<NAC_STREAM_MAX;c++) {
		sc->lock[c].pid=0;
		sc->lock[c].tgid=0;
	}
	sema_init(&sc->lock_sem, 1);

 //       sc->duck = duck_new();

	rc = pci_enable_device(dev);
	if (rc) return rc;
	//DD( (KERN_DEBUG "nac%d: device enabled\n", unit) );

	pci_set_master(dev); /* returns void */
	//DD( (KERN_DEBUG "nac%d: master enabled\n", unit) );

	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat < NAC_MIN_LATENCY) {
		DA( (KERN_DEBUG "nac%d: Setting latency timer to %d\n", unit, NAC_MIN_LATENCY) );
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	}

#ifdef HAVE_PCI_SET_MWI
	//DD( (KERN_DEBUG "nac%d: setting mwi\n", unit) );
	rc = pci_set_mwi(dev);
	if (rc) {
		/* This isn't a reason to fail the pci initialization, just output a warning */
		 DA( (KERN_WARNING "nac%d: mwi disabled\n", unit) );
	} else {
		//DD( (KERN_DEBUG "nac%d: mwi enabled\n", unit) );
	}
#endif

	/* PCI IO memory */
	iom = pci_resource_start(dev, 0);
	iom_size = pci_resource_len(dev, 0);

	if(check_mem_region(iom, iom_size)) {
		DA( (KERN_WARNING "nac%d: Fatal - IO memory 0x%08lx already in use\n", unit, iom) );
		return -EBUSY;
	}

	request_mem_region(iom, iom_size, "nac");
	DD( (KERN_DEBUG "nac%d: iom requested 0x%08lx\n", unit, iom) );

	sc->mmio = ioremap_nocache(iom, iom_size);
	sc->mmio_size = iom_size;
	DD( (KERN_DEBUG "nac%d: iom mmapped from 0x%08lx\n", unit, (long)sc->mmio) );

       *(sc->mmio+0x900FC)=0x1;

	/* Large PC Memory buffer */
	hole_size = 0;
	if((sc->mmbuf = nac_mem_malloc(&hole_size, 0)) == NULL) {
		DA( (KERN_WARNING "nac%d: Fatal - nacmem_malloc failed\n",
		      unit) );
		return -ENOMEM;
	}
	DD( (KERN_DEBUG "nac%d: hole allocated 0x%08lx\n", unit, (long)sc->mmbuf) );

	sc->buf = (uint8_t*)__pa(sc->mmbuf);	/* physical address */
	sc->buf_size = hole_size;

        error = pci_enable_msi(dev);
	if (error < 0)
        {
		DA((KERN_WARNING "nac%d: Fatal - MSI IRQ request Error %d: ", unit, error));
                return error;
        }
	
//	irq_flags &= ~SA_SHIRQ;
	
	/* IRQ line and handler */
	if (dev->irq) {
		error = request_irq(dev->irq, nac_interrupt,
				    irq_flags, "nac", (void *)sc);
		if (error < 0) {
			DA( (KERN_WARNING "nac%d: Fatal - IRQ request Error %d: ", unit, error) );
			if (error == -EBUSY)
				DA( (KERN_WARNING "IRQ Busy %d\n", dev->irq) );
			if (error == -EINVAL)
				DA( (KERN_WARNING "Bad irq number or handler %d\n", dev->irq) );
			return error;
		}
	} else {
		DA( (KERN_WARNING "nac%d: Fatal - no irq allocated\n",
		       unit) );
		return -EIO;
	}
	sc->irq = dev->irq;
	//DD( (KERN_DEBUG "nac%d: irq mapped %d\n", unit, sc->irq) );
/*
	nac_iowrite(0xffff, IntEn);
	DD( (KERN_DEBUG "nac%d: irqs enabled\n", unit) );
*/
#ifndef KERNEL_26X
	/* create devfs entries */
	sprintf(devname, "nac%i", unit);
	sc->devfs_nac=devfs_register(NULL, devname,
					  DEVFS_FL_AUTO_DEVNUM | DEVFS_FL_DEFAULT,
					  0,0, S_IFCHR | S_IRUGO | S_IWUGO,
					  nac_fops_array[NACMINOR_NAC], sc);
	if(sc->devfs_nac<0) {
		DA( (KERN_WARNING "nac%d: devfs %s register failed (%p)\n",
		       unit, devname, sc->devfs_nac) );
	}

	sprintf(devname, "nacmem%i", unit);
	sc->devfs_mem=devfs_register(NULL, devname,
					  DEVFS_FL_AUTO_DEVNUM | DEVFS_FL_DEFAULT,
					  0,0, S_IFCHR | S_IRUGO | S_IWUGO,
					  nac_fops_array[NACMINOR_MEM], sc);
	if(sc->devfs_mem<0) {
		DA( (KERN_WARNING "nac%d: devfs %s register failed (%p)\n",
		       unit, devname, sc->devfs_mem) );
	}

	sprintf(devname, "naciom%i", unit);
	sc->devfs_iom=devfs_register(NULL, devname,
					  DEVFS_FL_AUTO_DEVNUM | DEVFS_FL_DEFAULT,
					  0,0, S_IFCHR | S_IRUGO | S_IWUGO,
					  nac_fops_array[NACMINOR_IOM], sc);
	if(sc->devfs_iom<0) {
		DA( (KERN_WARNING "nac%d: devfs %s register failed (%p)\n",
		       unit, devname, sc->devfs_iom) );
	}

	//DD( (KERN_DEBUG "nac%d: devfs done\n", unit) );
#endif

	/* Save PCI Regs for later resets/reloads */
	nac_pciconfig_save(sc);
	//DD( (KERN_DEBUG "nac%d: saved pci config\n", unit) );

	pci_read_config_byte(dev, PCI_REVISION_ID, &rev);
	
	/* Success: Inform the user */
	DA( (KERN_INFO "nac%d: Rev %d at 0x%p irq %d buf %luMB\n",
	       unit,/* nac_device_name(dev->device, 1),*/ rev, (int*)iom, sc->irq, (unsigned long)hole_size>>20) );

     //  duck_init(sc);

#ifdef KERNEL_26X
	cdev_init(&sc->cdev_nac, nac_fops_array[NACMINOR_NAC]);
       	sc->cdev_nac.owner = THIS_MODULE;
	kobject_set_name(&sc->cdev_nac.kobj, "nac%d", unit);

	cdev_init(&sc->cdev_mem, nac_fops_array[NACMINOR_MEM]);
       	sc->cdev_mem.owner = THIS_MODULE;
	kobject_set_name(&sc->cdev_mem.kobj, "nacmem%d", unit);

	cdev_init(&sc->cdev_iom, nac_fops_array[NACMINOR_IOM]);
       	sc->cdev_iom.owner = THIS_MODULE;
	kobject_set_name(&sc->cdev_iom.kobj, "naciom%d", unit);

	if((error=cdev_add(&sc->cdev_nac, nac_dev|(NACMINOR_NAC*NAC_MAX_BOARDS+unit), 1))) {
		DA( (KERN_WARNING "nac%d: cdev nac register failed (%d)\n",
		       unit, error) );		
	}
	//DD( (KERN_DEBUG "Added nac%d mtype %d major %d minor %d dev_t 0x%08x\n", unit, NACMINOR_NAC, nac_dev, NACMINOR_NAC*NAC_MAX_BOARDS+unit, nac_dev|(NACMINOR_NAC*NAC_MAX_BOARDS+unit)) );

	if((error=cdev_add(&sc->cdev_mem, nac_dev|(NACMINOR_MEM*NAC_MAX_BOARDS+unit), 1))) {
		DA( (KERN_WARNING "nac%d: cdev nacmem register failed (%d)\n",
		       unit, error) );		
	}
	//DD( (KERN_DEBUG "Added nac%d mtype %d minor %d dev_t 0x%08x\n", unit, NACMINOR_MEM, NACMINOR_MEM*NAC_MAX_BOARDS+unit, nac_dev|(NACMINOR_MEM*NAC_MAX_BOARDS+unit)) );

	if((error=cdev_add(&sc->cdev_iom, nac_dev|(NACMINOR_IOM*NAC_MAX_BOARDS+unit), 1))) {
		DA( (KERN_WARNING "nac%d: cdev naciom register failed (%d)\n",
		       unit, error) );		
	}
	//DD( (KERN_DEBUG "Added nac%d mtype %d minor %d dev_t 0x%08x\n", unit, NACMINOR_IOM, NACMINOR_IOM*NAC_MAX_BOARDS+unit, nac_dev|(NACMINOR_IOM*NAC_MAX_BOARDS+unit)) );

#endif

	return 0;
}

void __devexit
nac_remove_one(struct pci_dev *dev)
{
#ifdef KERNEL_26X
	nac_softc_t *sc = dev->dev.driver_data;
#else
	nac_softc_t *sc = dev->driver_data;	
#endif
	if(!sc)	/* shouldn't be possible, paranoia */
		return;

#ifdef KERNEL_26X
	cdev_del(&sc->cdev_nac);

	cdev_del(&sc->cdev_mem);

	cdev_del(&sc->cdev_iom);

#else
	/* devfs entries */
	if(sc->devfs_nac)
		devfs_unregister(sc->devfs_nac);

	if(sc->devfs_mem)
		devfs_unregister(sc->devfs_mem);

	if(sc->devfs_iom)
		devfs_unregister(sc->devfs_iom);
#endif

	/* IRQ */
	if(sc->irq) {
		free_irq(sc->irq, sc);
		DA((KERN_INFO "IRQ Freed\n"));
	}

//       if(sc->duck){
//	   	duck_destroy(sc);
  //     }

	/* Large PC buffer */
	if(sc->mmbuf) {
		nac_mem_free(sc->mmbuf);
	}

	/* PCI IO memory */
	if(sc->mmio) {
		iounmap(sc->mmio);
		release_mem_region(pci_resource_start(dev,0), pci_resource_len(dev,0));
	}

	/* clear stored parameters */
	memset(sc, 0, sizeof(nac_softc_t));
}

struct file_operations nac_fops = {
	/* nac entry */
#ifdef KERNEL_26X
	owner:  THIS_MODULE,
#endif
	read:	nac_mem_read, /* use mem entry for now */
	write:	nac_mem_write,/* use mem entry for now */
	ioctl:	nac_ioctl,
	mmap:	nac_mmap,
	open:	nac_open,
	release: nac_release
};

struct file_operations nac_mem_fops = {
	/* mem entry */
#ifdef KERNEL_26X
	owner:  THIS_MODULE,
#endif
	read:	nac_mem_read,
	write:	nac_mem_write,
	ioctl:	nac_ioctl,
	mmap:	nac_mmap,
	open:	nac_open,
	release: nac_release
};

struct file_operations nac_iom_fops = {
	/* iom entry */
#ifdef KERNEL_26X
	owner:  THIS_MODULE,
#endif
	read:	nac_iom_read,
	write:	nac_iom_write,
	ioctl:	nac_ioctl,
	mmap:	nac_iom_mmap,
	open:	nac_open,
	release: nac_release
};


static struct pci_driver nac_pci_driver = {
       name:           "nac",
       probe:          nac_init_one,
       remove:	       __devexit_p(nac_remove_one),
       id_table:       nac_pci_tbl,
};

void __exit
nac_cleanup(void)
{
#ifdef KERNEL_26X
	if(nac_dev)
		unregister_chrdev_region(nac_dev, NAC_MAX_BOARDS*NACMINOR_MAX);
#else
	if (nac_major)
		devfs_unregister_chrdev(nac_major, "nac");
#endif
	pci_unregister_driver(&nac_pci_driver);
}

int __init
nac_init(void)
{
	int	err=0;
#ifndef KERNEL_26X
	SET_MODULE_OWNER(&nac_fops);
	SET_MODULE_OWNER(&nac_mem_fops);
	SET_MODULE_OWNER(&nac_iom_fops);
#endif
	DA( (KERN_INFO "nac: Version %s\n", knacReleaseVersion) );

#ifdef KERNEL_26X
	if (alloc_chrdev_region( &nac_dev, 0, NAC_MAX_BOARDS*NACMINOR_MAX, "nac" ) < 0) {
		DA( (KERN_WARNING "nac: Couldn't get major error %d\n", err) );
		goto fail_ch;
        }

	//DD( (KERN_DEBUG "Got major %d\n", nac_dev) );

#endif
//	err = pci_module_init(&nac_pci_driver);
	err = pci_register_driver(&nac_pci_driver);

	if (err) {
		DA( (KERN_WARNING "nac: Init failure %d\n", err) );
		goto fail_pci;
	}

#ifndef KERNEL_26X
	/* request a major, does nothing if devfs is used */
	if ((nac_major = devfs_register_chrdev(0, "nac", &nac_fops)) < 0) {
		DA( (KERN_WARNING "nac: Couldn't get major error %d\n", err) );
		goto fail_ch;
	}
#endif
	return 0;

 fail_ch:
	nac_cleanup();
 fail_pci:
	return err;
}

MODULE_AUTHOR("NAC Team ");
MODULE_DESCRIPTION("device driver module for  NAC network capture cards");
MODULE_SUPPORTED_DEVICE("nac");
MODULE_LICENSE("GPL");
module_init(nac_init);
module_exit(nac_cleanup);


