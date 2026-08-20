Kernel payload compression
==========================

uniLoader can optionally embed a compressed Linux kernel and expand it directly
into ``CONFIG_PAYLOAD_ENTRY`` before the architecture handoff.

Enable::

    CONFIG_KERNEL_DECOMPRESS=y

The individual decoders are independently selectable:

* ``CONFIG_KERNEL_DECOMPRESS_GZIP`` - Linux ``Image.gz``
* ``CONFIG_KERNEL_DECOMPRESS_LZ4`` - Linux legacy-framed ``Image.lz4``
* ``CONFIG_KERNEL_DECOMPRESS_XZ`` - Linux ``Image.xz``

All three default to enabled once ``CONFIG_KERNEL_DECOMPRESS`` is selected.
Raw ``Image`` remains supported and keeps the old direct-copy behavior.

Building a payload
------------------

The kernel build already has matching arm64 targets::

    make Image.gz
    make Image.lz4
    make Image.xz

Point ``CONFIG_KERNEL_PATH`` at the artifact you want to embed, for example::

    CONFIG_KERNEL_PATH="blob/Image.xz"

or simply copy the chosen artifact to whatever path is already configured.
The loader detects the payload format from its magic bytes; the filename is not
used for detection.

Format notes
------------

``Image.gz`` is a normal gzip stream. The gzip CRC32 and ISIZE trailer are
validated before boot.

``Image.lz4`` support intentionally targets the legacy LZ4 stream emitted by
the Linux kernel build (``lz4 -l``; magic ``0x184c2102``). Modern LZ4 frame
format is not accepted as a kernel payload.

``Image.xz`` uses XZ Embedded in single-call mode with CRC32 verification. The
ARM, ARM-Thumb and ARM64 BCJ filters used by Linux ``xz_wrap.sh`` are accepted.
Single-call mode uses the output buffer as the LZMA2 dictionary, so a large XZ
dictionary does not require a second dictionary-sized allocation in uniLoader.

Memory safety limit
-------------------

``CONFIG_KERNEL_DECOMPRESS_MAX_SIZE`` bounds how many decompressed bytes may be
written beginning at ``CONFIG_PAYLOAD_ENTRY``. The default is 64 MiB. Increase
it if necessary, but first verify that the resulting range does not overlap the
ramdisk, DT, framebuffer, persistent logs, or other bootloader-reserved data.
The limit is deliberately not applied to a raw ``Image`` so existing boards do
not change behavior merely by enabling compressed-payload support.

Herolte limits decompression to 32 MiB because its payload starts at
``0x90000000`` and the ramoops reservation starts at ``0x92000000``.

On AArch64, a successfully decompressed payload is also checked for the Linux
arm64 Image magic before uniLoader jumps to it.
