ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

ifeq (THUMB,$(findstring THUMB, $(PLATFORM)))
OPT=
TARGETDIR=$(ZDIR)\system\apps\Osmo4
ICONTARGETFILENAME=$(TARGETDIR)\Osmo4.aif
else
TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\osmo4_aif.mif
OPT=/X
endif


ICONDIR=..\..\applications\osmo4_sym\res

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing

RESOURCE :	
	mifconv $(ICONTARGETFILENAME) $(OPT) $(ICONDIR)\osmo4.svg
		
FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo $(ICONTARGETFILENAME)

FINAL : do_nothing

