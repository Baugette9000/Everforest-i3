#!/bin/bash

notify-send " Ram Usage ~" " $(free -h | awk ' /^Mem:/ {print $3 "/" $2}')"
