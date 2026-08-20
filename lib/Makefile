lib-y += compiler-rt.o

# simplefb
lib-$(CONFIG_SIMPLE_FB) += simplefb/simplefb.o

# debug
lib-y += console/console.o

# unic (neatlibc still temporarily resides)
lib-y += unic/string.o

# Embedded Linux kernel payload decompression
lib-$(CONFIG_KERNEL_DECOMPRESS) += decompress/kernel.o
lib-$(CONFIG_KERNEL_DECOMPRESS_GZIP) += \
	decompress/gzip.o \
	decompress/tinf/tinflate.o \
	decompress/tinf/tinfgzip.o \
	decompress/tinf/crc32.o
lib-$(CONFIG_KERNEL_DECOMPRESS_LZ4) += decompress/lz4.o
lib-$(CONFIG_KERNEL_DECOMPRESS_XZ) += \
	decompress/xz.o \
	decompress/xz/xz_crc32.o \
	decompress/xz/xz_dec_stream.o \
	decompress/xz/xz_dec_lzma2.o \
	decompress/xz/xz_dec_bcj.o

# The bit-at-a-time decoders are impractically slow at the loader's default
# -O0. Keep board and handoff code unoptimized while optimizing only the pure
# software decompression routines.
CFLAGS_gzip.o += -O2 -mstrict-align
CFLAGS_tinflate.o += -O2 -mstrict-align
CFLAGS_tinfgzip.o += -O2 -mstrict-align
CFLAGS_crc32.o += -O2 -mstrict-align
CFLAGS_lz4.o += -O2 -mstrict-align
CFLAGS_xz.o += -O2 -mstrict-align
CFLAGS_xz_crc32.o += -O2 -mstrict-align
CFLAGS_xz_dec_stream.o += -O2 -mstrict-align
CFLAGS_xz_dec_lzma2.o += -O2 -mstrict-align
CFLAGS_xz_dec_bcj.o += -O2 -mstrict-align

lib-$(CONFIG_LIBFDT) += \
	libfdt/fdt.o \
	libfdt/fdt_ro.o \
	libfdt/fdt_rw.o \
	libfdt/fdt_wip.o \
	libfdt/fdt_sw.o \
	libfdt/fdt_strerror.o

ccflags-y += -I$(srctree)/include/lib/libfdt
ccflags-y += -I$(srctree)/lib/decompress/tinf
