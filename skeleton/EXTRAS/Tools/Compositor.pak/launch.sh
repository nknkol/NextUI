#!/bin/sh

cd $(dirname "$0")
./compositor.elf &> ./log.txt
