#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "Logger.hpp"

#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_NULL
#define MA_NO_OPUS

// FIXME define in Makefile
#ifdef _WIN32
#define MA_ENABLE_WASAPI
#else
#define MA_ENABLE_ALSA
#endif

#include "miniaudio.h"
#if defined(HAVE_OPUS)
#include "miniaudio_libopus.h"
#endif // defined(HAVE_OPUS)

class AudioHandler {
public:
    enum Format {
        // abstract ma_format
        FormatAny = ma_format_unknown,
        FormatS16 = ma_format_s16,
        FormatS32 = ma_format_s32,
        FormatF32 = ma_format_f32
    };

    enum StateBits {
        StateExit     = 0,
        StateReady    = 0x08,
        StateIdle     = StateReady,
        // ops has to be LSbs
        StatePlayback = 0x01 | StateReady,
        StateCapture  = 0x02 | StateReady,
        StateRecord   = 0x03 | StateReady,
        StateMask     = StateRecord,
        StateOpMask   = StateMask ^ StateReady,
        StatePause    = 0x10,
        StateSeek     = 0x20,
        StateEOF      = 0x40,
        StateFlagMask = StatePause | StateSeek | StateEOF
    };

    enum Command {
        CmdNone     = 0,
        CmdStop,         // Stop current operation and close device and file
        CmdPlay,         // Playback, with optional file argument
        CmdCapture,      // Capture
        CmdRecord,       // Record, file argument required
        CmdPause,        // Activate pause
        CmdResume,       // Cancel pause
        CmdTogglePause,  // Activate or cancel pause based on current state
        CmdSeek,         // Seek input file to position, target frame argument required
        CmdRewind,       // Seek input file to begining
        CmdEnumerateDevices, // Enumerate available audio devices
        CmdSwitchPlaybackDevice, // Switch output to another device
        CmdSwitchCaptureDevice,  // Switch input to another device
        CmdSetPlaybackVolume,    // set playback volume
        CmdSetPlaybackFileName,  // set file name for next playback command
        CmdExit          // Signal command thread to cleanup and exit
    };

    enum Error {
        ErrorBase            = 0x8000,

        ErrorInvalidSequence,
        ErrorInvalidCmdArg,

        ErrorLast
    };
    static constexpr const char *ErrorDescriptions[] = {
        "Invalid command at the current state",
        "Invalid command argument"
    };

    enum NotificationEvent {
        EventPlayFile   = 0x01,
        EventRecordFile = 0x02,
        EventSeek       = 0x04,
        EventPause      = 0x08,
        EventResume     = 0x10,
        EventStop       = 0x20
    };
    enum NotificationEventOp {
        EventOpNone,
        EventOpPlayback,
        EventOpCaptureRecord
    };
    struct Notification {
        NotificationEvent event;
        const char* dataStr;
        uint64_t    dataU64;
    };

    typedef void (*frameDataCb) (Format format, uint32_t channels, const void *pData, uint32_t frameCount, void *userData);
    typedef void (*notificationCb) (const Notification &notification, void *userData);

private:
    template <typename UT, UT fn_uninit>
    struct ma_uninit {
        template <typename T>
        constexpr void operator()(T *ptr) const noexcept {
            DBG("uninit %p\n", ptr);
            fn_uninit(ptr);
            delete ptr;
        }
    };
    template <typename ST, ST fn_stop, typename UT, UT fn_uninit>
    struct ma_stop_uninit {
        template <typename T>
        constexpr void operator()(T *ptr) const noexcept {
            ma_result ret = fn_stop(ptr);
            DBG("stop uninit %p %d\n", ptr, ret);
            fn_uninit(ptr);
            delete ptr;
        }
    };
    typedef std::unique_ptr<ma_context,
            ma_uninit<decltype(&ma_context_uninit), ma_context_uninit>> ma_unique_context;
    typedef std::unique_ptr<ma_encoder,
            ma_uninit<decltype(&ma_encoder_uninit), ma_encoder_uninit>> ma_unique_encoder;
    typedef std::unique_ptr<ma_decoder,
            ma_uninit<decltype(&ma_decoder_uninit), ma_decoder_uninit>> ma_unique_decoder;
    typedef std::unique_ptr<ma_device,
            ma_stop_uninit<decltype(&ma_device_stop), ma_device_stop, decltype(&ma_device_uninit), ma_device_uninit>> ma_unique_device;

    struct Cmd {
        Cmd(Command initial = CmdNone) : cmd(initial), argU64(0), argF32(0.0f), argStr(), fromuser(false) {}
        Cmd(Command initial, uint64_t arg)    : Cmd(initial) { argU64 = arg; }
        Cmd(Command initial, float arg)       : Cmd(initial) { argF32 = arg; }
        Cmd(Command initial, const char *arg) : Cmd(initial) { if (arg) argStr = arg; }
        Cmd(Command initial, std::string const &arg) : Cmd(initial) { argStr = arg; }
        Cmd &operator=(Command newCmd) { cmd = newCmd; argU64 = 0; argF32 = 0.0f; argStr.clear(); return *this; }

        Command cmd;
        ma_uint64 argU64;
        ma_float argF32;
        std::string argStr;
        bool fromuser;
    };
    struct CmdQueue {
        bool empty() {
            std::lock_guard<std::mutex> lock(mutex);
            return queue.empty();
        }
        void clear() {
            std::lock_guard<std::mutex> lock(mutex);
            queue.clear();
            cond.notify_one();
        }
        void set(const Cmd &cmd) {
            std::lock_guard<std::mutex> lock(mutex);
            queue.clear();
            queue.push_back(cmd);
            cond.notify_one();
        }
        void internalCommand(const Cmd &cmd) {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push_front(cmd);
            cond.notify_one();
        }
        void userCommand(const Cmd &cmd) {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push_back(cmd);
            queue.back().fromuser = true;
            cond.notify_one();
        }
        Cmd const pendingCommand() {
            std::unique_lock<std::mutex> lock(mutex);
            if (queue.empty())
                cond.wait(lock, [this]{ return !queue.empty(); } );

            Cmd cmd;
            cmd = queue.front();
            queue.pop_front();

            return cmd;
        }

        std::deque<Cmd> queue;
        std::mutex mutex;
        std::condition_variable_any cond;
    };

public:
    struct State {
        State() : value(StateIdle), hasPlaybackFile(false) {}
        State(unsigned int initial) : value(initial), hasPlaybackFile(false) {}

        // tests
        constexpr bool isReady()     { return  (value & StateReady); }
        constexpr bool isIdle()      { return  (value & StateMask) == StateIdle; }
        constexpr bool isActive()    { return  (value ^ StateMask)  < StateOpMask; }
        constexpr bool isPlaying()   { return  (value & StateMask) == StatePlayback; }
        constexpr bool isCapturing() { return  (value & StateMask) == StateCapture; }
        constexpr bool isRecording() { return  (value & StateMask) == StateRecord; }
        constexpr bool isCapOrRec()  { return  (value & StateCapture) == StateCapture; }
        constexpr bool isPaused()    { return   value & StatePause; }
        constexpr bool atEOF()       { return   value & StateEOF; }

        bool canPlay(bool withFile = false)
                                     { return  (value & (StateSeek|StateOpMask^StatePlayback)) == StateIdle && (isPlaying() || withFile || hasPlaybackFile); }
        constexpr bool canCapture()  { return  (value & StateMask) == StateIdle || (value & StateMask) == StateRecord; }
        constexpr bool canRecord()   { return  (value & (StateOpMask^StateCapture)) == StateIdle
                                            || (value & (StatePause|StateMask)) == (StateRecord|StatePause); }
        constexpr bool canPause()    { return  (value & (StateSeek|StatePause|StateOpMask^StateCapture)) == StatePlayback; }
        constexpr bool canResume()   { return  (value & (StateFlagMask|StateOpMask^StateCapture)) == (StatePlayback|StatePause); }
        constexpr bool canSeek()     { return  (value & (StateSeek|StateMask)) == StatePlayback; }

        // modifiers
        constexpr State &operator=(unsigned int newState) { value = newState; return *this; }
        constexpr State &operator|=(unsigned int set)     { value |= set; return *this; }
        constexpr State &operator&=(unsigned int reset)   { value &= reset; return *this; }

        // access
        constexpr const unsigned int operator()() const { return value; }
        constexpr operator StateBits() const { return (const StateBits)value; }

        unsigned int value;
        bool hasPlaybackFile;
    };

    struct Devices {
        struct Device {
            std::string name;
            ma_device_id id;
        };
        std::vector<Device> list;
        int defaultIdx;
        std::string preferred;
        std::string selectedName;
        ma_device_id selectedId;
    };

    struct privateContext {
        privateContext();
        ~privateContext();

        State state;
        union {
           ma_result backendError;
           Error     stateError;
           int       genericError;
        };

        CmdQueue cmdQueue;

        std::string playbackFileName;
        std::string lastFileName;
        bool updatePlaybackFileName;
        ma_uint64 length;
        Cmd playbackEOFcmd;
        float playbackVolumeFactor;

        ma_unique_context context;
        ma_unique_device device;
        ma_unique_encoder encoder;
        ma_unique_decoder decoder;
        ma_decoder_config decoderConfig;

        std::mutex device_mutex;
        Devices playbackDevices;
        Devices captureDevices;

        frameDataCb frameDataCbProc;
        void *frameDataCbUserData;

        notificationCb notificationCbProc;
        void *notificationCbUserData;
        unsigned notificationCbMask;

        logger::Logger log;

        std::timed_mutex mutex;
        std::condition_variable_any cond;
    };

public:
    AudioHandler(uint32_t _sampleRateHz = 44100, uint32_t _channels = 2, Format _sampleFormat = FormatF32, Format _recordFormat = FormatF32);
    // _frameDataCbInterval is the preferred interval, in frames, frame data callback would be called,
    // it is not guaranteed that this value would have effect, as it depends on OS audio subsystem
    AudioHandler(uint32_t _sampleRateHz, uint32_t _channels, Format _sampleFormat, Format _recordFormat, uint32_t _frameDataCbInterval);
    ~AudioHandler();

    // callbacks
    // Do not call from within the callback any AudioHandler functions that marked as can block, as this end up in a dead lock
    // attachFrameDataCb: add frame data callback of type
    // void frameDataCb(Format format, uint32_t channels, const void *pData, uint32_t frameCount, void *userData)
    // where
    //   format:     sample format
    //   channels:   number of channels
    //   pData:      frame array pointer
    //   frameCount: number of frames in the frame array
    //   userData:   userData pointer provided to attachFrameDataCb()
    // can block
    void attachFrameDataCb(frameDataCb cbProc, void *userData = nullptr);
    // removeFrameDataCb: remove frame data callback
    // can block
    void removeFrameDataCb();
    // attachNotificationCb: add (masked by NotificationEvent mask) state change callback, of type
    // void notificationCb(const Notification &notification, void *userData);
    // where
    //   notification: struct providing notification context
    //   userData:   userData pointer provided to attachNotificationCb()
    // data provided to the callback function:
    //   EventPlayFile:   dataStr - file name opened for reading
    //   EventRecordFile: dataStr - file name opened for writing
    //   EventSeek:       dataStr - currently playing file name, dataU64 - cursor position set
    //   EventPause:      dataStr - name of the paused device, or "default" if unknown/not enumerated, dataU64 - NotificationEventOp
    //   EventResume:     dataStr - name of the resumed device, or "default" if unknown/not enumerated, dataU64 - NotificationEventOp
    //   EventStop:       dataStr - name of the stopped device, or "default" if unknown/not enumerated, dataU64 - NotificationEventOp
    // can block
    void attachNotificationCb(unsigned mask, notificationCb cbProc, void *userData = nullptr);
    // removeFrameDataCb: remove frame data callback
    // can block
    void removeNotificationCb();

    // devices
    // enumerate: enumerate available audio devices
    void enumerate();
    // getPlaybackDevices: get playback devices with the lock aquired
    const Devices &getPlaybackDevices();
    // getCaptureDevices: get playback devices with the lock aquired
    const Devices &getCaptureDevices();
    // unlockDevices: unlock devices lock
    void unlockDevices();
    // setPreferredPlaybackDevice: set or reset preferred payback device
    void setPreferredPlaybackDevice(const char *deviceName = nullptr);
    // setPreferredCaptureDevice: set or reset preferred capture device
    void setPreferredCaptureDevice(const char *deviceName = nullptr);
    // setPlaybackVolumeFactor: set playback volume factor (0~1)
    void setPlaybackVolumeFactor(const float &volumeFactor = 1.0f);
    // getPlaybackVolumeFactor: get playback volume factor (0~1)
    // can block
    bool getPlaybackVolumeFactor(float &volumeFactor);

    // control
    // setPlaybackEOFaction: action to perform at end of input file;
    // only CmdStop, CmdPlay (preselected file), CmdPause, CmdRewind (loop) and CmdCapture are allowed,
    // default is CmdStop
    // return value indicates if the request was successful
    // can block
    bool setPlaybackEOFaction(Command cmd);
    // setPlaybackFileName: set or reset (nullptr/"") filename for next playback command
    void setPlaybackFileName(const char *fileName);
    // setUpdatePlaybackFileName: update PlaybackFileName after successful record
    void setUpdatePlaybackFileName(bool newval) { pc.updatePlaybackFileName = newval; } // safe to modify directly
    // stop: stop the current operation and reset state
    void stop();
    // play: start playing a specified file or file preselected earlier,
    // if same file name requested resume (and rewind, if needed) is performed
    // only available if not capturing/recording
    void play(const char *fileName = nullptr);
    // capture: start capturing samples from input device,
    // when invoked while recording, recording is stopped
    // only available if not playing
    void capture();
    // capture: start capturing samples from input device and writing to the specified file,
    // only available if not playing
    void record(const char *fileName);
    // seek: set the current playing file cursor to the specified position
    // only available while playing
    void seek(uint64_t posInPcmFrames);
    // rewind: set the current playing file cursor to the beginning
    // only available while playing
    void rewind();
    // pause: pause playback or recording without closing the file
    // only available while playing or recording
    void pause();
    // resume: resume current operation
    // only available while playing or recording
    void resume();
    // activate or cancel pause basing on its current state
    // only available while playing or recording
    void togglePause();

    // state
    // get the current state, optionaly with current file length and position
    // return value indicates if the request was successful
    // can block
    bool getState(State &state, uint64_t *lenInPcmFrames = nullptr, uint64_t *posInPcmFrames = nullptr);
    // get error, if any, with optional description
    // return value indicates if the request was successful
    // can block
    bool getError(int *error, const char **description);

    // utility
    // framesToTime: convert position in PCM frames to time string
    static std::string framesToTime(uint64_t frames, uint32_t sampleRateHz);
    std::string framesToTime(uint64_t frames) { return framesToTime(frames, sampleRateHz); }

    // access
    // log: get internal log object
    logger::Logger &log() { return pc.log; }

    const ma_uint32 sampleRateHz;
    const ma_uint32 channels;
    const ma_format sampleFormat;
    const ma_format recordFormat;
    const ma_uint32 frameDataCbInterval;

private:
    constexpr bool isOperational() { return !pc.state.isIdle() && pc.backendError == MA_SUCCESS; }

    void commandProc();

    privateContext pc;

    std::thread commandThread;
};
