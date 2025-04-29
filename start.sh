#!/bin/bash
set -e

. $IDF_PATH/export.sh

idf.py fullclean

idf.py build

idf.py -p /dev/ttyUSB0 flash

idf.py -p /dev/ttyUSB0 monitor