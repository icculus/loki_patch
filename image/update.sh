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

# Get to the proper run directory
cd `FindPath "$0"`

# Add our binary path to the execution path
bin="bin/`uname -s`/`DetectARCH`"
PATH="$bin:$PATH"
export PATH

# Verify that the loki_patch version is okay
if loki_patch --verify patch.dat; then
    :
else
    exit 1
fi

# See if we should prompt the user for anything
prompt=yes
if [ "$1" = "--noprompt" ]; then
    prompt=no
    shift
fi

# Get some information about the product
product=`grep -i "Product:" patch.dat | sed 's/^[^:]*: //'`
component=`grep -i "Component:" patch.dat | sed 's/^[^:]*: //'`
version=`grep -i "Version:" patch.dat | sed 's/^[^:]*: //'`
description=`grep -i "Description:" patch.dat | sed 's/^[^:]*: //'`
installed=`loki_patch --info patch.dat "$1" |
           grep Installed: | sed 's/^[^:]*: //'`

# Be nice and verbose
if [ "$description" = "" ]; then
    if [ "$component" != "" ]; then
        description="$product_name: $component $version Update"
    else
        description="$product_name $version Update"
    fi
fi
echo "============================================================="
echo "Welcome to the $description"
echo "============================================================="

# See if the user wants to apply this patch
if tty -s && [ "$prompt" = "yes" ]; then
    # Show the user the README, if there is one available
    if [ -f README.txt ]; then
        echo ""
        echo -n "Would you like to read the README for this update?  [Y/n]: "
        read answer
        case $answer in
            [Nn]*)
                ;;
            *)
                more README.txt
                ;;
        esac
    fi
    # Ask if the user really wants to apply the update
    echo ""
    echo "============================================================="
    echo -n "Would you like to apply this update? [Y/n]: "
    read answer
    case $answer in
        [Nn]*)  echo "Update cancelled."
                exit 0
                ;;
    esac
    # See if we can find out where the product is installed
    if [ "$installed" = "" -o ! -d "$installed" ]; then
        for path in /opt /usr/local /usr/games /usr/local/games "$HOME"; do
            if [ -d "$path/$product" ]; then
                possible="$path/$product"
                break
            fi
        done
        echo ""
        while [ "$installed" = "" -o ! -d "$installed" ]; do
            echo -n "Please enter the installation path: [$possible]: "
            read line
            if [ "$line" != "" ]; then
                possible="$line"
            fi
            installed="$possible"
        done
    fi
fi
 
echo ""
echo "============================================================="
echo "Performing update:"

# Run the patch program
loki_patch patch.dat $installed
status=$?
if [ "$status" = "0" ]; then
    echo ""
    echo "Product updated successfully."
fi
exit $status
