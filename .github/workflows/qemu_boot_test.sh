#!/bin/sh
#
# Functional boot test for the QEMU "virt" board (configs/virt_defconfig).
#
# Builds uniLoader once per supported kernel payload format (raw, gzip, LZ4,
# XZ), embedding a tiny synthetic arm64 payload (qemu-fake-kernel.S) instead
# of a real kernel, then boots the result under qemu-system-aarch64 and
# checks the serial log to confirm the loader actually detected/decompressed
# the payload and jumped into it.
#
# CI calls this script from the project root.
#

set -e

procs=$(nproc --all)
makeargs="CROSS_COMPILE=aarch64-linux-gnu- -j$procs"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

payload_entry=$(sed -n 's/^CONFIG_PAYLOAD_ENTRY=//p' configs/virt_defconfig)

echo "Assembling synthetic test payload (link address $payload_entry)..."
aarch64-linux-gnu-as -o "$work/fake-kernel.o" .github/workflows/qemu-fake-kernel.S
aarch64-linux-gnu-ld -Ttext="$payload_entry" --entry=_start -nostdlib -static \
	-o "$work/fake-kernel.elf" "$work/fake-kernel.o"
aarch64-linux-gnu-objcopy -O binary "$work/fake-kernel.elf" "$work/Image"

gzip -n -9 -c "$work/Image" > "$work/Image.gz"
lz4 -l -9 -f -q "$work/Image" "$work/Image.lz4"
xz --check=crc32 --arm64 --lzma2=preset=9e -f -c "$work/Image" > "$work/Image.xz"

dd if=/dev/zero of=blob/dtb bs=1K count=4 status=none
dd if=/dev/zero of=blob/ramdisk bs=1K count=4 status=none

failures=0

test_format() {
	name=$1
	payload=$2
	tag=$3

	echo "=== Testing $name kernel payload ==="
	cp "$payload" blob/Image

	make distclean $makeargs >/dev/null
	make virt_defconfig $makeargs >/dev/null
	make $makeargs >/dev/null

	log="$work/qemu-$name.log"
	timeout 10 qemu-system-aarch64 -M virt -cpu cortex-a57 -m 512 \
		-nographic -no-reboot -kernel uniLoader.o 2>&1 | tr -d '\r' >"$log" || true

	cat "$log"

	if grep -q '\[EMERG\]' "$log"; then
		echo "FAIL ($name): CPU exception during boot"
		failures=$((failures + 1))
		return
	fi
	if [ -n "$tag" ] && ! grep -q "^\[INFO\] $tag compressed kernel image, decompressing" "$log"; then
		echo "FAIL ($name): $tag decompression path was not exercised"
		failures=$((failures + 1))
		return
	fi
	if ! grep -q 'UNILOADER-QEMU-BOOT-OK' "$log"; then
		echo "FAIL ($name): loader never jumped into the decompressed payload"
		failures=$((failures + 1))
		return
	fi
	echo "PASS ($name)"
}

test_format raw  "$work/Image"     ""
test_format gzip "$work/Image.gz"  "GZIP"
test_format lz4  "$work/Image.lz4" "LZ4"
test_format xz   "$work/Image.xz"  "XZ"

if [ "$failures" -ne 0 ]; then
	echo "$failures QEMU boot test(s) failed."
	exit 1
fi

echo "All QEMU image-type boot tests passed."
