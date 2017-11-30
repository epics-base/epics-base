#!/bin/sh
#
# Checkout submodules on their appropriate branches
#

git submodule foreach '\
    git checkout `git config -f $toplevel/.gitmodules submodule.$name.branch` && \
    git pull '
