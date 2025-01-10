#include <cstdio> // FIXME remove
#include <cctype> // toupper
#include <sstream>
#include <iomanip>
#include <chrono>
#include <locale>
#include <algorithm>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

//#define DBG(...) fprintf(stderr, __VA_ARGS__)
#define DBG(...)

#define MINIAUDIO_IMPLEMENTATION
#include "AudioHandler.h"

#if defined(HAVE_OPUS)
static ma_result ma_decoding_backend_init__libopus(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_libopus* pOpus;

    (void)pUserData;

    pOpus = (ma_libopus*)ma_malloc(sizeof(*pOpus), pAllocationCallbacks);
    if (pOpus == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_libopus_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pOpus);
    if (result != MA_SUCCESS) {
        ma_free(pOpus, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pOpus;

    return MA_SUCCESS;
}

static ma_result ma_decoding_backend_init_file__libopus(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_libopus* pOpus;

    (void)pUserData;

    pOpus = (ma_libopus*)ma_malloc(sizeof(*pOpus), pAllocationCallbacks);
    if (pOpus == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_libopus_init_file(pFilePath, pConfig, pAllocationCallbacks, pOpus);
    if (result != MA_SUCCESS) {
        ma_free(pOpus, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pOpus;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__libopus(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_libopus* pOpus = (ma_libopus*)pBackend;

    (void)pUserData;

    ma_libopus_uninit(pOpus, pAllocationCallbacks);
    ma_free(pOpus, pAllocationCallbacks);
}

static ma_result ma_decoding_backend_get_channel_map__libopus(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
{
    ma_libopus* pOpus = (ma_libopus*)pBackend;

    (void)pUserData;

    return ma_libopus_get_data_format(pOpus, NULL, NULL, NULL, pChannelMap, channelMapCap);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_libopus =
{
    ma_decoding_backend_init__libopus,
    ma_decoding_backend_init_file__libopus,
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoding_backend_uninit__libopus
};

static ma_decoding_backend_vtable* pCustomBackendVTables[] =
{
    &g_ma_decoding_backend_vtable_libopus
};
#endif // defined(HAVE_OPUS)

static bool match(const std::string& a, const std::string& b, const std::locale& loc = std::locale())
{
    auto it = std::search( a.begin(), a.end(), b.begin(), b.end(),
        [loc] (const char &cha, const char &chb) -> bool
        { return std::toupper(cha, loc) == std::toupper(chb, loc); });
    return it != a.end();
}

// wide char wrappers for Windows
#if defined(MA_WIN32)
static ma_result decoder_init_file(const char* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder)
{
    int bufsz = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pFilePath, -1, nullptr, 0);
    if (bufsz == 0)
        return MA_INVALID_ARGS;
    auto buf = std::unique_ptr<wchar_t[]>(new wchar_t[bufsz]());
    if (buf == nullptr)
        return MA_OUT_OF_MEMORY;
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pFilePath, -1, buf.get(), bufsz);

    return ma_decoder_init_file_w(buf.get(), pConfig, pDecoder);
}
static ma_result encoder_init_file(const char* pFilePath, const ma_encoder_config* pConfig, ma_encoder* pEncoder)
{
    int bufsz = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pFilePath, -1, nullptr, 0);
    if (bufsz == 0)
        return MA_INVALID_ARGS;
    auto buf = std::unique_ptr<wchar_t[]>(new wchar_t[bufsz]());
    if (buf == nullptr)
        return MA_OUT_OF_MEMORY;
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pFilePath, -1, buf.get(), bufsz);

    return ma_encoder_init_file_w(buf.get(), pConfig, pEncoder);
}
#else
#define decoder_init_file ma_decoder_init_file
#define encoder_init_file ma_encoder_init_file
#endif // !defined(MA_WIN32)

using namespace logger;

static void ah_log_callback(void* pUserData, ma_uint32 level, const char* pMessage)
{
    Logger *pLog = reinterpret_cast<Logger*>(pUserData);
    if (!pLog)
        return;

    // treat miniaudio internal messages as DEBUG
    pLog->LogMsg(LOG_DBG, "%s", pMessage);
}

static ma_bool32 ah_device_enum_callback(ma_context* pContext, ma_device_type deviceType,
                                         const ma_device_info* pInfo, void* pUserData)
{
    AudioHandler::privateContext *ppc = reinterpret_cast<AudioHandler::privateContext*>(pUserData);
    if (!ppc)
        return MA_FALSE;

    AudioHandler::Devices *devices;
    if (deviceType == ma_device_type_playback)
        devices = &ppc->playbackDevices;
    else
        devices = &ppc->captureDevices;

    if (pInfo->isDefault) {
        devices->defaultIdx = devices->list.size();
        if (devices->selectedName.empty())
            devices->selectedName = pInfo->name;
    }
    devices->list.push_back(AudioHandler::Devices::Device({pInfo->name, pInfo->id}));

    ppc->log.LogMsg(LOG_DBG, " %s: %s%s", deviceType == ma_device_type_capture ? "Capture" : "Playback", pInfo->name, pInfo->isDefault ? " [default]" : "" );

    return MA_TRUE;
}

static void ah_device_callback(const ma_device_notification* pNotification)
{
    AudioHandler::privateContext *ppc = reinterpret_cast<AudioHandler::privateContext*>(pNotification->pDevice->pUserData);
    if (!ppc)
        return;

    std::unique_lock<std::timed_mutex> lock(ppc->mutex, std::chrono::milliseconds(10));
    if (pNotification->type == ma_device_notification_type_stopped && ppc->state.isActive()) {
        // device externally stopped, sync the state
        ppc->log.LogMsg(LOG_WARN, "%s device stopped",
            pNotification->pDevice->type == ma_device_type_playback ? "Playback" : "Capture");
        ppc->cmdQueue.internalCommand(AudioHandler::CmdStop);
    }
}

static void ah_play_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    AudioHandler::privateContext *ppc = reinterpret_cast<AudioHandler::privateContext*>(pDevice->pUserData);
    if (!ppc)
        return;

    std::unique_lock<std::timed_mutex> lock(ppc->mutex, std::chrono::milliseconds(5));
    if (lock.owns_lock()) {
        if (!ppc->state.isActive() || !ppc->device || !ppc->decoder)
            return;
        ma_uint64 framesRead;
        ma_result result = ma_decoder_read_pcm_frames(ppc->decoder.get(), pOutput, frameCount, &framesRead);
        if (result != MA_SUCCESS) {
            if (result == MA_AT_END) {
                ppc->state |= AudioHandler::StateEOF;
                ppc->cmdQueue.internalCommand(ppc->playbackEOFcmd);
                ppc->log.LogMsg(LOG_INFO, "End of file");
            } else {
                ppc->backendError = result;
                ppc->cmdQueue.internalCommand(AudioHandler::CmdStop);
                ppc->log.LogMsg(LOG_ERR, "Error reading file: %s\n", ma_result_description(result));
            }
            return;
        }

        if (ppc->frameDataCbProc)
            ppc->frameDataCbProc((AudioHandler::Format)pDevice->playback.format, pDevice->playback.channels, pOutput, framesRead, ppc->frameDataCbUserData);
    }

    (void)pInput;
}

static void ah_capture_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    AudioHandler::privateContext *ppc = reinterpret_cast<AudioHandler::privateContext*>(pDevice->pUserData);
    if (!ppc)
        return;

    std::unique_lock<std::timed_mutex> lock(ppc->mutex, std::chrono::milliseconds(5));
    if (lock.owns_lock()) {
        if (!ppc->state.isActive() || !ppc->device)
            return;

        if (ppc->frameDataCbProc)
            ppc->frameDataCbProc((AudioHandler::Format)pDevice->capture.format, pDevice->capture.channels, pInput, frameCount, ppc->frameDataCbUserData);

        if (ppc->encoder) {
            ma_result result = MA_SUCCESS;
            if (pDevice->capture.format == ppc->encoder->config.format)
                result = ma_encoder_write_pcm_frames(ppc->encoder.get(), pInput, frameCount, nullptr);
            else {
                std::unique_ptr<char[]> outBuf(new char[frameCount * ma_get_bytes_per_frame(ppc->encoder->config.format,
                                                                                         ppc->encoder->config.channels)]);
                if (outBuf != nullptr) { // just ignore frames on allocation error
                    ma_pcm_convert(outBuf.get(), ppc->encoder->config.format, pInput, pDevice->capture.format, frameCount * pDevice->capture.channels, ma_dither_mode_none);
                    result = ma_encoder_write_pcm_frames(ppc->encoder.get(), outBuf.get(), frameCount, nullptr);
                }
            }
            if (result != MA_SUCCESS) {
                ppc->backendError = result;
                ppc->cmdQueue.internalCommand(AudioHandler::CmdStop);
                ppc->log.LogMsg(LOG_ERR, "Error writing file: %s\n", ma_result_description(result));
                return;
            }
        }
    }
    (void)pOutput;
}

AudioHandler::privateContext::privateContext() :
            state(StateExit),
            backendError(MA_SUCCESS),
            updatePlaybackFileName(false),
            length(0),
            playbackEOFcmd(CmdStop),
            playbackVolumeFactor(1.0f),
            context(new ma_context),
            device(nullptr),
            encoder(nullptr),
            decoder(nullptr),
            frameDataCbProc(nullptr),
            frameDataCbUserData(nullptr),
            notificationCbProc(nullptr),
            notificationCbUserData(nullptr),
            notificationCbMask(0)
{
    ma_result result;
    if (!context
        || (result = ma_context_init(NULL, 0, NULL, context.get())) != MA_SUCCESS) {
        backendError = context ? result : MA_OUT_OF_MEMORY;
        context = nullptr;
        log.LogMsg(LOG_ERR, "Failed to init audio context: %s", ma_result_description(backendError));
        return;
    }

    ma_log_register_callback(&context->log, ma_log_callback_init(ah_log_callback, &log));
    context->pUserData = this;
}

AudioHandler::privateContext::~privateContext()
{
}

AudioHandler::AudioHandler(uint32_t _sampleRateHz, uint32_t _channels, Format _sampleFormat, Format _recordFormat) :
                           AudioHandler(_sampleRateHz,
                                        _channels,
                                        _sampleFormat,
                                        _recordFormat,
                                        ma_calculate_buffer_size_in_frames_from_milliseconds(
                                            MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_CONSERVATIVE,
                                            _sampleRateHz
                                        ))
{
}

AudioHandler::AudioHandler(uint32_t _sampleRateHz, uint32_t _channels, Format _sampleFormat, Format _recordFormat, uint32_t _frameDataCbInterval) :
                           sampleRateHz(_sampleRateHz),
                           channels(_channels),
                           sampleFormat((ma_format)_sampleFormat),
                           recordFormat((ma_format)_recordFormat),
                           frameDataCbInterval(_frameDataCbInterval)
{
    if (pc.context) {
        std::unique_lock<std::timed_mutex> lock(pc.mutex);
        commandThread = std::thread( &AudioHandler::commandProc, this );
        pc.cond.wait(lock);
    }
}

AudioHandler::~AudioHandler()
{
    pc.cmdQueue.internalCommand(CmdExit);
    if (commandThread.joinable())
        commandThread.join();
}

void AudioHandler::attachFrameDataCb(frameDataCb cbProc, void *userData)
{
    std::lock_guard<std::timed_mutex> lock(pc.mutex);

    pc.frameDataCbProc = cbProc;
    pc.frameDataCbUserData = userData;
}

void AudioHandler::removeFrameDataCb()
{
    std::lock_guard<std::timed_mutex> lock(pc.mutex);

    pc.frameDataCbProc = nullptr;
    pc.frameDataCbUserData = nullptr;
}

void AudioHandler::attachNotificationCb(unsigned mask, notificationCb cbProc, void *userData)
{
    std::lock_guard<std::timed_mutex> lock(pc.mutex);

    pc.notificationCbProc = cbProc;
    pc.notificationCbUserData = userData;
    pc.notificationCbMask = cbProc ? mask : 0;
}

void AudioHandler::removeNotificationCb()
{
    std::lock_guard<std::timed_mutex> lock(pc.mutex);

    pc.notificationCbMask = 0;
    pc.notificationCbProc = nullptr;
    pc.notificationCbUserData = nullptr;
}

void AudioHandler::enumerate()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdEnumerateDevices);
}

const AudioHandler::Devices &AudioHandler::getPlaybackDevices()
{
    pc.device_mutex.lock();
    return pc.playbackDevices;
}

const AudioHandler::Devices &AudioHandler::getCaptureDevices()
{
    pc.device_mutex.lock();
    return pc.captureDevices;
}

void AudioHandler::unlockDevices()
{
    pc.device_mutex.unlock();
}

void AudioHandler::setPreferredPlaybackDevice(const char *deviceName)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdSwitchPlaybackDevice, deviceName});
}

void AudioHandler::setPreferredCaptureDevice(const char *deviceName)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdSwitchCaptureDevice, deviceName});
}

void AudioHandler::setPlaybackVolumeFactor(const float &volumeFactor)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdSetPlaybackVolume, volumeFactor});
}

bool AudioHandler::getPlaybackVolumeFactor(float &volumeFactor)
{
    if (!pc.context)
        return false;
    std::unique_lock<std::timed_mutex> lock(pc.mutex, std::chrono::milliseconds(10));
    if (!lock.owns_lock())
        return false;

    if (pc.device && pc.device->type == ma_device_type_playback)
        volumeFactor = ma_atomic_float_get(&pc.device->masterVolumeFactor);
    else
        volumeFactor = pc.playbackVolumeFactor;
    
    return true;
}

bool AudioHandler::setPlaybackEOFaction(Command cmd)
{
    switch(cmd) {
    case CmdStop:
    case CmdPlay:
    case CmdPause:
    case CmdRewind:
    case CmdCapture:
        break;
    default:
        return false;
    }

    std::lock_guard<std::timed_mutex> lock(pc.mutex);

    pc.playbackEOFcmd = cmd;

    return true;
}

void AudioHandler::setPlaybackFileName(const char *fileName)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdSetPlaybackFileName, fileName});
}

void AudioHandler::stop()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdStop);
}

void AudioHandler::play(const char *fileName)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdPlay, fileName});
}

void AudioHandler::capture()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdCapture);
}

void AudioHandler::record(const char *fileName)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdRecord, fileName});
}

void AudioHandler::seek(uint64_t posInPcmFrames)
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand({CmdSeek, posInPcmFrames});
}

void AudioHandler::rewind()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdRewind);
}

void AudioHandler::pause()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdPause);
}

void AudioHandler::resume()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdResume);
}

void AudioHandler::togglePause()
{
    if (!pc.context)
        return;

    pc.cmdQueue.userCommand(CmdTogglePause);
}

bool AudioHandler::getState(State &state, uint64_t *lenInPcmFrames, uint64_t *posInPcmFrames)
{
    std::unique_lock<std::timed_mutex> lock(pc.mutex, std::chrono::milliseconds(10));
    if (!lock.owns_lock())
        return false;

    if (lenInPcmFrames)
        *lenInPcmFrames = isOperational() ? pc.length : 0;
    if (posInPcmFrames) {
        if (pc.decoder) {
            // file cursor for playback
            ma_uint64 cursor;
            if (ma_decoder_get_cursor_in_pcm_frames(pc.decoder.get(), &cursor) == MA_SUCCESS)
                *posInPcmFrames = cursor;
            else
                *posInPcmFrames = 0;
        } else
            // same as length for capture/recording
            *posInPcmFrames = isOperational() ? pc.length : 0;
    }
    state = pc.state;

    return true;
}

bool AudioHandler::getError(int *error, const char **description)
{
    std::unique_lock<std::timed_mutex> lock(pc.mutex, std::chrono::milliseconds(10));
    if (!lock.owns_lock())
        return false;

    if (error)
        *error = pc.genericError > ErrorBase ? (int)pc.stateError : (int)pc.backendError;
    if (description) {
        if (pc.genericError > ErrorBase)
            *description = pc.stateError < ErrorLast ? ErrorDescriptions[pc.stateError - ErrorBase - 1] : nullptr;
        else
            *description = pc.backendError != MA_SUCCESS ? ma_result_description(pc.backendError) : nullptr;
    }

    pc.backendError = MA_SUCCESS;

    return true;
}

std::string AudioHandler::framesToTime(uint64_t frames, uint32_t sampleRateHz)
{
    auto hours   = div(frames / sampleRateHz, 3600);
    auto minutes = div(hours.rem, 60);

    std::stringstream ss;
    if (hours.quot) ss << hours.quot << ":";
    ss << minutes.quot << ":";
    ss << std::setw(2) << std::setfill('0') << minutes.rem;

    return ss.str();
}

void AudioHandler::commandProc()
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_encoder_config encoderConfig;
    ma_decoder_config decoderConfig;
    static const std::string default_device("default");  // for notifications
    const std::string *lastDeviceName = &default_device;

    {
        std::lock_guard<std::timed_mutex> lock(pc.mutex);
        pc.state |= StateReady; // ready for commands
        pc.cond.notify_one();
    }
    do {
        Cmd cc = pc.cmdQueue.pendingCommand();
        std::lock_guard<std::timed_mutex> lock(pc.mutex);
        DBG("cmd %u\n", cc.cmd);
        switch(cc.cmd) {
        case CmdNone:
            break;
        case CmdPlay:
            if (cc.fromuser && !pc.state.canPlay(!cc.argStr.empty())) {
                pc.stateError = ErrorInvalidSequence;
                break;
            }
            if (pc.state.isPlaying() && cc.argStr == pc.lastFileName) { // already playing and same file requested
                // reverse order
                pc.cmdQueue.internalCommand(CmdResume);          // and resume
                if (pc.state.atEOF())
                    pc.cmdQueue.internalCommand(CmdRewind);      // rewind if EOF
                break;
            }
            if (!cc.argStr.empty())
                pc.lastFileName = cc.argStr;
            else if (!pc.playbackFileName.empty())
                pc.lastFileName = pc.playbackFileName;
            else {
                pc.cmdQueue.internalCommand(CmdStop);
                break;
            }

            decoderConfig = ma_decoder_config_init(sampleFormat, channels, sampleRateHz);
#if defined(HAVE_OPUS)
            decoderConfig.pCustomBackendUserData = NULL;  /* In this example our backend objects are contained within a ma_decoder_ex object to avoid a malloc. Our vtables need to know about this. */
            decoderConfig.ppCustomBackendVTables = pCustomBackendVTables;
            decoderConfig.customBackendCount     = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
#endif // defined(HAVE_OPUS)

            pc.decoder = ma_unique_decoder(new ma_decoder());
            if (!pc.decoder
                || (result = decoder_init_file(pc.lastFileName.c_str(), &decoderConfig, pc.decoder.get())) != MA_SUCCESS) {
                pc.backendError = pc.decoder ? result : MA_OUT_OF_MEMORY;
                pc.decoder = nullptr;
                pc.log.LogMsg(LOG_ERR, "%s: failed to open file: %s", pc.lastFileName.c_str(),
                                                                      ma_result_description(pc.backendError));
                pc.cmdQueue.set(CmdStop);
                break;
            }
            pc.decoder->pUserData = &pc;
            ma_decoder_get_length_in_pcm_frames(pc.decoder.get(), &pc.length);
            pc.playbackFileName = pc.lastFileName; pc.state.hasPlaybackFile = true;
            pc.state = StatePlayback|StatePause;
            pc.cmdQueue.internalCommand(CmdResume);

            pc.log.LogMsg(LOG_INFO, "Playing file: %s (%s)", pc.lastFileName.c_str(), framesToTime(pc.length).c_str());
            if (pc.notificationCbMask & EventPlayFile)
                pc.notificationCbProc({EventPlayFile, pc.lastFileName.c_str(), 0}, pc.notificationCbUserData);
            break;
        case CmdCapture:
            if (cc.fromuser && !pc.state.canCapture()) {
                pc.stateError = ErrorInvalidSequence;
                break;
            }
            if (pc.state.isRecording()) // stop recording
                pc.encoder = nullptr;

            pc.state = StateCapture|StatePause;
            pc.cmdQueue.internalCommand(CmdResume);
            break;
        case CmdRecord:
            if (cc.fromuser && !pc.state.canRecord()) {
                pc.stateError = ErrorInvalidSequence;
                break;
            }
            if (cc.argStr.empty() || cc.argStr == pc.lastFileName) {
                pc.stateError = ErrorInvalidCmdArg;
                break;
            }
            pc.lastFileName = cc.argStr;

            encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, recordFormat, channels, sampleRateHz);

            pc.encoder = ma_unique_encoder(new ma_encoder());
            if (!pc.encoder
                || (result = encoder_init_file(pc.lastFileName.c_str(), &encoderConfig, pc.encoder.get())) != MA_SUCCESS) {
                pc.backendError = pc.encoder ? result : MA_OUT_OF_MEMORY;
                pc.encoder = nullptr;
                pc.log.LogMsg(LOG_ERR, "%s: failed to create file: %s", pc.lastFileName.c_str(),
                                                                        ma_result_description(pc.backendError));
                pc.cmdQueue.set(CmdStop);
                break;
            }
            pc.encoder->pUserData = &pc;
            pc.state = StateRecord|StatePause;
            pc.cmdQueue.internalCommand(CmdResume);

            pc.log.LogMsg(LOG_INFO, "Recording to file: %s", pc.lastFileName.c_str());
            if (pc.notificationCbMask & EventRecordFile)
                pc.notificationCbProc({EventRecordFile, pc.lastFileName.c_str(), 0}, pc.notificationCbUserData);
            break;
        case CmdRewind:
            cc.argU64 = 0;
            // fall through
        case CmdSeek:
            if (cc.fromuser && !pc.state.canSeek()) {
                pc.stateError = ErrorInvalidSequence;
                break;
            }
            if (!pc.decoder)
                break;
            pc.state |= StateSeek;
            result = ma_decoder_seek_to_pcm_frame(pc.decoder.get(), cc.argU64);
            if (result != MA_SUCCESS) {
                pc.backendError = result;
                pc.log.LogMsg(LOG_ERR, "%s: failed seek to %llu: %s",
                    pc.lastFileName.c_str(), cc.argU64, ma_result_description(result));
                pc.cmdQueue.set(CmdStop);
                break;
            }
            pc.state &= ~(StateSeek|StateEOF);

            pc.log.LogMsg(LOG_INFO, "%s: seek to %llu", pc.lastFileName.c_str(), cc.argU64);
            if (pc.notificationCbMask & EventSeek)
                pc.notificationCbProc({EventSeek, pc.lastFileName.c_str(), cc.argU64}, pc.notificationCbUserData);
            break;
        case CmdPause:
            if (cc.fromuser && !pc.state.canPause()) {
                pc.stateError = ErrorInvalidSequence;
                break;
            }
            pc.state |= StatePause;
            if (pc.device && ma_device_is_started(pc.device.get())) {
                result = ma_device_stop(pc.device.get());
                if (result != MA_SUCCESS) {
                    pc.backendError = result;
                    pc.log.LogMsg(LOG_ERR, "Failed to stop %s device: %s",
                        pc.device->type == ma_device_type_playback ? "playback" : "capture",
                        ma_result_description(result));
                    pc.cmdQueue.set(CmdStop);
                    break;
                }
            }

            if (pc.notificationCbMask & EventPause)
                pc.notificationCbProc({EventPause, lastDeviceName->c_str(),
                    (uint64_t)(pc.device ? (pc.device->type == ma_device_type_playback ? EventOpPlayback : EventOpCaptureRecord) : EventOpNone)},
                    pc.notificationCbUserData);
            break;
        case CmdResume:
            if (cc.fromuser && !pc.state.canResume()) {
                pc.stateError = ErrorInvalidSequence;
                break;
            }
            if (!pc.device || (pc.device->type == ma_device_type_playback && (pc.state & StateMask) != StatePlayback)) { // no device or inappropriate device type
                Devices *devices;
                const ma_device_id **ppConfigDeviceId;

                if (pc.state.isPlaying()) {
                    if (!pc.decoder) { // invalid state
                        pc.log.LogMsg(LOG_DBG, "Invalid playback state");
                        pc.cmdQueue.set(CmdStop);
                        break;
                    }
                    deviceConfig = ma_device_config_init(ma_device_type_playback);

                    deviceConfig.playback.format    = pc.decoder->outputFormat;     // Set to ma_format_unknown to use the device's native format.
                    deviceConfig.playback.channels  = pc.decoder->outputChannels;   // Set to 0 to use the device's native channel count.
                    deviceConfig.sampleRate         = pc.decoder->outputSampleRate; // Set to 0 to use the device's native sample rate.
                    deviceConfig.dataCallback       = ah_play_callback;     // This function will be called when miniaudio needs more data.
                    deviceConfig.notificationCallback = ah_device_callback; // This function will be called when device state changes.
                    deviceConfig.pUserData          = &pc;                  // Can be accessed from the device object (device.pUserData).
                    deviceConfig.periodSizeInFrames = frameDataCbInterval;

                    devices = &pc.playbackDevices;
                    ppConfigDeviceId = &deviceConfig.playback.pDeviceID;
                } else {
                    deviceConfig = ma_device_config_init(ma_device_type_capture);

                    deviceConfig.capture.format     = sampleFormat;     // Set to ma_format_unknown to use the device's native format.
                    deviceConfig.capture.channels   = channels;         // Set to 0 to use the device's native channel count.
                    deviceConfig.sampleRate         = sampleRateHz;     // Set to 0 to use the device's native sample rate.
                    deviceConfig.dataCallback       = ah_capture_callback;  // This function will be called when miniaudio needs more data.
                    deviceConfig.notificationCallback = ah_device_callback; // This function will be called when device state changes.
                    deviceConfig.pUserData          = &pc;              // Can be accessed from the device object (device.pUserData).
                    deviceConfig.periodSizeInFrames = frameDataCbInterval;

                    devices = &pc.captureDevices;
                    ppConfigDeviceId = &deviceConfig.capture.pDeviceID;
                }

                // select the device
                {
                    std::lock_guard<std::mutex> lock(pc.device_mutex);
                    if (!devices->preferred.empty()) {
                        for (int i = 0; i < devices->list.size(); ++i) {
                            if (match(devices->list[i].name, devices->preferred)) {
                                devices->selectedId = devices->list[i].id;
                                devices->selectedName = devices->list[i].name;
                                *ppConfigDeviceId = &devices->selectedId;
                                break;
                            }
                        }
                    }
                    if (devices->defaultIdx > -1) // enumerated
                    {
                        if (!*ppConfigDeviceId) {
                            devices->selectedId = devices->list[devices->defaultIdx].id;
                            devices->selectedName = devices->list[devices->defaultIdx].name;
                        }
                        lastDeviceName = &devices->selectedName;
                    }
                }

                pc.device = ma_unique_device(new ma_device());
                if (!pc.device
                    || (result = ma_device_init(pc.context.get(), &deviceConfig, pc.device.get())) != MA_SUCCESS) {
                    pc.backendError = pc.device ? result : MA_OUT_OF_MEMORY;
                    pc.device = nullptr;
                    pc.log.LogMsg(LOG_ERR, "Failed to open%s device: %s", pc.device ? (pc.device->type == ma_device_type_playback ? " playback" : " capture") : "",
                        ma_result_description(pc.backendError));
                    pc.cmdQueue.set(CmdStop);
                    break;
                }
                if (pc.device->type == ma_device_type_playback)
                    ma_atomic_float_set(&pc.device->masterVolumeFactor, pc.playbackVolumeFactor);

                pc.log.LogMsg(LOG_INFO, "Opened %s device: %s",
                    pc.device->type == ma_device_type_playback ? "playback" : "capture",
                    devices->selectedName.c_str());
            }

            if (!ma_device_is_started(pc.device.get())) {
                result = ma_device_start(pc.device.get());
                if (result != MA_SUCCESS) {
                    pc.backendError = result;
                    pc.log.LogMsg(LOG_ERR, "Failed to start %s device: %s",
                        pc.device->type == ma_device_type_playback ? "playback" : "capture",
                        ma_result_description(result));
                    pc.cmdQueue.set(CmdStop);
                    break;
                }
            }
            pc.state &= ~StatePause;

            if (pc.notificationCbMask & EventResume)
                pc.notificationCbProc({EventResume, lastDeviceName->c_str(),
                (uint64_t)(pc.device ? (pc.device->type == ma_device_type_playback ? EventOpPlayback : EventOpCaptureRecord) : EventOpNone)},
                pc.notificationCbUserData);
            break;
        case CmdTogglePause:
            if (pc.state.canPause())
                pc.cmdQueue.internalCommand(CmdPause);
            else if (pc.state.canResume())
                pc.cmdQueue.internalCommand(CmdResume);
            else
                pc.stateError = ErrorInvalidSequence;
            break;
        case CmdEnumerateDevices:
            pc.log.LogMsg(LOG_INFO, "Enumerating devices...");
            {
                std::lock_guard<std::mutex> lock(pc.device_mutex);
                pc.playbackDevices.list.clear();
                pc.playbackDevices.defaultIdx = -1;
                pc.captureDevices.list.clear();
                pc.captureDevices.defaultIdx = -1;

                result = ma_context_enumerate_devices(pc.context.get(), ah_device_enum_callback, &pc);
            }
            if (result != MA_SUCCESS) {
                pc.backendError = result;
                pc.log.LogMsg(LOG_ERR, "Failed to enumerate devices: %s", ma_result_description(result));
            }
            break;
        case CmdSwitchPlaybackDevice:
        case CmdSwitchCaptureDevice:
            if (cc.fromuser) {
                std::lock_guard<std::mutex> lock(pc.device_mutex);
                if (cc.cmd == CmdSwitchPlaybackDevice)
                    pc.playbackDevices.preferred = cc.argStr;
                else
                    pc.captureDevices.preferred = cc.argStr;

                if (pc.playbackDevices.list.empty() && pc.captureDevices.list.empty()) {
                    // probably not enumerated yet
                    // reverse order
                    pc.cmdQueue.internalCommand(cc.cmd);
                    pc.cmdQueue.internalCommand(CmdEnumerateDevices);
                    break;
                }
            }
            // only restart when changing active device
            if (pc.device &&
                (pc.device->type == ma_device_type_playback ? cc.cmd == CmdSwitchPlaybackDevice : cc.cmd == CmdSwitchCaptureDevice)) {
                if (!pc.state.isPaused()) {
                    // reverse order
                    pc.cmdQueue.internalCommand(CmdResume);
                    pc.cmdQueue.internalCommand(cc.cmd); // will stop on pause error
                    pc.cmdQueue.internalCommand(CmdPause);
                    break;
                }
                // pause is active, safe to destroy the device
                pc.device = nullptr;
            }
            // just copy the preferred name to selected,
            // will be updated by the actual device name on device start
            if (cc.cmd == CmdSwitchPlaybackDevice)
                pc.playbackDevices.selectedName = pc.playbackDevices.preferred;
            else
                pc.captureDevices.selectedName = pc.captureDevices.preferred;
            break;
        case CmdSetPlaybackVolume:
            if (cc.argF32 < 0.0f || cc.argF32 > 1.0f) {
                pc.stateError = ErrorInvalidCmdArg;
                break;
            }
            pc.playbackVolumeFactor = cc.argF32;
            if (pc.device && pc.device->type == ma_device_type_playback)
                ma_atomic_float_set(&pc.device->masterVolumeFactor, pc.playbackVolumeFactor);
            break;
        case CmdSetPlaybackFileName:
            pc.playbackFileName = cc.argStr; pc.state.hasPlaybackFile = !cc.argStr.empty();
            break;
        case CmdStop:
        {
            NotificationEventOp op = pc.device ? (pc.device->type == ma_device_type_playback ? EventOpPlayback : EventOpCaptureRecord) : EventOpNone;
            if (pc.encoder && pc.updatePlaybackFileName && !pc.genericError) {
                pc.playbackFileName = pc.lastFileName;
                pc.state.hasPlaybackFile = true;
            }
            // reset state
            pc.state   = StateIdle;
            pc.device  = nullptr;
            pc.encoder = nullptr;
            pc.decoder = nullptr;
            pc.length  = 0;

            if (pc.notificationCbMask & EventStop)
                pc.notificationCbProc({EventStop, lastDeviceName->c_str(), (uint64_t)op}, pc.notificationCbUserData);
            break;
        }
        case CmdExit:
            pc.state &= ~StateReady;
            break;
        default:
            pc.log.LogMsg(LOG_DBG, "Invalid command %u", cc.cmd);
            break;
        }
        if (!pc.state.isReady())
            break;
    } while (true);

    std::lock_guard<std::timed_mutex> lock(pc.mutex);
    pc.device  = nullptr;
    pc.encoder = nullptr;
    pc.decoder = nullptr;
    pc.state   = StateExit;
    pc.length  = 0;
    pc.cmdQueue.clear();
}
