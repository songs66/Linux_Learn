#!/bin/bash

for i in {50..53}; do
	ping -c 2 101.126.129.$i &> /dev/null
	if [ $? -eq 0 ]; then
		echo "101.126.129.$i is up"
	else
		echo "101.126.129.$i is down"
	fi
done


