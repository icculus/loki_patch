#!/bin/sh
#
# Run the patch process with appropriate environment

# Function to find the real directory a program resides in.
# Feb. 17, 2000 - Sam Lantinga, Loki Entertainment Software
FindPath()
{
    fullpath="`echo $1 | grep /`"
    if [ "$fullpath" = "" ]; then
        oIFS="$IFS"
        IFS=:
        for path in $PATH
        do if [ -x "$path/$1" ]; then
               if [ "$path" = "" ]; then
                   path="."
               fi
               fullpath="$path/$1"
               break
           fi
        done
        IFS="$oIFS"
    fi
    if [ "$fullpath" = "" ]; then
        fullpath="$1"
    fi
    # Is the awk/ls magic portable?
    if [ -L "$fullpath" ]; then
        fullpath="`ls -l "$fullpath" | awk '{print $11}'`"
    fi
    dirname $fullpath
}

# Get to the proper run directory
cd `FindPath "$0"`

# Return the appropriate architecture string
DetectARCH()
{
	status=1
	case `uname -m` in
		i?86)  echo "x86"
			status=0;;
		*)     echo "`uname -m`"
			status=0;;
	esac
	return $status
}

# See if the user wants to apply this patch
if [ "$1" = "--noreadme" ]; then
    shift;
else
    more README.txt
    echo ""
    echo -n "Do you want to apply this patch? [Y/n]: "
    read answer
    case $answer in
        [Nn]*)  echo "Aborting patch!"
                exit 0
                ;;
    esac
fi
 
# Add our binary path to the execution path
bin="bin/`uname -s`/`DetectARCH`"
PATH="$bin:$PATH"
export PATH

# Verify that the loki_patch version is okay
if loki_patch patch.dat --verify; then
    :
else
    exit 1
fi

# Pre-patch script?
if [ -f pre-patch.sh ]; then
    if sh pre-patch.sh; then
        :
    else
        exit 2
    fi
fi
# Run the patch program
if loki_patch patch.dat $*; then
    :
else
    exit 3
fi
# Post-patch script?
if [ -f post-patch.sh ]; then
    if sh post-patch.sh; then
        :
    else
        exit 4
    fi
fi
exit 0
