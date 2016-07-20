#ifndef http_sink_hpp_included_
#define http_sink_hpp_included_

#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <exception>

#include "./error.hpp"

namespace http {

/** Sink for sending/receiving data to/from the client.
 */
class SinkBase {
public:
    typedef std::shared_ptr<SinkBase> pointer;

    SinkBase() {}
    virtual ~SinkBase() {}

    struct FileInfo {
        /** File content type.
         */
        std::string contentType;

        /** Timestamp of last modification. -1 means now.
         */
        std::time_t lastModified;

        /** Timestamp of expiration, -1 means never
         */
        std::time_t expires;

        FileInfo(const std::string &contentType = "application/octet-stream"
                 , std::time_t lastModified = -1
                 , std::time_t expires = -1)
            : contentType(contentType), lastModified(lastModified)
            , expires(expires)
        {}
    };

    class DataSource {
    public:
        typedef std::shared_ptr<DataSource> pointer;

        DataSource(bool hasContentLength = true)
            : hasContentLength_(hasContentLength)
        {}

        virtual ~DataSource() {}

        virtual FileInfo stat() const = 0;

        virtual std::size_t read(char *buf, std::size_t size
                                 , std::size_t off) = 0;

        virtual std::string name() const { return "unknown"; }

        virtual void close() const {}

        /** Returns size of response.
         *  >=0: exact length, use content-length
         *  <0: unknown, use content-transfer-encoding chunked
         */
        virtual long size() const = 0;

        bool hasContentLength() const { return hasContentLength_; }

    private:
        bool hasContentLength_;
    };

    /** Sends content to client.
     * \param data data top send
     * \param stat file info (size is ignored)
     */
    void content(const std::string &data, const FileInfo &stat);

    /** Sends content to client.
     * \param data data top send
     * \param stat file info (size is ignored)
     */
    template <typename T>
    void content(const std::vector<T> &data, const FileInfo &stat);

    /** Sends content to client.
     * \param data data top send
     * \param size size of data
     * \param stat file info (size is ignored)
     * \param needCopy data are copied if set to true
     */
    void content(const void *data, std::size_t size
                 , const FileInfo &stat, bool needCopy);

    /** Sends current exception to the client.
     */
    void error();

    /** Sends given error to the client.
     */
    void error(const std::exception_ptr &exc);

    /** Tell client to look somewhere else.
     */
    void seeOther(const std::string &url);

    /** Sends given error to the client.
     */
    template <typename T> void error(const T &exc);

private:
    virtual void content_impl(const void *data, std::size_t size
                              , const FileInfo &stat, bool needCopy) = 0;
    virtual void error_impl(const std::exception_ptr &exc) = 0;
    virtual void seeOther_impl(const std::string &url) = 0;
};

/** Sink for sending/receiving data to/from the client.
 */
class ServerSink : public SinkBase {
public:
    typedef std::shared_ptr<ServerSink> pointer;
    typedef std::function<void()> AbortedCallback;

    struct ListingItem {
        enum class Type { dir, file };

        std::string name;
        Type type;

        ListingItem(const std::string &name = "", Type type = Type::file)
            : name(name), type(type)
        {}

        bool operator<(const ListingItem &o) const;
    };

    typedef std::vector<ListingItem> Listing;

    ServerSink() {}

    /** Generates listing.
     */
    void listing(const Listing &list);

    /** Checks wheter client aborted request.
     *  Throws RequestAborted exception when true.
     */
    void checkAborted() const;

    /** Sets aborted callback.
     */
    void setAborter(const AbortedCallback &ac);

    /** Pull in base class content(...) functions
     */
    using SinkBase::content;

    /** Sends content to client.
     * \param stream stream to send
     */
    void content(const DataSource::pointer &source);

private:
    virtual void content_impl(const DataSource::pointer &source) = 0;
    virtual void listing_impl(const Listing &list) = 0;
    virtual bool checkAborted_impl() const = 0;
    virtual void setAborter_impl(const AbortedCallback &ac) = 0;
};

class ClientSink : public SinkBase {
public:
    typedef std::shared_ptr<ClientSink> pointer;

    ClientSink() {}

    /** Content has not been modified. Called only when asked.
     *  Default implementation calls error(NotModified());
     */
    void notModified();

private:
    virtual void notModified_impl();
};

// inlines

inline void SinkBase::content(const std::string &data, const FileInfo &stat)
{
    content_impl(data.data(), data.size(), stat, true);
}

inline void SinkBase::content(const void *data, std::size_t size
                              , const FileInfo &stat, bool needCopy)
{
    content_impl(data, size, stat, needCopy);
}

template <typename T>
inline void SinkBase::content(const std::vector<T> &data, const FileInfo &stat)
{
    content_impl(data.data(), data.size() * sizeof(T), stat, true);
}

inline void ServerSink::content(const DataSource::pointer &source)
{
    content_impl(source);
}

inline void SinkBase::seeOther(const std::string &url)
{
    seeOther_impl(url);
}

inline void SinkBase::error(const std::exception_ptr &exc)
{
    error_impl(exc);
}

template <typename T> inline void SinkBase::error(const T &exc) {
    error_impl(std::make_exception_ptr(exc));
}

inline void ServerSink::listing(const Listing &list) { listing_impl(list); }

inline void ServerSink::setAborter(const AbortedCallback &ac) {
    setAborter_impl(ac);
}

inline bool ServerSink::ListingItem::operator<(const ListingItem &o) const
{
    if (type < o.type) { return true; }
    if (o.type < type) { return false; }
    return name < o.name;
}

inline void ClientSink::notModified() { notModified_impl(); }

inline void ClientSink::notModified_impl() {
    error(NotModified("Not Modified"));
}

} // namespace http

#endif // http_sink_hpp_included_
