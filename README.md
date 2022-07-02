# CSE4100-System-Programming
Sogang University, Jaejin Lee

## Project 1 : MyShell Project

Goal : Build a mini shell that can perform some jobs done by the real linux shell and understand how processes and signals work.

- phase 1 : basic linux shell commands such as ls, cd <directory>, cat, touch, ps.. implemented.
- phase 2 : pipeline commands utility added to the implementation of phase 1.
- phase 3 : fg, bg, jobs <pid> commands work. Can handle single commands in fg, bg. Failed to implement pipelined Ctrl + Z / fg / bg.

## Project 2 : Concurrent Stock Server Program

Goal : Build a simple concurrent stock server based on concurrent event-based server and thread-based server pool.

- task_1 : event-based server that can handle multiple requests from multiclient.c
- task_2 : thread-based server that can handle multiple requests from multiclient.c

## Project 3 : My own malloc project

Goal : Understand how a dynamic memory allocator works and implement it based on the idea of Segmentation List.

mm.c is the main program I implemented. Most of the macros and basic code lines are similar to the ones in the textbook (CS:APP). Built based on the idea of Seglist.
run the program with the command ./mdriver or ./mdriver -v. Has 90/100 score. (memory utilization : 50 + throughput : 40)
