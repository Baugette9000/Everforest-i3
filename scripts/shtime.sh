
#!/bin/bash
hour=`date '+%H'`
hour=$((10#$hour + 1))

if [[ ${hour} -ge 5 ]] && [[ ${hour} -lt 12 ]]; then
    echo "Good Morning! 
    "
elif [[ ${hour} -lt 17 ]]; then
    echo "Good Afternoon"
elif [[ ${hour} -lt 21 ]]; then
    echo "Good Evening"
else
    echo "Go to sleep!"
fi
