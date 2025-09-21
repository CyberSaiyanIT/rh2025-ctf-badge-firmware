#!/bin/sh

while true; do
    ./ssh
    echo "SSH server crashed. Restarting in 1s..."
    sleep 1
done &
 
while true; do
    ./ntp
    echo "NTP server crashed. Restarting in 1s..."
    sleep 1
done &

wait