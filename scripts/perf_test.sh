#! /bin/bash

exec=$1
shift

while getopts ":d:s:h" arg; do
  case $arg in
    d) # destination ip addr
      dest=${OPTARG}
      ;;
    s) # source ip addr
      src=${OPTARG}
      ;;
    h) # help
      echo '-d   destination ip address'
      echo '-s   source ip address'
      exit 0
      ;;
  esac
done

dir=`dirname $0`

${dir}/../release/bin/${exec} -p 20000 -d ${dest} -a 10000 -s ${src} -l 200 -t 5000000 -i 0 -f 1 -r 20000 -n 1 -w 10 -u 10000 -e 10 -x 1
