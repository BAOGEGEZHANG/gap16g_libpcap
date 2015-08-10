#define KBUILD_MODNAME "nac"

#if defined(__linux__)
# ifdef __KERNEL__
#  include <linux/kernel.h>
#  include <linux/init.h>
#  include <linux/pci.h>
# else
#  include <stdio.h>
# endif
#else
# include <stdio.h>
#endif


#include "nacpci.h"


#ifdef __linux__
# ifdef __KERNEL__
struct pci_device_id nac_pci_tbl[] __devinitdata = {
		{ PCI_DEVICE(PCI_VENDOR_ID_NAC,	PCI_DEVICE_ID_POS_10G) },
		{ PCI_DEVICE(PCI_VENDOR_ID_NAC,	PCI_DEVICE_ID_POS_2G5) },
		{ PCI_DEVICE(PCI_VENDOR_ID_NAC,	PCI_DEVICE_ID_GE_10G) },
		{ PCI_DEVICE(PCI_VENDOR_ID_NAC,	PCI_DEVICE_ID_GE_4G) },
		{0,}
};
# endif /* __KERNEL__ */
#endif /* __linux__ */

char * nac_device_name(uint32_t device, uint32_t flag)
{
	static char retbuf[64];

	switch(device) {
		/*
		 * DAG3 series
		 */
	  case PCI_DEVICE_ID_POS_10G:		return "NAC 10GPOS";
	  case PCI_DEVICE_ID_POS_2G5:	return "NAC 2.5GPOS";
	  case PCI_DEVICE_ID_GE_10G:	return "NAC 10GE";
	  case PCI_DEVICE_ID_GE_4G:	return "NAC 4XGE";
		/*
		 * None of the above
		 */
	  default:
		if(flag) {
			(void)snprintf(retbuf, 64, "unknown card 0x%.4x", device);
			return retbuf;
		} else {
			return NULL;
		}
	}

	/*NOTREACHED*/
	return NULL;
}


