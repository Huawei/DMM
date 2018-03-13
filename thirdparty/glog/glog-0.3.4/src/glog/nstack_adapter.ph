#ifndef __NSTACK_ADAPTER_H__
#define __NSTACK_ADAPTER_H__

#include <ostream>
#include <sstream>
#include <string>
#include "glog/nstack_glog.ph"

namespace nstack_space {

// LogMessageAdapter::LogStream is a std::ostream backed by this streambuf.
// This class ignores overflow and leaves two bytes at the end of the
// buffer to allow for a '\n' and '\0'.
class LogStreamBuf : public std::streambuf {
public:
    // REQUIREMENTS: "len" must be >= 2 to account for the '\n' and '\n'.
    LogStreamBuf(char *buf, int len) {
        setp(buf, buf + len - 2);
    }

    // This effectively ignores overflow.
    virtual int_type overflow(int_type ch) {
        return ch;
    }

    // Legacy public ostrstream method.
    size_t pcount() const { return pptr() - pbase(); }
    char* pbase() const { return std::streambuf::pbase(); }
};

class  LogMessageAdapter {
public:
    class LogRamMgnt {
    public:
        LogRamMgnt() {};
        ~LogRamMgnt() {};

        void *operator new(std::size_t size);
        void operator delete(void *ptr);
    };

    class LogStream : public std::ostream, public LogRamMgnt {
    public:
        LogStream(char *buf, int len, int log_ctr)
            : std::ostream(NULL),
            streambuf_(buf, len),
            ctr_(log_ctr),
            self_(this) {
            rdbuf(&streambuf_);
        }

        int ctr() const { return ctr_; }
        void set_ctr(int s_ctr) { ctr_ = s_ctr; }
        LogStream* self() const { return self_; }

        // Legacy std::streambuf methods.
        size_t pcount() const { return streambuf_.pcount(); }
        char* pbase() const { return streambuf_.pbase(); }
        char* str() const { return pbase(); }


        private:
            LogStreamBuf streambuf_;
            int ctr_;  // Counter hack (for the LOG_EVERY_X() macro)
            LogStream *self_;  // Consistency check hack
    };


    LogMessageAdapter(const char *file,
                              int         line,
                              const char *funcName,
                              int         logLevel);


    ~LogMessageAdapter();

    std::ostream& stream();

    class LogMsgAdpData : public LogRamMgnt {
    public:
        LogMsgAdpData(){};

        char *pMsgBuff;
        int  level;
        char fileName[MAX_ROSA_LOG_LEN];
        int  line;
        char func[MAX_ROSA_LOG_LEN];

        LogStream* stream_alloc_;
        LogStream* stream_;

        ~LogMsgAdpData();
    };

private:
    LogMsgAdpData* allocated;
    LogMsgAdpData* data;

};

}

#endif
