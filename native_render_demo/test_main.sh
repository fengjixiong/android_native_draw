#!/bin/sh

TEST_ID=$1

# 如果没有参数，则打印说明
if [ -z $TEST_ID  ];then
    echo "usage: 使用示例"
    echo ""
    echo "$0 101 -- (Qcom RAW) show and dump RGBA8888"
    echo "$0 102 -- (Qcom RAW) show and dump RGB888"
    echo "$0 103 -- (Qcom RAW) show and dump RGBA1010102"
    echo "$0 104 -- (Qcom RAW) show and dump YUV420_NV12"
    echo "$0 105 -- (Qcom RAW) show and dump YUV420_P010"
    echo ""
    echo "$0 111 -- (MTK  RAW) show and dump RGBA8888"
    echo "$0 112 -- (MTK  RAW) show and dump RGB888"
    echo "$0 113 -- (MTK  RAW) show and dump RGBA1010102"
    echo "$0 114 -- (MTK  RAW) show and dump YUV420_NV12"
    echo "$0 115 -- (MTK  RAW) show and dump YUV420_P010"
    echo ""
    # echo "$0 201 -- (UBWC GPU) show and dump RGBA8888"
    # echo "$0 203 -- (UBWC GPU) show and dump RGBA1010102"
    # echo "$0 204 -- (UBWC GPU) show and dump YUV420_NV12"
    # echo "$0 205 -- (UBWC GPU) show and dump YUV420_P010"
    # echo ""
    # echo "$0 211 -- (AFBC GPU) show and dump RGBA8888"
    # echo "$0 213 -- (AFBC GPU) show and dump RGBA1010102"
    echo ""
    echo "$0 301 -- (UBWC GPU) load and show RGBA8888"
    # echo "$0 303 -- (UBWC GPU) load and show RGBA1010102"
    echo "$0 304 -- (UBWC GPU) load and show YUV420_NV12"
    # echo "$0 305 -- (UBWC GPU) load and show YUV420_P010"
    echo ""
    echo "$0 311 -- (AFBC GPU) load and show RGBA8888"
    echo "$0 321 -- (AFBC HWC) load and show RGBA8888"
    # echo "$0 312 -- (AFBC GPU) load and show RGB888"
    # echo "$0 313 -- (AFBC GPU) load and show RGBA1010102"
    echo ""

    exit 0
fi

echo "Test Case ${TEST_ID}"
function test_process()
{
    ID=$1
    OD=$2
    IW=$3
    IH=$4
    IE=$5
    IF=$6
    IU=$7
    CA=$8
    MF=$9
    UG=${10}
    PT=${11}

    # 这10个参数是必须指定的，涉及到芯片规格、图像尺寸、输入和输出
    LD_LIBRARY_PATH=. ./native_render_demo --image_in_dir ${ID} --image_out_dir ${OD} \
            --image_width ${IW} --image_height ${IH} --image_ext ${IE} \
            --image_format ${IF} --image_usage ${IU} --color_rgba  ${CA} \
            --max_frames ${MF} --use_gpu ${UG} --platform_type ${PT} --use_dmabuf_tool 1

    RET_CODE=$?
    return $RET_CODE
}

function expect_equal()
{
    EXPECT=$1
    REAL=$2

    if [ $EXPECT != $REAL ]; then
        echo -e "\033[31m TEST FAIL \033[0m"
        exit 1
    else
        echo -e "\033[32m TEST PASS \033[0m"
    fi
}

function get_platform()
{
    product=`getprop ro.product.device`
    if [ $product == "PD2564" ]; then
        echo "1"
    else
        echo "0"
    fi
}

######################### raw 测试 #########################
if [ $TEST_ID == "101" -o $TEST_ID == "111" -o $TEST_ID == "mtk" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) show and dump RGBA8888"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=rgba8888.raw
    IMG_FMT=1
    IMG_USG=0
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "102" -o $TEST_ID == "112" -o $TEST_ID == "mtk" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) show and dump RGB888"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=rgb888.raw
    IMG_FMT=3
    IMG_USG=0
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "103" $TEST_ID == "113" -o $TEST_ID == "mtk" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) show and dump RGBA1010102"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=rgba1010102.raw
    IMG_FMT=43
    IMG_USG=0
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "104" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) show and dump YUV420_NV12 in QCOM"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=yuv420_nv12.raw
    IMG_FMT=265 #0x109
    IMG_USG=0
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "114" -o $TEST_ID == "mtk" ];then
    echo "(   RAW  ) show and dump YUV420_NV12 in MTK"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=yuv420_nv12.raw
    IMG_FMT=4096 #0x1000
    IMG_USG=0x330
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "105" -o $TEST_ID == "115" -o $TEST_ID == "mtk" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) show and dump YUV420_P010"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=yuv420_p010.raw
    IMG_FMT=54
    IMG_USG=0
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "301" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) load and show UBWC RGBA8888"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=rgba8888.ubwc.raw
    IMG_FMT=1
    IMG_USG=0x10000300
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "304" -o $TEST_ID == "qcom" ];then
    echo "(   RAW  ) load and show UBWC YUV420 NV12"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=1920
    IMG_HEIGHT=1080
    IMG_EXT=yuv420_nv12.ubwc.raw
    IMG_FMT=265 # PD2366用2141391878
    IMG_USG=0x10000300
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi


if [ $TEST_ID == "311" -o $TEST_ID == "mtk" ];then
    echo "(   RAW  ) load and show AFBC RGBA8888"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=256
    IMG_HEIGHT=256
    IMG_EXT=rgba8888.afbc.raw
    IMG_FMT=1
    IMG_USG=0x300
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi

if [ $TEST_ID == "321" -o $TEST_ID == "mtk" ];then
    echo "(   RAW  ) load and show AFBC RGBA8888"

    IN_DIR=./images
    OUT_DIR=./dump
    IMG_WIDTH=1080
    IMG_HEIGHT=2376
    IMG_EXT=hwc.afbc.raw
    IMG_FMT=1
    IMG_USG=0xB00
    COLOR=0xffff0000
    MAX_FRAMES=1
    USE_GPU=0
    PLATFORM=`get_platform`

    test_process $IN_DIR $OUT_DIR $IMG_WIDTH $IMG_HEIGHT $IMG_EXT \
                 $IMG_FMT $IMG_USG $COLOR $MAX_FRAMES $USE_GPU $PLATFORM

    expect_equal "0" $?
fi
