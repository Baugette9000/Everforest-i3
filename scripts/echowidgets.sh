#!/bin/sh
tput clear 
echo '▒██▀░█░░▒█░█░░▒█░░░█░░▒█░█░█▀▄░▄▀▒▒██▀░▀█▀░▄▀▀'
echo '░█▄▄░▀▄▀▄▀░▀▄▀▄▀▒░░▀▄▀▄▀░█▒█▄▀░▀▄█░█▄▄░▒█▒▒▄██'

echo '
Menu (Opens Widgets Menu)
Stats (Stats Widgets)
Greeter (Greeter Widget) [Lockscreen]
Profile (Profile) [Welcome Message]
'
echo ''
echo '▒██▀░█░░▒█░█░░▒█░░░▄▀▀░▄▀▄░█▄▒▄█░█▄▒▄█▒▄▀▄░█▄░█░█▀▄░▄▀▀'
echo '░█▄▄░▀▄▀▄▀░▀▄▀▄▀▒░░▀▄▄░▀▄▀░█▒▀▒█░█▒▀▒█░█▀█░█▒▀█▒█▄▀▒▄██'
echo '
Menu (eww open-many leftbg profileleft calendar sysctl musicfore statslef)
Stats (eww open stats)
Greeter (eww open greeter)
Profile (eww open profile)
'

i3 floating enable &> /dev/null
