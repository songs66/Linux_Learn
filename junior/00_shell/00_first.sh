#!/bin/bash

echo "hello world"

me=chinese
echo "i am a ${me}man"

for file in $(ls /home/songzihao/share/); do
	echo "$file"
done
