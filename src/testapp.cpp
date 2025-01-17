#define NOMINMAX
#define _USE_MATH_DEFINES
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <cerrno>

#include "Analyzer.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

const    ma_format sample_format = ma_format_f32;
typedef                            ma_float       sample_t;

typedef struct _ctx_t
{
    Analyzer analyzer;
    ma_device device;
    const char *infile;
    std::unique_ptr<sample_t[]> framebuf;
    uint32_t sampleRate;
    uint32_t channels;
    uint64_t totalPCMFrameCount;
    uint64_t readCursorInPCMFrames;
    bool quit;
} ctx_t;

int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++)
    {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}

uint64_t analyze_frames(ctx_t &ctx, uint64_t from, uint64_t count)
{
    assert(from < ctx.totalPCMFrameCount);
    if (ctx.totalPCMFrameCount - from < count)
        count = ctx.totalPCMFrameCount - from;
    uint64_t ret = count;
    uint32_t nch = ctx.channels;
    from *= nch; // convert to samples
    count *= nch;
    const sample_t *bufptr = ctx.framebuf.get() + from;
    for(size_t i = 0; i < count; i += nch)
    {
        sample_t sample = bufptr[i];
        for (size_t ch = 1; ch < nch; ++ch) // downmix to mono
            sample += bufptr[i + ch]; // will overflow on integral formats
        ctx.analyzer.addData(sample / nch);
    }
    return ret;
}

int create_pitch_map(ctx_t &ctx, const char *outfile)
{
    std::ofstream of;
    if (outfile)
    {
        of.open(outfile);
        if (!of.is_open())
            return -errno;
        printf("Writing pitchmap of %s to %s\n", ctx.infile, outfile);
    }

    uint64_t count = ctx.totalPCMFrameCount;
    uint64_t offset = 0;
    while (offset < count)
    {
        uint64_t analyzed = analyze_frames(ctx, offset, Analyzer::ANALYZE_INTERVAL);
        if (outfile)
            of << (double)offset / ctx.sampleRate << " " << ctx.analyzer.get_peak_freq() << std::endl;
        offset += analyzed;
    }

    if (outfile)
        of.close();

    return 0;
}

ma_result load_file(ctx_t &ctx, ma_uint32 nch = 2)
{
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_uint64 total, read;
    ma_result result;

    decoderConfig = ma_decoder_config_init(sample_format, nch, 44100);
    result = ma_decoder_init_file(ctx.infile, &decoderConfig, &decoder);
    if (result != MA_SUCCESS)
        return result;

    ma_decoder_get_length_in_pcm_frames(&decoder, &total);
    std::unique_ptr<sample_t[]> newbuf(new sample_t[total * decoder.outputChannels * sizeof(sample_t)]());
    assert(newbuf != nullptr);

    result = ma_decoder_read_pcm_frames(&decoder, newbuf.get(), total, &read);
    if (result != MA_SUCCESS)
        return result;

    ctx.totalPCMFrameCount = read;
    ctx.readCursorInPCMFrames = 0;
    ctx.sampleRate = decoder.outputSampleRate;
    ctx.channels = decoder.outputChannels;
    ctx.framebuf = std::move(newbuf);

    ma_decoder_uninit(&decoder);

    return result;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ctx_t *ctx = (ctx_t*)pDevice->pUserData;
    assert(ctx != nullptr);

    if (frameCount > ctx->totalPCMFrameCount - ctx->readCursorInPCMFrames)
        frameCount = (ma_uint32)(ctx->totalPCMFrameCount - ctx->readCursorInPCMFrames);

    if (frameCount > 0)
    {
        const sample_t *bufptr = ctx->framebuf.get() + ctx->readCursorInPCMFrames * ctx->channels;
        memcpy(pOutput, bufptr, frameCount * ctx->channels * sizeof(sample_t));
        analyze_frames(*ctx, ctx->readCursorInPCMFrames, frameCount);
        ctx->readCursorInPCMFrames += frameCount;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s cmd [cmd_opts]\n", argv[0]);
        printf("commands:\n");
        printf("  p: play file <file.wav>\n");
        printf("  c: create pitch map for file <file.wav>\n");
        printf("  b: benchmark on <file.wav>\n");
        printf("  d: FFT dump at frame <file.wav> <frame> [back_intervals]\n");
        printf("  e: print detection error at frame <file.wav> <frame> <ref_value> [max_err_cents]\n");
        return -1;
    }

    ma_result result;
    char op = argv[1][0];
    ctx_t ctx = { };
    ctx.infile = argv[2];

    result = load_file(ctx, op == 'p' ? 2 : 1);
    if (result != MA_SUCCESS)
    {
        printf("failed to open %s: %s\n", ctx.infile, ma_result_description(result));
        return -1;
    }

    switch(op)
    {
        case 'p':
        {
            ma_device_config config = ma_device_config_init(ma_device_type_playback);
            config.playback.format   = sample_format;  // Set to ma_format_unknown to use the device's native format.
            config.playback.channels = ctx.channels;   // Set to 0 to use the device's native channel count.
            config.sampleRate        = ctx.sampleRate; // Set to 0 to use the device's native sample rate.
            config.dataCallback      = data_callback;  // This function will be called when miniaudio needs more data.
            config.pUserData         = &ctx;           // Can be accessed from the device object (device.pUserData).
            config.periodSizeInFrames = (ma_uint32)Analyzer::ANALYZE_INTERVAL;
            result = ma_device_init(NULL, &config, &ctx.device);
            if (result != MA_SUCCESS)
            {
                printf("failed to init ma_device: %s\n", ma_result_description(result));
                return -1;
            }

            /*ma_context_config contextConfig = ma_context_config_init();
            contextConfig.threadPriority = ma_thread_priority_realtime;*/

            ma_device_start(&ctx.device);
            std::this_thread::sleep_for(std::chrono::milliseconds((ctx.totalPCMFrameCount + Analyzer::ANALYZE_INTERVAL) * 1000 / ctx.sampleRate));
            ma_device_stop(&ctx.device);
            ma_device_uninit(&ctx.device);
        } break;
        case 'c':
            create_pitch_map(ctx, argc > 3 ? argv[3] : "pitchmap.txt");
        break;
        case 'b':
        {
            const int cnt = 10;
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            for (int i = 0; i < cnt; ++i)
                create_pitch_map(ctx, nullptr);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            printf("%.2f events/s\n", (double)ctx.totalPCMFrameCount / Analyzer::ANALYZE_INTERVAL * cnt *
                1000000UL / std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
        } break;
        case 'd':
        {
            if (argc < 4)
            {
                printf("specify after the file name the frame number at which the state should be dumped,\n"
                       "%zu <= N <= %" PRIu64 " for this file\n"
                       "negative value offsets from file end\n",
                    Analyzer::ANALYZE_INTERVAL, ctx.totalPCMFrameCount);
                printf("optionally, maximum backtrack interval count can be specified, upto default %zu\n",
                    Analyzer::PITCH_BUF_SIZE);
                return -1;
            }
            int64_t at = std::strtol(argv[3], NULL, 0);
            if (at < 0)
                at = ctx.totalPCMFrameCount + at + 1;
            if (at < Analyzer::ANALYZE_INTERVAL || (uint64_t)at > ctx.totalPCMFrameCount)
            {
                printf("frame number for this file should be in range %zu <= N <= %" PRIu64 "\n",
                    Analyzer::ANALYZE_INTERVAL, ctx.totalPCMFrameCount);
                return -1;
            }
            int64_t start = at - Analyzer::PITCH_BUF_SIZE * Analyzer::ANALYZE_INTERVAL;
            if (argc > 4)
            {
                int64_t maxcnt = std::strtol(argv[4], NULL, 0);
                if (maxcnt < 1)
                    maxcnt = 1;
                else if (maxcnt > Analyzer::PITCH_BUF_SIZE)
                    maxcnt = Analyzer::PITCH_BUF_SIZE;
                start = at - maxcnt * Analyzer::ANALYZE_INTERVAL;
            }
            if (start < 0)
                start = 0;

            ctx.analyzer.clearData();
            analyze_frames(ctx, start, at - start);
            ctx.analyzer.dump();
        } break;
        case 'e':
        {
            if (argc < 5)
            {
                printf("specify after the file name the frame number at which the state should be dumped,\n"
                       "%zu <= N <= %" PRIu64 " for this file\n"
                       "negative value offsets from file end\n"
                       "and reference frequency in Hz\n"
                       "optionally, specify maximum error %%\n",
                    Analyzer::ANALYZE_INTERVAL, ctx.totalPCMFrameCount);
                return -1;
            }
            int64_t at = std::strtol(argv[3], NULL, 0);
            if (at < 0)
                at = ctx.totalPCMFrameCount + at + 1;
            if (at < Analyzer::ANALYZE_INTERVAL || (uint64_t)at > ctx.totalPCMFrameCount)
            {
                printf("frame number for this file should be in range %zu <= N <= %" PRIu64 "\n",
                    Analyzer::ANALYZE_INTERVAL, ctx.totalPCMFrameCount);
                return -1;
            }
            int64_t start = at - ((int)((double)Analyzer::FFTSIZE / Analyzer::ANALYZE_INTERVAL) + 1) * Analyzer::ANALYZE_INTERVAL;
            if (start < 0)
                start = 0;
            double fref = strtod(argv[4], nullptr);
            double maxerr = -1.0;
            if(argc > 5)
                maxerr = strtod(argv[5], nullptr);

            ctx.analyzer.clearData();
            analyze_frames(ctx, start, at - start);

            double pitchf = ctx.analyzer.get_peak_freq();
            double err = Analyzer::freq_to_cent(pitchf) - Analyzer::freq_to_cent(fref);
            printf("pitch: %g ref %g dHz %0.3f err%% %0.2f", pitchf, fref, pitchf - fref, err);
            if (maxerr > 0.0 && std::abs(err) > maxerr)
                printf(" OUT");
            printf("\n");
        } break;
        default:
            printf("unknown command %c\n", op);
            return -1;
    }

    return 0;
}
