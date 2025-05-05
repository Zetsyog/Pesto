#!/usr/bin/env bash

# This script formats all the files in the current directory and its subdirectories
(cd include/ && git ls-files '*.[ch]pp' '*.[ch]' | xargs clang-format -i)
(cd lib/ && git ls-files '*.[ch]pp' '*.[ch]' | xargs clang-format -i)
(cd tool/ && git ls-files '*.[ch]pp' '*.[ch]' | xargs clang-format -i)
