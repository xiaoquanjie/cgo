#!/bin/bash

./tool/toutf8/toutf8.exe --file ./cgo --ext ".h|.cpp|.c|.hpp"
./tool/toutf8/toutf8.exe --file ./test --ext ".h|.cpp|.c|.hpp"
./tool/toutf8/toutf8.exe --file ./tutorial --ext ".h|.cpp|.c|.hpp"