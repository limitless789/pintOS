#!/bin/bash

dirname=${PWD}
echo $dirname
sed -i "257 c\        my \$name = find_file ('$dirname/src/threads/build/kernel.bin');" ./src/utils/pintos
sed -i "362 c\    \$name = find_file ('$dirname/src/threads/build/loader.bin') if !defined \$name;" ./src/utils/Pintos.pm

