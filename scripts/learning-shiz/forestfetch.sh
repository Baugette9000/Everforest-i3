#!/bin/sh
echo  "os"  " ~ "  $(lsb_release -d 2>/dev/null | awk -F':' '/Description/ {printf $2}')
echo  "sh"  " ~ "  "zsh"
echo  "wm"  " ~ "  $(printf '%s\n' "$XDG_CURRENT_DESKTOP")
echo  "kr"  " ~ "  $(uname -r)
echo  "                                        "
echo  "  welcome to the forest, Breadchan .* "
echo  "                                        "
echo  "                                        "
