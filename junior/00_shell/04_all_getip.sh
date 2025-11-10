#!/bin/bash

# 检查 ifconfig 命令是否存在
if command -v ifconfig &> /dev/null; then
	    # 使用 ifconfig 获取 IP 地址
	        ip_address=$(ifconfig | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}')
	elif command -v ip &> /dev/null; then
		    # 使用 ip 命令获取 IP 地址
		        ip_address=$(ip addr show | grep 'inet ' | grep -v '127.0.0.1' | awk '{print $2}' | cut -d'/' -f1)
		else
			    echo "Error: Neither ifconfig nor ip command is available."
			        exit 1
fi

# 输出 IP 地址
echo "Your IP address is: $ip_address"
