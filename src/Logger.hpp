#pragma once

#include <cstring>
#include <memory>
#include <forward_list>
#include <mutex>
#include <ctime>

namespace logger {

enum LOG_LVL {
    LOG_ERR = 0,
    LOG_WARN,
    LOG_INFO,
    LOG_DBG,
    LOG_MAXLVL = LOG_DBG
};

class Logger {
public:

    typedef struct {
        unsigned long long N;
        time_t Ts;
        LOG_LVL Lvl;
        std::unique_ptr<const char[]> Msg;
    } Entry;

    typedef void (*MsgCB)(void *param);

    Logger(LOG_LVL Lvl = LOG_INFO, size_t MaxSize = 200) :
        mSize(0),
        mMaxSize(MaxSize),
        mLastN(0),
        mLvl(Lvl),
        mCB(nullptr),
        mCBparam(nullptr)
    {
    }
    const std::forward_list<Entry>& LockEntries() { mMutex.lock(); return mEntries; }
    void UnlockEntries() { mMutex.unlock(); }

    void LogMsg(LOG_LVL lvl, const char *msg) {
        if (lvl > mLvl)
            return;
        int len = (int)strlen(msg);
        if (len == 0)
            return;
        std::unique_ptr<char[]> buf(new char[len + 1]);
        memcpy(buf.get(), msg, len);
        // terminate and get rid of trailing new line
        buf[len] = '\0';
        while(--len >= 0 && (buf[len] == '\r' || buf[len] == '\n'))
            buf[len] = '\0';
        mMutex.lock();
        mEntries.emplace_front(Entry({++mLastN, time(nullptr), lvl, std::move(buf)}));
        mSize++;
        if (mSize > mMaxSize)
            mEntries.resize(mSize = mMaxSize);
        mMutex.unlock();
        if (mCB)
            mCB(mCBparam);
    }

    template<typename... Args>
    void LogMsg(LOG_LVL lvl, const char *fmt, Args&&... args) {
        if (lvl > mLvl)
            return;
        int len = snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
        if (len <= 0)
            return;
        std::unique_ptr<char[]> buf(new char[len + 1]);
        snprintf(buf.get(), len + 1, fmt, std::forward<Args>(args)...);
        // get rid of trailing new line
        while(--len >= 0 && (buf[len] == '\r' || buf[len] == '\n'))
            buf[len] = '\0';
        mMutex.lock();
        mEntries.emplace_front(Entry({++mLastN, time(nullptr), lvl, std::move(buf)}));
        mSize++;
        if (mSize > mMaxSize)
            mEntries.resize(mSize = mMaxSize);
        mMutex.unlock();
        if (mCB)
            mCB(mCBparam);
    }
    void Clear() {
        mMutex.lock();
        mEntries.clear();
        mSize = 0;
        mLastN = 0;
        mMutex.unlock();
        if (mCB)
            mCB(mCBparam);
    }
    void Trim(size_t newsize) {
        if (newsize >= mSize)
            return;
        mMutex.lock();
        mEntries.resize(newsize);
        mSize = newsize;
        mMutex.unlock();
        if (mCB)
            mCB(mCBparam);
    }
    size_t Size() { return mSize; }
    unsigned long long LastN() { return mLastN; }
    void SetLevel(LOG_LVL lvl) { mLvl = lvl; }
    LOG_LVL GetLevel() { return mLvl; }
    void SetMsgCB(MsgCB cb, void *param = nullptr) { mCB = cb; mCBparam = param; };
    static constexpr const char* Lvl2Str(LOG_LVL lvl) { return (size_t)lvl >= sizeof(L2S)/sizeof(*L2S)
                                                               || (int)lvl < 0 ? "" : L2S[lvl]; }

private:
    std::mutex mMutex;
    std::forward_list<Entry> mEntries;
    size_t mSize;
    const size_t mMaxSize;
    unsigned long long mLastN;
    LOG_LVL mLvl;
    MsgCB mCB;
    void* mCBparam;
    static constexpr const char* L2S[] = { "ERR", "WARN", "INFO", "DBG" };
};

} // namespace logger
