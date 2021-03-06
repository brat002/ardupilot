#git submodule init && git submodule update
export TOOLCHAIN

ROOT=`cd ../../../../..; pwd`

export PATH=/usr/local/bin:$PATH

echo $ROOT


( # AirBotF4 board
 cd $ROOT/ArduCopter
 make revomini-clean
 make revomini VERBOSE=1 BOARD=revomini_AirbotV2
) && (
 cd $ROOT/ArduPlane
 make revomini-clean
 make revomini VERBOSE=1 BOARD=revomini_AirbotV2
)

# at 4e017bf5b3da4f2a9ffc2e1cc0a37b94edac2bdc
