/* cf. bsps/powerpc/motorola_powerpc/bootloader/head.S
 * and bsps/powerpc/motorola_powerpc/bootloader/ppcboot.lds
 * in the RTEMS source tree.
 *
 * This file provides the symbols _binary_rtems_gz_start
 * and _binary_rtems_gz_end normally/originally intended
 * to be created implicitly by inclusion of rtems.gz
 * by ppcboot.lds.
 *
 * Depending on the hard-coded "rtems.gz" name allows
 * problems with make -j
 */

#define STRINGIFY1(x) #x
#define STRINGIFY(x)     STRINGIFY1(x)

.section ".rodata"

.global _binary_rtems_gz_start
.global _binary_rtems_gz_end

_binary_rtems_gz_start:
.incbin STRINGIFY(RTEMS_GZ)
_binary_rtems_gz_end:
