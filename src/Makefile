

all: bld_errSym dlsource eddsource seqsource

bld_errSym:
	cd libUnix; 	make TARGET=Unix 
	cd libvxWorks; 	make TARGET=vxWorks

dlsource:
	cd opi/dl ; /bin/sh build.it

eddsource:
	cd opi/edd ; /bin/sh build.it

seqsource:
	cd sequencer ; /bin/sh build.it
