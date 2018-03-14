#!/bin/bash

if [ "$1" == "-h" ] ; then
    echo "Download and install the rapidjson dependency from github. Installs to /usr/include by default. Usage: `basename $0` [-h] [install-dir] "
    exit 0
fi

if [ -z "$1" ] ; then
    INSTALL_DIR=/usr/include
else
    INSTALL_DIR=$1
fi

wget https://github.com/Tencent/rapidjson/archive/v1.1.0.zip

if [[ "$?" != 0 ]]; then 
    echo "Failed to download rapidjson from github!"
    exit -1
fi


unzip v1.1.0.zip 'rapidjson-1.1.0/include/rapidjson/*' -d $INSTALL_DIR/rapidjson

if [[ "$?" != 0 ]]; then 
    echo "Failed to install rapidjson headers to $INSTALL_DIR"

else 
# unzip preserves directory structure, so we'll just move them to where we want them
mv $INSTALL_DIR/rapidjson/rapidjson-1.1.0/include/rapidjson/* $INSTALL_DIR/rapidjson/
rm -r $INSTALL_DIR/rapidjson/rapidjson-1.1.0 

fi

rm v1.1.0.zip
