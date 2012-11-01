#An attempt at a Makefile without actually knowing what I'm trying to compile. CL 12-5-7
PROGRAM = esc_test
SOURCES = $(PROGRAM).cpp embedded-atmel-twi/TWISlaveMem14.c
OBJECTS = $(patsubst %.cpp,%.o, $(filter %.cpp, $(SOURCES))) \
          $(patsubst %.c,%.o, $(filter %.c,$(SOURCES)))
MAPFILE=$(PROGRAM).map

LOCAL_INCLUDES = 
LOCAL_LDFLAGS  = -Wl,-Map=$(MAPFILE),--cref
# ,--section-start=.i2c_memory=0x800100,--section-start=.data=0x800400

ELF = $(PROGRAM).elf
HEX = $(PROGRAM).hex
EEP = $(PROGRAM).eep

all: ${HEX}

include Makefile.config

%.elf : ${OBJECTS}
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LOCAL_LDFLAGS) -o $@

%.elf.dis : %.elf
	$(OBJDUMP) -S $< > $@

%.hex.dis : %.hex
	$(OBJDUMP) -m avr -D $< > $@

%.o.dis : %.o
	$(OBJDUMP) -m avr -S $< > $@

upload: $(HEX)
	$(AVRDUDE) -e -C$(AVRDUDE_CONF) -p$(MCU) -c$(PROGRAMMER) -P$(PORT) -b$(BAUD) -D -Uflash:w:$(HEX):i

$(EXEC): $(OBJECTS)
