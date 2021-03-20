#!/bin/sh
path=$(cd "$(dirname "$0")"; pwd)
cd $path

echo "kill skynet..."
killall skynet
#killall main

echo "clear log..."
rm -rf logs/*

screen -S dev -X quit
sleep 1

echo "All servers have stopped!"
