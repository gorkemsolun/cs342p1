#!/bin/bash
#NUM_OF_TERM
NUM_OF_TERM=3
for((i=1; i<=NUM_OF_TERM;i++)); do
    gnome-terminal -- bash -c "./client server -b commands.txt -s 1024"
done
