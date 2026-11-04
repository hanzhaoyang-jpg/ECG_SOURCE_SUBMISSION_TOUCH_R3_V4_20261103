#include "d_cnn_3.h"
#include "input_data_v.h"
extern "C" __declspec(dllexport) int predict_batch(const int8_t *data, int count, int *out) {
    if (!data || !out || count < 0) return -1;
    for (int n = 0; n < count; ++n) {
        hls::stream<int8_t> stream;
        for (int i = 0; i < 864; ++i) stream.write(data[n * 864 + i]);
        d_cnn_3(stream, &out[n]);
        if (out[n] < 0 || out[n] > 3) return -2;
    }
    return 0;
}
extern "C" __declspec(dllexport) int known_vector() {
    int result = -1;
    predict_batch(input_data_v, 1, &result);
    return result;
}
