#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <math.h>

#include "pipewire/stream.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#define M_PI_M2 (M_PI + M_PI)

#define DEFAULT_RATE 48000
#define DEFAULT_CHANNELS 1
#define DEFAULT_FORMAT SPA_AUDIO_FORMAT_S32
#define DEFAULT_FREQ 1000
#define DEFAULT_VOLUME 0.5
#define DEFAULT_TARGET NULL

struct parameters {
    double freq;
    double volume;
};

struct ctx {
    struct pw_main_loop* loop;
    struct pw_stream* stream;
    double accumulator;
    struct parameters params;
    const char* target;
};

static void on_process(void* userdata)
{
    struct ctx* ctx = userdata;
    struct pw_buffer* b;
    struct spa_buffer* buf;
    int c, stride;
    uint64_t n_frames, i;
    int32_t *dst, val;

    if ((b = pw_stream_dequeue_buffer(ctx->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((dst = buf->datas[0].data) == NULL) {
        return;
    }

    stride = sizeof(int32_t) * DEFAULT_CHANNELS;
    n_frames = buf->datas[0].maxsize / stride;
    if (b->requested)
        n_frames = SPA_MIN(b->requested, n_frames);

    for (i = 0; i < n_frames; i++) {
        ctx->accumulator += M_PI_M2 * ctx->params.freq / DEFAULT_RATE;
        if (ctx->accumulator >= M_PI_M2) {
            ctx->accumulator -= M_PI_M2;
        }

        val = sin(ctx->accumulator) * ctx->params.volume * 2147483647.0;
        for (c = 0; c < DEFAULT_CHANNELS; c++) {
            *dst++ = val;
        }
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = n_frames * stride;

    pw_stream_queue_buffer(ctx->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

static void show_help(const char* name, bool error)
{
    fprintf(error ? stderr : stdout, "%s [options]\n"
                                     "  -h, --help                            Show this help\n"
                                     "      --version                         Show version\n"
                                     "  -f, --frequency                       Set frequency\n"
                                     "  -v, --volume                          Set volume\n"
                                     "  -t, --target                          Set node target (node.name or object.serial)\n",
        name);
}

int main(int argc, char* argv[])
{
    int c;
    struct ctx ctx = { 0 };
    const struct spa_pod* params[1];
    uint8_t buffer[1024];
    enum pw_stream_flags flags = 0;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    static const struct option long_options[] = {
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'V' },
        { "frequency", required_argument, NULL, 'f' },
        { "volume", required_argument, NULL, 'v' },
        { "target", required_argument, NULL, 't' },
        { NULL, 0, NULL, 0 }
    };

    pw_init(&argc, &argv);

    ctx.params.freq = DEFAULT_FREQ;
    ctx.params.volume = DEFAULT_VOLUME;

    while ((c = getopt_long(argc, argv, "hVf:v:t:", long_options, NULL)) != -1) {
        switch (c) {
        case 'h':
            show_help(argv[0], false);
            return 0;
        case 'V':
            printf("%s\n"
                   "Compiled with libpipewire %s\n"
                   "Linked with libpipewire %s\n",
                argv[0],
                pw_get_headers_version(),
                pw_get_library_version());
            return 0;
        case 'f':
            if (optarg == NULL) {
                fprintf(stderr, "Error: -f option requires a numeric argument.\n");
                return -1;
            }
            ctx.params.freq = atof(optarg);
            break;
        case 'v':
            if (optarg == NULL) {
                fprintf(stderr, "Error: -v option requires a numeric argument.\n");
                return -1;
            }
            ctx.params.volume = atof(optarg);
            break;
        case 't':
            ctx.target = optarg;
            if (optarg != NULL) {
                flags |= PW_STREAM_FLAG_AUTOCONNECT;
            }
            if (spa_streq(ctx.target, "0")) {
                ctx.target = NULL;
                flags &= ~PW_STREAM_FLAG_AUTOCONNECT;
            }
            break;
        default:
            show_help(argv[0], true);
            return -1;
        }
    }

    ctx.loop = pw_main_loop_new(NULL);

    ctx.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(ctx.loop),
        "pw-sine",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            PW_KEY_TARGET_OBJECT, ctx.target,
            NULL),
        &stream_events,
        &ctx);

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
                .format = SPA_AUDIO_FORMAT_S32,
            .channels = DEFAULT_CHANNELS,
            .rate = DEFAULT_RATE));

    pw_stream_connect(ctx.stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        flags | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
        params, 1);

    pw_main_loop_run(ctx.loop);

    pw_stream_destroy(ctx.stream);
    pw_main_loop_destroy(ctx.loop);

    pw_deinit();

    return 0;
}
