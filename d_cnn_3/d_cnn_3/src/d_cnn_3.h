
#ifndef MAIN_H
#define MAIN_H

#include <ap_int.h>        // 支持任意精度整数类型
#include <hls_stream.h>    // HLS流数据类型
#include <stdint.h>        // 标准整数类型


void d_cnn_3(hls::stream<int8_t> &input_stream,
		int *result
);


#endif
