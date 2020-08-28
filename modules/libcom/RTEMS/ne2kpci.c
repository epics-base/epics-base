/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Wrapper around the ISA ne2000 driver to support detection of the PCI variant.
 * Can be used with the ne2k_pci device provided by QEMU.
 *
 * eg. "qemu-system-i386 ... -net nic,model=ne2k_pci"
 */

#include <stdio.h>
#include <inttypes.h>
#include <bsp.h>
#include <rtems/pci.h>
#include <rtems/rtems_bsdnet.h>

/* The plain ISA driver attach()
 * which doesn't (can't?) do any test to see if
 * the HW is really present
 */
extern int
rtems_ne_driver_attach (struct rtems_bsdnet_ifconfig *config, int attach);

int
rtems_ne2kpci_driver_attach (struct rtems_bsdnet_ifconfig *config, int attach)
{
    uint8_t  irq;
    uint32_t bar0;
    int B, D, F, ret;
    printk("Probing for NE2000 on PCI (aka. Realtek 8029)\n");

    if(pci_find_device(PCI_VENDOR_ID_REALTEK, PCI_DEVICE_ID_REALTEK_8029, 0, &B, &D, &F))
    {
        printk("Not found\n");
        return 0;
    }

    printk("Found %d:%d.%d\n", B, D, F);

    ret = pci_read_config_dword(B, D, F, PCI_BASE_ADDRESS_0, &bar0);
    ret|= pci_read_config_byte(B, D, F, PCI_INTERRUPT_LINE, &irq);

    if(ret || (bar0&PCI_BASE_ADDRESS_SPACE)!=PCI_BASE_ADDRESS_SPACE_IO)
    {
        printk("Failed reading card config\n");
        return 0;
    }

    config->irno = irq;
    config->port = bar0&PCI_BASE_ADDRESS_IO_MASK;

    printk("Using port=0x%x irq=%u\n", (unsigned)config->port, config->irno);

    return rtems_ne_driver_attach(config, attach);
}
