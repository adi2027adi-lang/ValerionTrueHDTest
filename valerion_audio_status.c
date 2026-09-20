
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    int32_t codec;
    int32_t channels;
    int32_t sample_rate;
} HuiAudioInfo;

typedef int (*HUI_AOutGetAudioInfo_fn)(HuiAudioInfo* info);

static const char* codec_name(int c) {
    switch (c) {
        case 5: return "DDP";
        case 6: return "TRUEHD";
        case 7: return "MAT";
        case 8: return "DDP_ATMOS";
        case 9: return "TRUEHD_ATMOS";
        case 10: return "MAT_ATMOS";
        case 11: return "DTS";
        case 12: return "DTS_HD";
        case 13: return "DTS_X";
        default: return "UNKNOWN";
    }
}

static void stamp(char *buf, size_t n) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm t;
    localtime_r(&ts.tv_sec, &t);
    snprintf(buf, n, "%02d:%02d:%02d.%03ld",
             t.tm_hour, t.tm_min, t.tm_sec, ts.tv_nsec / 1000000L);
}

int main(int argc, char** argv) {
    int count = 30;
    int delay_ms = 1000;
    const char* libpath = "/vendor/lib/libAvHui.so";

    if (argc > 1) count = atoi(argv[1]);
    if (argc > 2) delay_ms = atoi(argv[2]);
    if (argc > 3) libpath = argv[3];
    if (count <= 0) count = 30;
    if (delay_ms < 100) delay_ms = 100;

    void* h = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        fprintf(stderr, "DLOPEN_FAIL path=%s error=%s\n", libpath, dlerror());
        h = dlopen("libAvHui.so", RTLD_NOW | RTLD_LOCAL);
        if (!h) {
            fprintf(stderr, "DLOPEN_FAIL fallback=libAvHui.so error=%s\n", dlerror());
            return 10;
        }
    }

    dlerror();
    HUI_AOutGetAudioInfo_fn fn =
        (HUI_AOutGetAudioInfo_fn)dlsym(h, "HUI_AOutGetAudioInfo");
    const char* err = dlerror();
    if (err || !fn) {
        fprintf(stderr, "DLSYM_FAIL HUI_AOutGetAudioInfo error=%s\n",
                err ? err : "unknown");
        dlclose(h);
        return 11;
    }

    printf("VALERION_AUDIO_STATUS_READY count=%d interval_ms=%d\n", count, delay_ms);
    printf("codec mapping: 7=MAT, 10=MAT_ATMOS\n");
    fflush(stdout);

    for (int i = 0; i < count; ++i) {
        HuiAudioInfo info;
        memset(&info, 0, sizeof(info));

        int ret = fn(&info);

        char tbuf[32];
        stamp(tbuf, sizeof(tbuf));
        printf("%s | ret=%d | codec=%d (%s) | channels=%d | sampleRate=%d\n",
               tbuf, ret, info.codec, codec_name(info.codec),
               info.channels, info.sample_rate);
        fflush(stdout);

        usleep((useconds_t)delay_ms * 1000U);
    }

    dlclose(h);
    return 0;
}
