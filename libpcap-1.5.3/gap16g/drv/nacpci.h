#ifndef NACPCI_H
#define NACPCI_H

#ifndef __KERNEL__ 
#ifndef _WIN32
# include <inttypes.h>
#endif /* _WIN32 */
#endif

#define PCI_VENDOR_ID_NAC	0xeaea
#define PCI_DEVICE_ID_POS_10G 	0x5003
#define PCI_DEVICE_ID_POS_2G5 	0x5051
#define PCI_DEVICE_ID_GE_10G 	0x5052
#define PCI_DEVICE_ID_GE_4G 	0x2003
 

#ifdef __KERNEL__
extern struct pci_device_id nac_pci_tbl[];
#endif


char * nac_device_name(uint32_t device, uint32_t flag);


#endif /* NACPCI_H */
