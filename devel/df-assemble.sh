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

# read df version from CMakeLists.txt
export version=$(grep "set(DF_VERSION" CMakeLists.txt | sed -e 's/set(DF_VERSION "\(.*\)\")/\1/')

if [ ! -f "build/CMakeCache.txt" ]
then
    echo "Build dfhack into the build/ directory before running this script."
fi

which tar
if [ $? -ne 0 ]
then
    echo "tar (the program) not found."
    exit 3
fi

which bzip2
if [ $? -ne 0 ]
then
    echo "bzip2 (the program) not found."
    exit 3
fi

echo "df version $version"

plexdir="plugins/dfplex"

cd "$plexdir/"

bzdest="df.tar.bz2"
tardest="df.tar"

if [ ! -f $tardest ]
then
    if [ ! -f $bzdest ]
    then
        echo "downloading dwarf fortress v$version from bay12games.com..."
        sleep 1
        minor=$(echo "$version" | cut -d. -f2)
        patch=$(echo "$version" | cut -d. -f3)
        url="http://www.bay12games.com/dwarves/df_${minor}_${patch}_linux.tar.bz2"
        echo Downloading from $url
        wget "$url" -O "$bzdest"
    fi
    
    if [ -f "$bzdest" ]
    then
        echo "decompressing..."
        bzip2 -d $bzdest
        if [ -f "df_*.tar" ]
        then
            mv "df_*.tar" $tardest
        fi
    else
        echo "Failed to download."
        exit 2
    fi
fi

if [ ! -f "$tardest" ]
then
    echo "Failed to download and decompress dwarf fortress. ($tardest not found.)"
    exit 2
fi

dfdir="df"

# remove previous df version
if [ -d $dfdir ]
then
    echo "removing previous $dfdir/"
    if [ -L $dfdir/data/save ]
    then
        echo "removing symbollic link to save directory."
        unlink $dfdir/data/save
    fi
    rm -r $dfdir
fi
mkdir $dfdir

echo "Extracting df.tar into $dfdir/"
# extract tar
tar xf "$tardest" --strip-components=1 -C $dfdir

if [ $? -ne 0 ]
then
    echo "Failed to extract dwarf fortress tar to $dfdir/"
    exit 2
fi

cd ../..
if [ ! -d "build" ]
then
    echo "build/ directory not found in $(pwd)"
    exit 4
fi

# install dfhack into df
echo "installing dfhack into $dfdir ..."
echo ""
cd build/
# ideally, this only forces a rebuild if the prefix was not set correctly.
makeprogram=make

grep -q "^CMAKE_MAKE_PROGRAM.*ninja" CMakeCache.txt
if [ $? -eq 0 ]
then
    makeprogram=ninja
fi
cmake .. -DCMAKE_INSTALL_PREFIX=../$plexdir/$dfdir && $makeprogram install -j8
if [ $? -ne 0 ]
then
    echo "installation failed."
    exit 4
fi
cd ..

if [ ! -d $plexdir ]
then
    echo "Failed to find $plexdir"
    exit 4
fi

cd $plexdir

# link in save data
echo "softlinking local save data (for persistence)"
if [ ! -d save ]
then
    mkdir save
fi
if [ -d $dfdir/data/save ]
then
    rm -r $dfdir/data/save
fi
cd $dfdir/data
ln -s ../../save ./save
cd -

# edit init.txt
echo "editing df configuration."
sed -i 's/G_FPS_CAP:[0-9]*/G_FPS_CAP:8000/g' $dfdir/data/init/init.txt
sed -i 's/MACRO_MS:[0-9]*/MACRO_MS:0/g' $dfdir/data/init/init.txt
# disable sound for development convenience
sed -i 's/SOUND:\(YES\|NO\)/SOUND:NO/g' $dfdir/data/init/init.txt
# autosave
sed -i 's/AUTOSAVE:NONE/AUTOSAVE:SEASONAL/g' $dfdir/data/init/init.txt
# autosave
sed -i 's/AUTOSAVE:NONE/AUTOSAVE:SEASONAL/g' $dfdir/data/init/init.txt
# engravings
sed -i 's/ENGRAVINGS_START_OBSCURED:NO/ENGRAVINGS_START_OBSCURED:YES/g' $dfdir/data/init/init.txt

# announcement edits for more harmonious multiplayer
sed -i 's/BIRTH_CITIZEN:A_D:D_D:P:R/BIRTH_CITIZEN:A_D:D_D/g' $dfdir/data/init/announcements.txt
sed -i 's/MOOD_BUILDING_CLAIMED:A_D:D_D:P:R/MOOD_BUILDING_CLAIMED:A_D:D_D/g' $dfdir/data/init/announcements.txt
sed -i 's/ARTIFACT_BEGUN:A_D:D_D:P:R/ARTIFACT_BEGUN:A_D:D_D/g' $dfdir/data/init/announcements.txt

dfplexinit="devel/dfhack.init-web"
if [ -f $dfplexinit ]
then
    echo "copying in dfplex's dfhack.init"
    cp $dfplexinit "$dfdir/dfhack.init"
fi

chmod a+x devel/df-launch.sh

echo
echo "setup complete. dfhack is installed alongside df in $dfdir/"
echo "Now run the following command:"
echo ""
echo "  devel/df-launch.sh"
echo ""
echo "Then connect to localhost:1233/dfplex.html in your web browser."
