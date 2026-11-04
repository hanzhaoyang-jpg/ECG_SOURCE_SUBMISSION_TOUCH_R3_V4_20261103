#include <assert.h>
#include <stdio.h>
#include "../../d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/ecg_recorder.c"
static int sync_calls, close_calls, fail_sync, fail_close;
void xil_printf(const char8 *format, ...) { (void)format; }
FRESULT f_sync(FIL *file)
{ (void)file; return ++sync_calls == fail_sync ? FR_DISK_ERR : FR_OK; }
FRESULT f_close(FIL *file)
{ (void)file; return ++close_calls == fail_close ? FR_DISK_ERR : FR_OK; }
int main(void)
{
    EcgRecorder rec;
    int scenario;
    for (scenario = 0; scenario < 5; ++scenario) {
        memset(&rec, 0, sizeof(rec));
        rec.recording = 1;
        rec.sample_file = &g_sample_fil;
        rec.event_file = &g_event_fil;
        sync_calls = close_calls = 0;
        fail_sync = scenario < 3 ? scenario : 0;
        fail_close = scenario >= 3 ? scenario - 2 : 0;
        assert((ecg_recorder_stop(&rec) == 0) == (scenario == 0));
        assert(sync_calls == 2 && close_calls == 2);
        assert(!rec.recording && !rec.sample_file && !rec.event_file);
    }
    sync_calls = close_calls = 0;
    assert(ecg_recorder_stop(&rec) == 0 && !sync_calls && !close_calls);
    puts("PASS recorder close: both syncs and closes attempted, failures propagated, no false successful close");
    return 0;
}
