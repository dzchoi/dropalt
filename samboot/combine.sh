#!/usr/bin/dash
# combine.sh  Generate the combined firmware for mdloader-based flashing.
#
# Usage: combine.sh <bkswrst.bin> <dropalt-fw.bin> <output.bin> <fw_offset>
#
# fw_offset: byte offset at which dropalt-fw.bin is placed in the output
#            (= 0x20000 - 0x4000 = 0x1C000 = 114688); accepts 0x hex or decimal.

set -e

[ $# -eq 4 ] || {
    printf 'Usage: %s <bkswrst.bin> <fw.bin> <output.bin> <fw_offset>\n' "$0" >&2
    exit 1
}

bkswrst=$1  fw=$2  out=$3
fw_offset=$(printf '%d' "$4")   # convert 0x hex or plain decimal to decimal

bkswrst_size=$(wc -c < "$bkswrst")
fw_size=$(wc -c < "$fw")

if [ "$bkswrst_size" -gt "$fw_offset" ]; then
    printf 'error: BKSWRST binary (%d B) exceeds fw_offset (%d B = 0x%x)\n' \
        "$bkswrst_size" "$fw_offset" "$fw_offset" >&2
    exit 1
fi

padding=$(( fw_offset - bkswrst_size ))

mkdir -p "$(dirname "$out")"

{
    cat "$bkswrst"
    dd if=/dev/zero bs=1 count="$padding" 2>/dev/null | tr '\000' '\377'
    cat "$fw"
} > "$out"

printf '[samboot] %s\n'               "$out"
printf '[samboot]   BKSWRST:  %d B\n' "$bkswrst_size"
printf '[samboot]   padding:  %d B\n' "$padding"
printf '[samboot]   firmware: %d B\n' "$fw_size"
printf '[samboot]   total:    %d B\n' "$(( fw_offset + fw_size ))"
