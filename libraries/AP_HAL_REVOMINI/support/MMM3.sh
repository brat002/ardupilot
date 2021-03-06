#git submodule init && git submodule update
export TOOLCHAIN

ROOT=`cd ../../..; pwd`

export PATH=/usr/local/bin:$PATH

echo $ROOT


 ( # Cl_Racing F4 board
 cd $ROOT/ArduCopter
 make revomini VERBOSE=1 BOARD=revo_cl_racing  && (

 cp $ROOT/ArduCopter/revo_cl_racing.bin $ROOT/Release/Copter
 cp $ROOT/ArduCopter/revo_cl_racing.hex $ROOT/Release/Copter
 cp $ROOT/ArduCopter/revo_cl_racing.dfu $ROOT/Release/Copter

 make revomini-clean

 )
) && (
 cd $ROOT/ArduPlane
 make revomini-clean
 make revomini VERBOSE=1 BOARD=revomini_Airbot && (

 cp $ROOT/ArduPlane/revo_cl_racing.bin $ROOT/Release/Plane
 cp $ROOT/ArduPlane/revo_cl_racing.hex $ROOT/Release/Plane
 cp $ROOT/ArduPlane/revo_cl_racing.dfu $ROOT/Release/Plane

 make revomini-clean

 )
) && ( # AirBotF4 board
 cd $ROOT/ArduCopter
# make revomini-clean
 make revomini VERBOSE=1 BOARD=revomini_AirbotV2  && (

 cp $ROOT/ArduCopter/revomini_AirbotV2.bin $ROOT/Release/Copter
 cp $ROOT/ArduCopter/revomini_AirbotV2.hex $ROOT/Release/Copter
 cp $ROOT/ArduCopter/revomini_AirbotV2.dfu $ROOT/Release/Copter


 )
) && (
 cd $ROOT/ArduPlane
 make revomini-clean
 make revomini VERBOSE=1 BOARD=revomini_AirbotV2 && (

 cp $ROOT/ArduPlane/revomini_AirbotV2.bin $ROOT/Release/Plane
 cp $ROOT/ArduPlane/revomini_AirbotV2.hex $ROOT/Release/Plane
 cp $ROOT/ArduPlane/revomini_AirbotV2.dfu $ROOT/Release/Plane
 )

) && (
 cd $ROOT

 zip -r latest.zip Release
 git add . -A
)











