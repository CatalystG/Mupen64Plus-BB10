QMAKE_TARGET  = mupen64plus-cascades-ui
QMAKE         = $(QNX_HOST)/usr/bin/qmake
TARGET        = $(QMAKE_TARGET)


all: Makefile $(QMAKE_TARGET)

clean:
	$(MAKE) -C ./arm -f Makefile distclean
	#$(MAKE) -C ./x86 -f Makefile distclean	


Makefile: FORCE	
	$(QMAKE) -spec unsupported/blackberry-armv7le-qcc -o arm/Makefile $(QMAKE_TARGET).pro CONFIG+=device
	#$(QMAKE) -spec unsupported/blackberry-x86-qcc -o x86/Makefile $(QMAKE_TARGET).pro CONFIG+=simulator

FORCE:

$(QMAKE_TARGET): device

device:
	$(MAKE) -C ./arm -f Makefile all

Device-Debug: Makefile
	$(MAKE) -C ./arm -f Makefile debug
	
Device-Release: Makefile
	$(MAKE) -C ./arm -f Makefile release

simulator:
	$(MAKE) -C ./x86 -f Makefile all

Simulator-Debug: Makefile
	$(MAKE) -C ./x86 -f Makefile debug
 