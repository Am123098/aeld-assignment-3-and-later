#!/bin/sh

if [[ $# -lt 2 ]]; then
    echo "ERROR: Invalide number of arguments!! (expected: 2 found: $#)"
    echo "Arg(1): File directory path"
    echo "Arg(2): String to be searched in the specified directory path"
    exit 1
fi

filesdir=$1
searchstr=$2

if [[ -d $filesdir ]]; then
    X=$(find $filesdir -type f | wc -l)
    Y=$(grep $searchstr -r $filesdir | wc -l)
    echo "The number of files are $X and the number of matching lines are $Y"
else
    echo "'$filesdir' does not exist or is not a directory!!"
    exit 1
fi
