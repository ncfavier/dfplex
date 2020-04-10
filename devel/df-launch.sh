# go to the correct directory
if [ ! -d "plugins/" ]
then
    if [ ! -d "devel" ]
    then
        cd ..
    fi
    if [ ! -d "devel" ]
    then
        echo "error: must be run from dfhack or dfhack/plugins/dfplex directory."
        exit 3
    fi
    cd ../..
    if [ ! -d "plugins/" ]
    then
        echo "error: must be run from dfhack or dfhack/plugins/dfplex directory."
        exit 3
    fi
fi
cd plugins/dfplex 
#!/bin/bash
set -x
dfdir="df"
if [ ! -d $dfdir ]
then
    echo "$dfdir/ does not exist. Did you run df-assemble.sh?"
    exit 1
fi

export DFPLEX_STATICPORT="1233"
export DFPLEX_STATICDIR="../static"

cd $dfdir
./dfhack "$@"
cd -
