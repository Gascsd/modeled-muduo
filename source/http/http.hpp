/**
 * 这里是一个http的协议支持，封装了应用层http协议需要做的事情
*/
#include "../server.hpp"

#include <sys/stat.h>

#include <fstream>
#include <regex>

// 状态码对应的状态信息
std::unordered_map<int, std::string> _statu_msg = {
    {100, "Continue"},
    {101, "Switching Protocol"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choice"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"},
    {599, "Network Connect Timeout Error"},
    {600, "Unparseable Response Headers"}};

// 文件类型对应的协议报头信息
std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi"},
    {".midi", "audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/x-rar-compressed"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}};

/**
 * 这是一个工具类，里面封装了一些小工具
 * 1. 字符串分割
 * 2. 读取文件内容
 * 3. 写入文件内容
 * 4. url编码
 * 5. url解码
 * 6. 获取状态码对应的信息
 * 7. 获取文件后缀对应的协议报头信息
*/
class Util
{
public:
    // 字符串分割函数,使用sep 作为分隔符，分割src字符串，将分割后的字符串放入result中
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> &result)
    {
        size_t offset = 0;
        while (offset < src.size())
        {
            size_t pos = src.find(sep, offset);
            if (pos == std::string::npos) // 有分隔符
            {
                if (pos == src.size())
                    break;
                result.push_back(src.substr(offset)); // 读取剩余内容
                return result.size();
            }
            if (pos == offset)
            {
                offset = pos + sep.size();
                continue;
            }
            result.push_back(src.substr(offset, pos - offset)); // 分割[pos,pos]
            offset = pos + sep.size();                          // 更新pos(起始位置)
        }
        return result.size();
    }
    // 读取文件的所有内容
    static bool ReadFile(const std::string &filepath, std::string &content) // 读取到字符串中
    {
        std::ifstream ifs(filepath, std::ios::binary);
        if (!ifs.is_open())
        {
            // 输出文件打开失败的原因
            LOG(ERROR, "Failed to open the file: %s, code:%d, reason:%s", filepath.c_str(), errno, strerror(errno));
            return false;
        }
        size_t fsize = 0;      // 文件大小
        ifs.seekg(0, ifs.end); // 将文件读写位置移动到文件末尾
        fsize = ifs.tellg();   // 此时文件大小就是文件读写位置的偏移量
        ifs.seekg(0, ifs.beg); // 将文件读写位置移动到文件开头
        content.resize(fsize);
        ifs.read(&content[0], fsize); // 读取文件内容
        if (ifs.good() == false)      // 文件读取失败
        {
            ifs.close();
            LOG(ERROR, "Failed to open the file: %s, code:%d, reason:%s", filepath.c_str(), errno, strerror(errno));
            return false;
        }
        ifs.close();
        return true;
    }
    static bool ReadFile(const std::string &filepath, Buffer &buffer) // 读取到Buffer中
    {
        std::string fileContent;
        if (!ReadFile(filepath, fileContent))
        {
            return false;
        }
        buffer.WriteAndPush(fileContent.c_str(), fileContent.size());
        return true;
    }
    // 向文件中写入内容
    static bool WriteFile(const std::string &filepath, const std::string &content)
    {
        std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            LOG(ERROR, "Failed to open the file: %s, code:%d, reason:%s", filepath.c_str(), errno, strerror(errno));
            return false;
        }
        ofs.write(content.c_str(), content.size());
        if (ofs.good() == false)
        {
            ofs.close();
            LOG(ERROR, "Failed to open the file: %s, code:%d, reason:%s", filepath.c_str(), errno, strerror(errno));
            return false;
        }
        ofs.close();
        return true;
    }
    // url编码
    static std::string UrlEncode(const std::string &url, bool convert_space_to_plus)
    {
        std::string result;
        for (auto &c : url)
        {
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
            {
                result += c;
            }
            else if (c == ' ' && convert_space_to_plus == true)
            {
                result += '+';
            }
            else
            {
                char tmp[4] = {0};
                snprintf(tmp, 4, "%%%02X", c);
                result += tmp;
            }
        }
        return result;
    }
    // url解码
    static char HextoI(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        else if (c >= 'a' && c <= 'z')
            return c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            return c - 'A' + 10;
        else
            return -1;
    }
    static std::string UrlDecode(const std::string &url, bool convert_space_to_plus)
    {
        std::string result;
        for (size_t i = 0; i < url.size(); ++i)
        {
            if (url[i] == '+' && convert_space_to_plus == true)
            {
                result += ' ';
            }
            else if (url[i] == '%')
            {
                if (i + 2 < url.size())
                {
                    char v1 = HextoI(url[i + 1]);
                    char v2 = HextoI(url[i + 2]);
                    char v = v1 * 16 + v2;
                    result += v;
                    i += 2;
                }
                else
                {
                    result += url[i];
                }
            }
            else
            {
                result += url[i];
            }
        }
        return result;
    }

    // 响应状态码描述解析
    static std::string GetStatusCodeDesc(int code)
    {
        auto it = _statu_msg.find(code);
        if (it == _statu_msg.end())
        {
            return "Unknown";
        }
        return it->second;
    }

    // 获取文件扩展名对应的mime
    static std::string GetFileMime(const std::string &filename)
    {
        // 找到最后一个.和之后的字符
        size_t pos = filename.rfind('.');
        if (pos == std::string::npos)
        {
            return "application/octet-stream";
        }
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream";
        }
        return it->second;
    }

    // 判断文件是否是目录
    static bool IsDirectory(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }
    // 判断文件是否是普通文件
    static bool IsRegular(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISREG(st.st_mode);
    }

    // 判断文件路径是否有效（只能在相对根目录下查找）
    static bool IsValidPath(const std::string &path)
    {
        // 按照/进行目录分割，计算目录深度，如果深度小于0就是有问题
        std::vector<std::string> subdirs;
        int depth = 0;
        Split(path, "/", subdirs);
        for (auto &str : subdirs)
        {
            if (str == "..")
            {
                --depth;
                if (depth < 0)
                    return false;
            }
            else
            {
                ++depth;
            }
        }
        return true;
    }
};

// 请求解析 GET /index.html?word=C++ HTTP/1.1
/**
 * HttpRequest，对http协议的请求报文进行解析
*/
class HttpRequest
{
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 请求路径
    std::string _version;                                  // 请求版本
    std::unordered_map<std::string, std::string> _headers; // 请求头KV结构
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
    std::string _body;                                     // 请求正文
    std::smatch _match;                                    // 正则化的资源路径
public:
    HttpRequest() : _version("HTTP/1.1") {}
    // 重置
    void Reset()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _headers.clear();
        _params.clear();
        _body.clear();
        std::smatch match;
        _match.swap(match);
    }
    // 插入头部字符串
    void SetHeader(const std::string &key, const std::string &value)
    {
        _headers[key] = value;
    }
    // 判断是否存在头部字符串
    bool HaveHeader(const std::string &key)
    {
        return _headers.find(key) != _headers.end();
    }
    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    // 插入查询字符串
    void SetParam(const std::string &key, const std::string &value)
    {
        _params[key] = value;
    }
    // 判断是否存在查询字符串
    bool HaveParam(const std::string &key)
    {
        return _params.find(key) != _params.end();
    }
    // 获取指定查询字符串
    std::string GetParam(const std::string &key)
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return "";
        }
        return it->second;
    }
    // 获取正文长度
    size_t GetBodyLength()
    {
        auto it = _headers.find("Content-Length");
        if (it == _headers.end())
        {
            return 0;
        }
        return std::stol(it->second);
    }
    // 判断是否是长连接
    bool KeepAlive()
    {
        auto it = _headers.find("Connection");
        if (it == _headers.end())
        {
            return false;
        }
        if (it->second == "keep-alive")
        {
            return true;
        }
        return false;
    }
};

/**
 * HttpResponse类，对http的相应进行解析
*/
class HttpResponse
{
public:
    int _status_code = 200;                                // 响应状态码
    std::string _status_msg;                               // 响应状态描述
    std::unordered_map<std::string, std::string> _headers; // 响应头KV结构,头部字段
    std::string _body;                                     // 响应正文
    bool _rediret_flag;                                    // 是否是重定向
    std::string _rediret_url;                              // 重定向url

public:
    HttpResponse(int status = 200, bool flag = false) : _rediret_flag(flag), _status_code(status) {}
    void ReSet()
    {
        _status_code = 200;
        _status_msg.clear();
        _headers.clear();
        _body.clear();
        _rediret_flag = false;
        _rediret_url.clear();
    }
    // 头部字段的增加查询获取
    void SetHeader(const std::string &key, const std::string &value)
    {
        _headers[key] = value;
    }
    // 判断是否存在头部字段
    bool HaveHeader(const std::string &key)
    {
        return _headers.find(key) != _headers.end();
    }
    // 获取头部字段
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    // 设置正文
    void SetContent(std::string &body, std::string type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    void SetRediret(std::string &url, int statu = 302) // 设置重定向
    {
        _rediret_flag = true;
        _rediret_url = url;
        _status_code = statu;
    }
    // 判断是否是长连接
    bool KeepAlive()
    {
        auto it = _headers.find("Connection");
        if (it == _headers.end())
        {
            return false;
        }
        // LOG(DEBUG, "[%s]", it->second.c_str());
        if (it->second == "keep-alive")
        {
            return true;
        }
        return false;
    }
};

enum HttpState
{
    RECV_HTTP_ERROR, // 接收出错
    RECV_HTTP_LINE,  // 接收请求行阶段
    RECV_HTTP_HEAD,  // 接收请求头阶段
    RECV_HTTP_BODY,  // 接收请求正文阶段
    RECV_HTTP_OVER   // 接收完成阶段
};
/**
 * HttpContext：http上下文，用于保存http请求和响应，接收请求并解析，构建对应的响应
*/
const static int MAX_LINE_SIZE = 8192;
class HttpContext
{
private:
    int _response_statu;   // 响应状态码
    HttpState _recv_state; // 当前接收状态
    HttpRequest _request;  // 已经解析得到的请求
private:
    bool ParseRequestLine(const std::string &line) // 解析请求行
    {
        if (_recv_state != RECV_HTTP_LINE)
            return false;
        std::smatch matches;
        std::regex reg("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase); // 正则表达式解析首行
        bool ret = std::regex_match(line, matches, reg);
        if (ret == false)
        {
            _recv_state = RECV_HTTP_ERROR;
            _response_statu = 400; // BAD REQUEST
            return false;
        }
        _request._method = matches[1]; // 请求方法
        // 将请求方法转换为全大写
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper); // 这里的::表示的是toupper是一个全局接口
        _request._path = Util::UrlDecode(matches[2], false);                                                   // 解析url,不需要'+'->' '
        _request._version = matches[4];
        std::string _request_query = matches[3]; // 表单项
        std::vector<std::string> _request_query_array;
        Util::Split(_request_query, "&", _request_query_array);
        for (auto &str : _request_query_array)
        {
            // str是每一个表单项，用=分割
            ssize_t pos = str.find("=");
            if (pos == std::string::npos)
            {
                _recv_state = RECV_HTTP_ERROR;
                _response_statu = 400; // BAD REQUEST
                return false;
            }
            std::string key = str.substr(0, pos);
            std::string val = str.substr(pos + 1);
            _request.SetParam(key, val);
        }
        _recv_state = RECV_HTTP_HEAD;
        return true;
    }
    bool RecvRequestLine(Buffer *buffer) // 接收请求行
    {
        if (_recv_state != RECV_HTTP_LINE)
        {
            return false;
        }
        // 1. 获取一行数据（需要考虑里面数据不够一行/一行内容太大）
        std::string line = buffer->GetLineAndPop();
        if (line.empty()) // 里面数据不够一行
        {
            if (buffer->ReadableSize() > MAX_LINE_SIZE)
            {
                _recv_state = RECV_HTTP_ERROR;
                _response_statu = 414; // URI TOO LOG
                return false;
            }
            // 缓冲区不足一行，但是也挺少，继续接收
            return true;
        }
        if (line.size() > MAX_LINE_SIZE)
        {
            _recv_state = RECV_HTTP_ERROR;
            _response_statu = 414; // URI TOO LOG
            return false;
        }
        int ret = ParseRequestLine(line);
        if (ret == false)
        {
            return false;
        }
        // buffer->MoveReadOffset(line.size() + 2); // 移动读偏移，这里+2是因为\r\n两个字符
    }

    bool RecvRequestHead(Buffer *buffer) // 接收请求头
    {
        if (_recv_state != RECV_HTTP_HEAD)
            return false;
        while (true)
        {
            // 1. 获取一行数据（需要考虑里面数据不够一行/一行内容太大）
            std::string line = buffer->GetLineAndPop();
            if (line.empty()) // 里面数据不够一行
            {
                if (buffer->ReadableSize() > MAX_LINE_SIZE)
                {
                    _recv_state = RECV_HTTP_ERROR;
                    _response_statu = 414; // URI TOO LOG
                    return false;
                }
                // 缓冲区不足一行，但是也挺少，继续接收
                return true;
            }
            if (line.size() > MAX_LINE_SIZE)
            {
                _recv_state = RECV_HTTP_ERROR;
                _response_statu = 414; // URI TOO LOG
                return false;
            }
            if (line == "\r\n" || line == "\n") // 读到空行，表示头部结束
            {
                // buffer->MoveReadOffset(2); // 移动读偏移
                break;
            }
            bool ret = ParseRequestHead(line);
            if (ret == false)
            {
                return false;
            }
            // buffer->MoveReadOffset(line.size() + 2); // 移动读偏移，这里+2是因为\r\n两个字符
        }
        _recv_state = RECV_HTTP_BODY;
        return true;
    }
    bool ParseRequestHead(std::string &line) // 解析请求头
    {
        if (line.back() == '\n')
            line.pop_back();
        if (line.back() == '\r')
            line.pop_back();
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            _recv_state = RECV_HTTP_ERROR;
            _response_statu = 400; // BAD REQUEST
            return false;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);
        _request.SetHeader(key, val);
        return true;
    }
    bool RecvRequestBody(Buffer *buffer) // 接收请求正文
    {
        if (_recv_state != RECV_HTTP_BODY)
            return false;
        size_t content_length = _request.GetBodyLength();
        if (content_length == 0)
        {
            // 没有正文,表示请求结束
            _recv_state = RECV_HTTP_OVER;
            return true;
        }
        size_t real_length = content_length - _request._body.size(); // 实际需要接收的长度
        if (buffer->ReadableSize() >= real_length)
        {
            // 缓冲区里面有正文,并且够一整条正文
            _request._body.append(buffer->ReadPosition(), real_length);
            buffer->MoveReadOffset(real_length);
            _recv_state = RECV_HTTP_OVER;
            return true;
        }
        else
        {
            // 缓冲区里面有正文，但是不够一条正文
            _request._body.append(buffer->ReadPosition(), buffer->ReadableSize());
            buffer->MoveReadOffset(buffer->ReadableSize());
            return true;
        }
    }

public:
    HttpContext() : _response_statu(200), _recv_state(RECV_HTTP_LINE) {}
    // 获取相应状态码
    int ResponseStatu() { return _response_statu; }
    // 重置上下文
    void Reset()
    {
        _request.Reset();
        _response_statu = 200;
        _recv_state = RECV_HTTP_LINE;
    }
    // 获取接收状态
    HttpState GetState() { return _recv_state; }
    HttpRequest &Request() { return _request; }
    // 接收并解析Http请求
    void RecvHttpRequest(Buffer *buffer)
    {
        switch (_recv_state)
        {
        // 这里不带上break是因为处理完上一次的内容后要立即处理下一部分内容
        case RECV_HTTP_LINE:
            RecvRequestLine(buffer);
        case RECV_HTTP_HEAD:
            RecvRequestHead(buffer);
        case RECV_HTTP_BODY:
            RecvRequestBody(buffer);
        }
    }
};

const static int DEFAULT_TIMEOUT = 10; // HTTP默认请求超时时间
/**
 * HttpServer：封装上面的接口，能够提供一个快速构建http服务器的组件
*/
class HttpServer
{
    using Handler = std::function<void(const HttpRequest &, HttpResponse &)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;

private:
    TcpServer _server;      // TcpServer对象
    std::string _base_path; // web根目录
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;

private:
    // 组织http协议响应并发送
    void WriteResponse(const PtrConnection &conn, HttpRequest &req, HttpResponse &rsp)
    {
        // 1. 完善头部字段
        if (req.KeepAlive() == false) // 设置Connection状态
            rsp.SetHeader("Connection", "close");
        else
            rsp.SetHeader("Connection", "keep-alive");
        if (rsp._body.empty() == false && rsp.HaveHeader("Content-Length") == false)
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size())); // 正文存在，设置Content-Length
        if (rsp._body.empty() == false && rsp.HaveHeader("Content-Type") == false)
            rsp.SetHeader("Content-Type", "application/octet-stream"); // 设置Content-Type
        if (rsp._rediret_flag == true)
            rsp.SetHeader("Location", rsp._rediret_url); // 设置转发
        // 2. 将rsp中的要素，按照http协议格式组织
        std::stringstream rsp_str;
        rsp_str << req._version << " " << std::to_string(rsp._status_code) << " " << Util::GetStatusCodeDesc(rsp._status_code) << "\r\n";
        for (auto &h : rsp._headers)
        {
            rsp_str << h.first << ": " << h.second << "\r\n";
        }
        rsp_str << "\r\n";
        rsp_str << rsp._body;
        // 3. 发送数据
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }
    // 判断请求是否是静态资源请求
    bool IsFileHandler(const HttpRequest &req)
    {
        // 1. 必须设置了静态资源请求的根目录
        if (_base_path.empty())
            return false;
        // 2. 请求方法必须是GET/ HEAD方法
        if (req._method != "GET" && req._method != "HEAD")
            return false;
        // 3. 判断必须是合法路径
        if (Util::IsValidPath(req._path) == false)
            return false;
        // 4. 请求的资源必须存在（如果不存在调用错误处理，返回404）
        //      特殊情况，请求的是目录，就访问/index.html
        std::string req_path = _base_path + req._path; // 为了避免直接修改请求的资源路径，这里定义临时对象
        if (req._path.back() == '/')
        {
            req_path += "index.html";
        }
        if (Util::IsRegular(req_path) == false)
            return false;
        return true;
    }
    // 静态资源请求的处理
    void FileHandler(const HttpRequest &req, HttpResponse &rsp)
    {
        std::string req_path = _base_path + req._path; // 为了避免直接修改请求的资源路径，这里定义临时对象
        if (req._path.back() == '/')
        {
            req_path += "index.html";
        }
        bool ret = Util::ReadFile(req_path, rsp._body);
        if (ret == false)
            return;
        std::string mime = Util::GetFileMime(req_path);
        rsp.SetHeader("Content-Type", mime);
    }
    // 功能性请求的分类处理
    void Dispatcher(HttpRequest &req, HttpResponse &rsp, Handlers &handlers)
    {
        // 在对应的请求方法中查找对应资源的请求处理函数，如果找到就调用，否则就返回404
        // 实现思路：路由表中存放的就是键值对 正则表达式-处理函数
        // 使用正则表达式对请求的资源路径进行匹配，如果成功，就调用对应的函数处理，
        for (auto &handler : handlers)
        {
            const std::regex &reg = handler.first;
            const Handler &functor = handler.second;
            bool ret = std::regex_match(req._path, req._match, reg);
            if (ret == false)
                continue;
            return functor(req, rsp); // 调用函数处理请求
        }
        rsp._status_code = 404; // not found
    }
    // 请求的路由
    void Route(HttpRequest &req, HttpResponse &rsp)
    {
        // 1. 确定请求的类型,是静态请求还是功能性请求
        //      静态资源请求就调用FileHandler处理
        //      功能性请求就调用Dispatcher分类处理
        //      如果都不是就出错，返回错误处理（404）
        if (IsFileHandler(req))
        {
            // 是静态资源请求
            return FileHandler(req, rsp);
        }
        // 如果能走到这里，表示可能是功能性请求
        if (req._method == "GET" || req._method == "HEAD")
            return Dispatcher(req, rsp, _get_route);
        else if (req._method == "POST")
            return Dispatcher(req, rsp, _post_route);
        else if (req._method == "PUT")
            return Dispatcher(req, rsp, _put_route);
        else if (req._method == "DELETE")
            return Dispatcher(req, rsp, _delete_route);
        rsp._status_code = 405; // 请求方法不支持
    }
    // 获取上下文
    void OnConnection(const PtrConnection &conn)
    {
        conn->SetContext(HttpContext());
        LOG(DEBUG, "new connection %p", conn.get());
    }
    // 错误处理
    void ErrorHandle(const HttpRequest &req, HttpResponse &rsp)
    {
        // 1. 组织一个错误展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1 style='color:red'>";
        body += std::to_string(rsp._status_code);
        body += " ";
        body += Util::GetStatusCodeDesc(rsp._status_code);
        body += "</h1></body></html>";
        // 2. 将页面响应信息作为正文放入rsp
        rsp.SetContent(body, "text/html");
    }
    // 缓冲区数据解析+处理
    void OnMessage(const PtrConnection &conn, Buffer *buffer)
    {
        while (buffer->ReadableSize() > 0)
        {
            // 1. 获取上下文
            HttpContext *context = conn->GetContext()->get<HttpContext>();
            // 2. 通过上下文对缓冲区数据进行解析，得到HttpRequest对象
            //      1. 解析失败就进行出错响应
            //      2. 解析成功就进行路由处理
            context->RecvHttpRequest(buffer);
            HttpRequest &request = context->Request();
            HttpResponse response(context->ResponseStatu());
            if (context->ResponseStatu() >= 400)
            {
                // 进行错误响应，关闭连接
                ErrorHandle(request, response);         // 错误响应处理
                WriteResponse(conn, request, response); // 发送错误响应
                context->Reset();                       // 重置上下文
                buffer->MoveReadOffset(buffer->ReadableSize()); // 出错了就直接清空缓冲区
                conn->Shutdown();                       // 关闭连接
                return;
            }
            if (context->GetState() != RECV_HTTP_OVER)
            {
                // 如果解析不完整，就继续等待
                return;
            }
            // 3. 请求路由 + 业务处理
            Route(request, response);
            // 4. 组织response并发送
            WriteResponse(conn, request, response);
            // 5. 重置上下文
            context->Reset();
            // 6. 通过长短连接判断是否要关闭
            if (response.KeepAlive() == false)
                conn->Shutdown(); // 如果是短连接，就直接关闭
        }
    }

public:
    HttpServer(uint16_t port, int timeout = DEFAULT_TIMEOUT) : _server(port)
    {
        _server.EnableInactiveRelease(timeout);
        _server.SetConnectedCallback(std::bind(&HttpServer::OnConnection, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void SetBasePath(const std::string &path)
    {
        assert(Util::IsDirectory(path) == true);
        _base_path = path;
    }
    // 设置/添加 请求（请求的正则表达式） 与处理映射的关系
    void Get(const std::string &pattern, const Handler handler)
    {
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Post(const std::string &pattern, const Handler handler)
    {
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Put(const std::string &pattern, const Handler handler)
    {
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Delete(const std::string &pattern, const Handler handler)
    {
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void SetThreadNum(int num)
    {
        _server.SetThreadNum(num);
    }
    void Listen()
    {
        _server.Start();
    }
};
