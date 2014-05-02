echo "*****"
echo "Setting up the environment and running the configuration script"
echo "Note that you must have a working gumstix buildroot compiled at $HOME/gumstix/gumstix-builroot"
echo "and have installed the peiskernel into this buildroot"
echo "*****"
echo ""
BUILD_ROOT=${HOME}/gumstix/gumstix-buildroot
STAGING=${BUILD_ROOT}/build_arm_nofpu/staging_dir
export PATH=$PATH:${STAGING}/bin
export LDFLAGS="-L${BUILD_ROOT}/build_arm_nofpu/root/lib"
export CFLAGS="-I${BUILD_ROOT}/build_arm_nofpu/root/include"
CC=arm-linux-gcc ./configure --host=arm-linux --build=i686-linux --prefix=/home/moz/gumstix/gumstix-buildroot/build_arm_nofpu/root             
