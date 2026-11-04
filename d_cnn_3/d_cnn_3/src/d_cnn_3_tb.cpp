#include <stdlib.h>
#include <iostream>
#include "d_cnn_3.h"
#include "input_data_v.h"  // 包含测试输入数据

hls::stream<int8_t> input_stream;

int main() {
	int result = 0;
	const int expected_result = 3;

	for (int j = 0; j < 864; j++) {
		input_stream.write(input_data_v[j]);
	}

	d_cnn_3(input_stream, &result);
	std::cout << "classification result: " << result << std::endl;

	if (result != expected_result) {
		std::cerr << "expected " << expected_result
		          << ", got " << result << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}




