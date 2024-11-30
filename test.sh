#!/bin/bash

# terminate at first error
set -e
# verbose
set -v

function check {
    if diff --brief input.txt temp.txt; then
        rm temp*
        echo "All good"
    fi
}

function ratio {
    compressed=$(wc -c < "temp.txt.huf");
    printf "%.3f%% \n" $(echo "$compressed / $filesize * 100" | bc -l)
}

filesize=$(wc -c < "input.txt")

# Standard compression
./compressor -c input.txt
mv input.txt.huf temp.txt.huf
./compressor -x temp.txt.huf

ratio
check

# Mode 2 = compressed header
./compressor -c input.txt -2
mv input.txt.huf temp.txt.huf
./compressor -x temp.txt.huf

ratio
check

# Mode 1 = std, quiet
./compressor -q -c input.txt
mv input.txt.huf temp.txt.huf
./compressor -x temp.txt.huf -q

ratio
check

# Mode 2 = compressed header, quiet
./compressor -q -c input.txt -2 
mv input.txt.huf temp.txt.huf
./compressor -x temp.txt.huf -q

ratio
check

# help
./compressor -h

