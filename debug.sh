#!/bin/bash

# this bash script is called by "make debug"

echo "START debug.sh"

# start the gdb client, connect to the remote server, load the program onto
# the target, set a breakpoint at main, and enable the text user interface
# also disable confirmations so you can quit by just using "q"
gdb-multiarch -ex "set confirm off" \
    -ex "target extended-remote /dev/ttyBmpGdb" -ex "monitor swd_scan" -ex "attach 1" \
    -ex "load cm0plus/bin/mstrcmp_cm0plus.elf" \
    -ex "file cm4/bin/mstrcmp_cm4.elf" -ex "load" \
    -ex "add-symbol-file cm0plus/bin/mstrcmp_cm0plus.elf"


echo "DONE"
