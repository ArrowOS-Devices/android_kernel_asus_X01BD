#!/bin/bash

#set -e

DATE_POSTFIX=$(date +"%Y%m%d")

## Copy this script inside the kernel directory
KERNEL_DIR=$PWD
KERNEL_TOOLCHAIN=$PWD/../aarch64-linux-android-4.9/bin/aarch64-linux-android-
CLANG_TOOLCHAIN=$PWD/../clang/bin/clang-9
KERNEL_DEFCONFIG=sanders_defconfig
DTBTOOL=$KERNEL_DIR/Dtbtool/
JOBS=16
ZIP_DIR=$KERNEL_DIR/zip/
FINAL_KERNEL_ZIP=MAYHEM-KERNEL~beast-mod-Release-$DATE_POSTFIX.zip
# Speed up build process
MAKE="./makeparallel"

BUILD_START=$(date +"%s")
blue='\033[0;34m'
cyan='\033[0;36m'
#red
R='\033[05;31m'
#purple
P='\e[0;35m'
yellow='\033[0;33m'
red='\033[0;31m'
nocol='\033[0m'

echo -e  "$P // Setting up Toolchain //"
export CROSS_COMPILE=$KERNEL_TOOLCHAIN
export ARCH=arm64
export SUBARCH=arm64

echo -e  "$R // Cleaning up //"
make clean && make mrproper && rm -rf out/

echo -e "$cyan // defconfig is set to $KERNEL_DEFCONFIG //"
echo -e "$blue***********************************************"
echo -e "$R          BUILDING MAYHEMKERNEL          "
echo -e "***********************************************$nocol"
make $KERNEL_DEFCONFIG O=out
make -j$JOBS CC=$CLANG_TOOLCHAIN CLANG_TRIPLE=aarch64-linux-android- O=out

echo -e "$blue***********************************************"
echo -e "$R          Generating DT image          "
echo -e "***********************************************$nocol"
$DTBTOOL/dtbToolCM -2 -o $KERNEL_DIR/out/arch/arm64/boot/dtb -s 2048 -p $KERNEL_DIR/out/scripts/dtc/ $KERNEL_DIR/out/arch/arm64/boot/dts/qcom/

echo -e "$R // Verify Image.gz & dtb //"
ls $KERNEL_DIR/out/arch/arm64/boot/Image.gz
ls $KERNEL_DIR/out/arch/arm64/boot/dtb

echo -e "$R // Verifying zip Directory //"
ls $ZIP_DIR
echo "// Removing leftovers //"
rm -rf $ZIP_DIR/dtb
rm -rf $ZIP_DIR/Image.gz
rm -rf $ZIP_DIR/$FINAL_KERNEL_ZIP

echo "**** Copying Image.gz ****"
cp $KERNEL_DIR/out/arch/arm64/boot/Image.gz $ZIP_DIR/
echo "**** Copying dtb ****"
cp $KERNEL_DIR/out/arch/arm64/boot/dtb $ZIP_DIR/

echo "**** Time to zip up! ****"
cd $ZIP_DIR/
zip -r9 $FINAL_KERNEL_ZIP * -x README $FINAL_KERNEL_ZIP
cp $KERNEL_DIR/zip/$FINAL_KERNEL_ZIP /home/ubuntu/$FINAL_KERNEL_ZIP

echo -e "$yellow // Build Successfull  //"
cd $KERNEL_DIR
rm -rf arch/arm64/boot/dtb
rm -rf $ZIP_DIR/$FINAL_KERNEL_ZIP
rm -rf zip/Image.gz
rm -rf zip/dtb
rm -rf $KERNEL_DIR/out/

BUILD_END=$(date +"%s")
DIFF=$(($BUILD_END - $BUILD_START))
echo -e "$yellow Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds.$nocol"
