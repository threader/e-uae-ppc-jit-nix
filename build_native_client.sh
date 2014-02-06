#!/bin/bash

###########################################################################
# Set up. Adjust these as necessary.
###########################################################################

# Path to the NaCl SDK to use. Should be pepper_31 or later.
export NACL_SDK_ROOT=${HOME}/nacl_sdk/pepper_31

# Check out naclports and point this to the folder with 'src/'.
export NACL_PORTS_ROOT=${HOME}/work/naclports

# Staging dir on local web server.
export WEB_SERVER_DESTINATION_DIR=${HOME}/work/puae/staging

# URL to access the staging dir via the web server.
export WEB_SERVER="http://localhost:8080"

# The OS you're building this on.
# Note: Building on Windows has not been tried.
export OS=mac # Should be mac, linux, or win.

# Set the Chrome/Chromium to use.
export CHROME_EXE='/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary'

# Any extra CPPFLAGS.
export NACL_CPPFLAGS="-DDEBUG"
export NACL_CFLAGS=""

# Enable build key (yes or no/empty).
# This generates a random key that is used to apply naive XOR encryption
# on all UAE data files (ROMs, ADFs, etc.) fetched from the web server.
export ENABLE_BUILD_KEY=yes
export DATA_FILES_TO_DEPLOY_DIR=${HOME}/work/puae/data_files_to_deploy

# Choose SDL or Pepper for graphics, sound, and input.
# TODO(cstefansen): Fix NaCl SDL build.
#export UAE_CONFIGURE_FLAGS_NACL="--enable-drvsnd \
#    --with-sdl --with-sdl-gfx --with-sdl-gl --with-sdl-sound"
export UAE_CONFIGURE_FLAGS_NACL="--enable-drvsnd --with-pepper --enable-serial-port"
# (Serial port emulation is needed for debugging AROS ROMs.)


###########################################################################
# You shouldn't need to change anything below this point.
###########################################################################

export UAE_ROOT=`cd \`dirname $0\` && pwd`


############################################################################
# Do a clean build for PNaCl.
############################################################################

if [ "$1" = "pnacl" ]; then
    export NACL_TOOLCHAIN_ROOT=${NACL_SDK_ROOT}/toolchain/${OS}_pnacl
    export CC=${NACL_TOOLCHAIN_ROOT}/bin/pnacl-clang++
    export CXX=$CC
    export RANLIB=${NACL_TOOLCHAIN_ROOT}/bin/pnacl-ranlib
    export AR=${NACL_TOOLCHAIN_ROOT}/bin/pnacl-ar
    export NACL_INCLUDE=${NACL_SDK_ROOT}/include
    export NACL_LIB=${NACL_SDK_ROOT}/lib/pnacl/Release

    export CPPFLAGS="-I${NACL_INCLUDE} ${NACL_CPPFLAGS}"
    export CFLAGS="${NACL_CFLAGS} -O2 -g -Wno-unused-parameter -Wno-missing-field-initializers"
    export CXXFLAGS="-O2 -g -Wno-unused-parameter -Wno-missing-field-initializers"
    export LDFLAGS="-L${NACL_LIB}"

    # Build zlib.
    cd ${NACL_PORTS_ROOT}/src/libraries/zlib
    if [ "$OS" = "mac" ]; then
        # TODO(cstefansen): Upstream this fix, so it builds on Mac without
        # patching.
        patch -p0 -N <<EOF
index fe99ed0..2a45383 100755
--- nacl-zlib.sh
+++ nacl-zlib.sh
@@ -91,7 +91,7 @@ PackageInstall() {
   ConfigureStep
   BuildStep
   ValidateStep
-  TestStep
+#  TestStep
   DefaultInstallStep
   if [ "${NACL_GLIBC}" = "1" ]; then
     ConfigureStep shared
EOF
    fi  # "$OS" = "mac"
    NACL_ARCH=pnacl NACL_GLIBC=0 ./nacl-zlib.sh

    # Generate build key.
    cd ${UAE_ROOT}/src
    if [ "$ENABLE_BUILD_KEY" = "yes" ]; then
        time dd if=/dev/urandom of=build.key bs=1 count=4096
        xxd -i build.key build.key.c
        export CPPFLAGS="${CPPFLAGS} -DENABLE_BUILD_KEY"
    fi  # ENABLE_BUILD_KEY

    # Build PUAE.
    cd ${UAE_ROOT}
    make clean
    ./bootstrap.sh
    ./configure --host=le32-unknown-nacl ${UAE_CONFIGURE_FLAGS_NACL}
    make
    exit $?
fi  # "pnacl"


###########################################################################
# Incremental
###########################################################################

if [ "$1" = "incremental" ]; then
    make
    exit $?
fi  # incremental


############################################################################
# Build for desktop.
# This is left in for convenience to aid debugging/profiling.
############################################################################

if [ "$1" = "desktop" ]; then
    CPPFLAGS=""
    CFLAGS="-m32 -g"
    CXXFLAGS="-m32 -g"
    LDFLAGS=""
    cd ${UAE_ROOT}
    make clean
    ./bootstrap.sh
    ./configure --disable-ui --disable-jit \
        --with-sdl --with-sdl-gfx --without-sdl-gl --with-sdl-sound \
        --disable-autoconfig
    if [ "$?" -ne "0" ]; then
        echo "./configure failed for desktop UAE."
        exit 1
    fi
    make
    if [ "$?" -ne "0" ]; then
        echo "make failed for desktop UAE."
        exit 1
    fi
    exit 0
fi  # "desktop"


###########################################################################
# Running/debugging
###########################################################################

if [ "$1" = "run" ] || [ "$1" = "debug" ]; then
    # Encrypt if requested and then stage data files.
    cd ${DATA_FILES_TO_DEPLOY_DIR}
    if [ "$ENABLE_BUILD_KEY" = "yes" ]; then
        for f in *
        do
            ${UAE_ROOT}/src/xor_encryption.py $f ${UAE_ROOT}/src/build.key
            mv $f.encrypted ${WEB_SERVER_DESTINATION_DIR}/$f
        done
    else
        cp * ${WEB_SERVER_DESTINATION_DIR}/
    fi  # ENABLE_BUILD_KEY

    # Copy stuff to web server.
    cd ${UAE_ROOT}
    echo "Copying files to web server directory."
    mkdir -p ${WEB_SERVER_DESTINATION_DIR}/img
    cp src/gui-html/img/amiga500_128x128.png \
        src/gui-html/img/amiga_pointer.png \
        src/gui-html/img/first_demos.png \
        src/gui-html/img/read_me.png \
        src/gui-html/img/loading.gif \
    	${WEB_SERVER_DESTINATION_DIR}/img/
    cp src/gui-html/uae.html src/gui-html/uae_faq.html src/gui-html/uae.css \
        src/gui-html/uae.js src/gui-html/uae.nmf src/gui-html/default.uaerc \
        src/gui-html/img/favicon.ico \
        ${WEB_SERVER_DESTINATION_DIR}/

    # TODO(cstefansen): Put the pnacl-finalize step in a make rule instead.
    ${NACL_SDK_ROOT}/toolchain/${OS}_pnacl/bin/pnacl-finalize src/uae \
        -o ${WEB_SERVER_DESTINATION_DIR}/uae.pexe

    # Make sure the files are readable.
    chmod -R 0755 ${WEB_SERVER_DESTINATION_DIR}

    # Have Chrome navigate to the app.
    CHROME_APP_LOCATION="$WEB_SERVER/uae.html"

    # Chrome flags.
    # Add the flag --disable-gpu to simulate system without OpenGLES support.
    CHROME_FLAGS='--user-data-dir=../chrome-profile \
        --no-first-run'
    # Some other convenient command-line flags to use:
    #   --show-fps-counter --incognito

    # Clean the user profile.  We don't want cached files.
    rm -fR ../chrome-profile

    if [ "$1" = "debug" ]; then
        CHROME_FLAGS+=' --enable-nacl-debug'
        echo "*** Starting Chrome in NaCl debug mode. ***"
        echo "Start nacl-gdb and connect with 'target remote localhost 4010'."
    else
        ${NACL_SDK_ROOT}/toolchain/${OS}_pnacl/bin/pnacl-strip \
            ${WEB_SERVER_DESTINATION_DIR}/uae.pexe
    fi
    echo "${CHROME_EXE}" ${CHROME_FLAGS} ${CHROME_APP_LOCATION}
    "${CHROME_EXE}" ${CHROME_FLAGS} ${CHROME_APP_LOCATION}
    exit 0
fi  # "run" || "debug"


###########################################################################
# Didn't find anything useful to do; print usage message.
###########################################################################

cat <<EOF
Usage: $0 (pnacl | desktop | incremental | run | debug)

How to use this script:

1. Adjust the variables in the beginning of the script.

2. To make a clean PNaCl build of UAE, execute
       ./build_native_client.sh pnacl

3. Start the web server, e.g., by executing
       python -m SimpleHTTPServer 8080
   from the web server destination dir.

4. To stage the files and run Chrome do
       ./build_native_client.sh run

5. Do incremental builds with
      ./build_native_client.sh incremental

6. To run and make Chrome/PNaCl wait until a NaCl debugger is attached use
       ./build_native_client.sh run
EOF
