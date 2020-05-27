if [ "$#" -ne 1 ]
then
    echo "usage: $0 <buildNumber>"
    exit
fi

buildnumber="$1"
url="https://buildmaster.lubar.me/artifacts/download?applicationId=48&releaseNumber=dfhack-0.47.04-r1&buildNumber=${1}&artifactName="
dst="release"
pre="dfplex-v0.2-"
assets="${pre}assets"
root=`pwd`

if [ ! -d "static" ]
then
    echo "static/ folder not found -- is this the right directory?"
    exit 1
fi

if [ -d "$dst" ]
then
    echo "Removing previous..."
    rm -r "$dst"
fi

############ create assets template #############
echo "Creating asset template..."
mkdir "$dst" && cd "$dst" && mkdir "$assets" && cd "$assets"

# dist/shared
for filename in $root/dist/shared/*
do
    if [ -d "$filename" ]
    then
        cp -r "$filename" .
    elif [ -f "$filename" ]
    then
        cp "$filename" .
    fi
done

# static
cp -r "$root/static" "./hack/www"

# dfhack init
cp "$root/devel/dfhack.init" .

# license
cp "$root/LICENSE.rst" .

############ create target packages #############
for platform in "Windows32" "Windows64" "Linux32" "Linux64"
do
    cd "$root/$dst"
    echo "assembling ${platform}"
    target="${pre}${platform}"
    cp -r "${assets}" "${target}"
    cd "${target}/hack"
    if [ ! -d "plugins" ] 
    then
        mkdir "plugins"
    fi
    cd "plugins"
    
    # Download zip from build server
    wget -O "artifact.zip" "${url}${platform}"
    if [ $? -ne 0 ]
    then
        echo "Failed to download artifact for $platform".
        exit 2
    fi
    
    # extract zip
    7z e "artifact.zip"
    if [ $? -ne 0 ]
    then
        echo "Failed to extract zip for $platform".
        exit 3
    fi
    
    # remove zip to avoid name clash with next step.
    rm "artifact.zip"
    
    # extract zip
    zip=`find . -name "*.zip"`
    if [ -f "$zip" ]
    then
        7z e "$zip"
        if [ $? -ne 0 ]
        then
            echo "Failed to extract archive for $platform".
            exit 5
        fi
    fi
    
    # extract tar.gz
    targz=`find . -name "*.tar.gz"`
    if [ -f "$targz" ]
    then
        tar -xf "$targz"
        if [ $? -ne 0 ]
        then
            echo "Failed to extract archive for $platform".
            exit 5
        fi
    fi
    
    if find . | grep -q "dfplex.plug.*"
    then
        echo "Extracted binary for $platform successfully."
    else
        echo "failed to extract binary from archive for $platform. Contents are:"
        find .
        exit 4
    fi
    
    # clean up.
    touch "a.zip"
    touch "a.tar.gz"
    rm *.zip
    rm *.tar.gz
    
    cd "$root/$dst"
    echo "Creating $target.zip ..."
    7z a "$target.zip" "$target"
    
    if [ $? -ne 0 ]
    then
        echo "Failed to create archive for $platform, $target.zip".
        exit 7
    fi
    
    echo ""
done