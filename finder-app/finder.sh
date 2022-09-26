#!/bin/bash

# Expected args: $1 - files path, $2 - search string text 
if [ ! $# -eq 2 ]
then
	echo "Usage: path, search text";
	exit 1;
fi

# Make a copy for convience
filesdir=$1
searchstr=$2

# does dir exist?
if [ ! -d ${filesdir} ]
then
	echo "Directory does not exist";
	exit 1;
fi

#  grep for the string in the file, -c count, -r recursive
matchinglines=$(grep -r "${searchstr}" "${filesdir}" | wc -l)

# another search for matching files then pipe into wc to get a line count.
filesfound=$(grep -r -l "${searchstr}" "${filesdir}" | wc -l)

echo "The number of files are ${filesfound} and the number of matching lines are ${matchinglines}"

exit 0;
