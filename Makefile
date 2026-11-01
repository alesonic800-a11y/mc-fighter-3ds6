# Moncloa's Fighter 3DS - compilacion directa con devkitARM + libctru
ifeq ($(strip $(DEVKITPRO)),)
$(error Define DEVKITPRO (p.ej. export DEVKITPRO=/opt/devkitpro))
endif
export DEVKITARM ?= $(DEVKITPRO)/devkitARM
export PATH := $(DEVKITPRO)/tools/bin:$(DEVKITARM)/bin:$(PATH)

TARGET      := moncloas_fighter
APP_TITLE   := Moncloa's Fighter
APP_DESC    := Port 3DS - 5 luchadores
APP_AUTHOR  := Port
PRODUCT     := CTR-P-MCFT
UNIQUE_ID   := 0xF8E01
CTRULIB     := $(DEVKITPRO)/libctru

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS  := -std=gnu11 -O2 -Wall -g -mword-relocations -ffunction-sections -fdata-sections $(ARCH) -D__3DS__ -I$(CTRULIB)/include
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -L$(CTRULIB)/lib
LIBS    := -lctru -lm

.PHONY: all 3dsx cia assets clean
all: 3dsx
3dsx: $(TARGET).3dsx
cia: $(TARGET).cia

assets:                      # solo hace falta si tienes la carpeta del juego web (ya viene convertido en romfs/)
	python3 tools/convert_assets.py $(WEB_GAME_DIR) romfs

$(TARGET).elf: source/main.c source/audio.c source/audio.h
	arm-none-eabi-gcc $(CFLAGS) source/main.c source/audio.c $(LDFLAGS) $(LIBS) -o $@

$(TARGET).smdh: gfx/icon.png
	smdhtool --create "$(APP_TITLE)" "$(APP_DESC)" "$(APP_AUTHOR)" gfx/icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh $(shell find romfs -type f)
	3dsxtool $(TARGET).elf $@ --smdh=$(TARGET).smdh --romfs=romfs

banner.bnr: gfx/banner.png gfx/audio.wav
	bannertool makebanner -i gfx/banner.png -a gfx/audio.wav -o $@

$(TARGET).cia: $(TARGET).elf $(TARGET).smdh banner.bnr app.rsf $(shell find romfs -type f)
	makerom -f cia -o $@ -elf $(TARGET).elf -rsf app.rsf -icon $(TARGET).smdh -banner banner.bnr -exefslogo -target t \
	  -DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE=$(PRODUCT) -DAPP_UNIQUE_ID=$(UNIQUE_ID) -DAPP_ROMFS=romfs

clean:
	rm -f $(TARGET).elf $(TARGET).3dsx $(TARGET).cia $(TARGET).smdh banner.bnr
