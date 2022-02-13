#!/bin/sh

# Script to build SDLmain.o startup code

gcc `../bin/sdl-config --cflags` -c SDLmain.m
