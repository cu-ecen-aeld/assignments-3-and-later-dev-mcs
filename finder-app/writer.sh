#!/bin/sh

# Expected args: $1 - files path, $2 - search string text 
if [ ! $# -eq 2 ]
then
	echo "Usage: file path, text to write";
	exit 1;
fi

writefile="$1"
writestr="$2"

mkdir -p $(dirname "${writefile}") 
if [ ! $? -eq 0 ]
then
        echo "Failed to create file and write text"
        exit 1;
fi

echo "${writestr}" > "${writefile}"
if [ ! $? -eq 0 ]
then
	echo "Failed to create file and write text"
	exit 1;
fi

exit 0;
