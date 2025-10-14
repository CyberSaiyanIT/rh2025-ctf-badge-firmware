#!/bin/bash
exec docker run --rm -it --cap-drop=ALL --security-opt no-new-privileges --tmpfs /tmp --tmpfs /run --security-opt apparmor=docker-default ubuntu

