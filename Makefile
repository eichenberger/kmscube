CC      ?= ${CROSS_COMPILE}gcc
CFLAGS  ?= -pedantic -Wall -I /usr/include/libdrm -I .
LDFLAGS ?= -L/usr/lib/aarch64-linux-gnu/ -l:libgbm.so.1 -ldrm -lGLESv2 -lEGL -lm

OBJ = common.o cube-shadertoy.o cube-smooth.o cube-tex.o drm-atomic.o drm-common.o drm-legacy.o esTransform.o frame-512x512-NV12.o frame-512x512-RGBA.o kmscube.o perfcntrs.o test.o
PROGNAME = kmscube 

exec_prefix ?= /usr
bindir ?= $(exec_prefix)/bin

all: $(OBJ)
	$(CC) $(CFLAGS) -o $(PROGNAME) $(OBJ) $(LDFLAGS)

install: all
	install -d $(DESTDIR)$(bindir)
	install -m 0755 $(PROGNAME) $(DESTDIR)$(bindir)

clean:
	@echo "Clean object files"
	@rm -f $(OBJ)
	@rm -f $(PROGNAME)

%.o: %.c
	$(CC) $(CFLAGS) -c $<
