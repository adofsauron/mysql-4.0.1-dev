#!/bin/bash


libtoolize --automake --copy --debug --force

autoscan

aclocal

autoheader

automake


