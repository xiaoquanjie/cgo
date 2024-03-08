#!/bin/sh

this_dir=$(dirname ${0})
cd ${this_dir}

../excel2cpp/excel2cpp --excel ${this_dir}/excel --oneproto=true

cp ../excel2cpp/template/sheet_reader.h ./cpp/

echo "10s后自动关闭"
sleep 10