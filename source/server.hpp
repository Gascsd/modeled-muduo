/**
 * 项目名称:仿muduo库的One Thread One Loop高性能服务器组件
 * 项目思想:本项目是为了在网络通信中提高IO效率，采用了epoll模型和Reactor设计模式。
 *        设计了主从线程模型，主线程负责监听新连接，新连接交给从线程处理。
 *        每个线程都具有一个单独的Loop，用于处理本线程的任务，包括读写事件，定时器事件等。
 * 项目成果:完成了一个高性能服务器组件的组件，实现了主从线程模型，epoll模型，Reactor设计模式
 *         能够让使用者快速搭建出一个网络服务器。同时本项目后期可以提供对各种应用层协议的支持
 *         能够让使用者快速搭建出一个对应协议的服务器。
 *         目前已经提供了http协议的支持
 * 项目亮点:1. 实现了主从线程模型，epoll模型，Reactor设计模式
 *         2. 利用继承和多态实现了一个类，实例化的对象能够接收不同类型的数据（Any类的设计思想）
 *         3. 使用了时间轮技术,利用智能指针和对象销毁自动执行析构函数的特性执行定时任务，同时能够完成对定时任务的取消
 * 开发环境:Linux-CentOS,gcc-g++,gdb,makefile,Vim,Vs code,WebBench
 * 项目技术:epoll模型,Reactor设计模式,主从线程模型,线程池模型,http协议,Socket编程,C++11,智能指针,WebBench性能测试工具
*/


#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cassert>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <utility>
#include <typeinfo>

/**
 * 日志宏
 */
#define NORMAL (1 << 0)
#define DEBUG (1 << 1)
#define ERROR (1 << 2)
#define FATAL (1 << 3)

#define LOG_LEVEL ERROR

#define LOG(level, format, ...)                                                             \
    do                                                                                      \
    {                                                                                       \
        if (level < LOG_LEVEL)                                                              \
            break;                                                                          \
        time_t t = time(NULL);                                                              \
        struct tm *ltm = localtime(&t);                                                     \
        char tmp[32] = {0};                                                                 \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                 \
        fprintf(stdout, "[%p %s %s:%d] " format "\n", pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define MAX_EPOLLEVENTS 1024 // 最大事件通知个数
typedef enum
{
    DISCONNECTED, // 连接关闭状态
    CONNECTING,   // 连接建立成功，等待处理状态
    CONNECTED,    // 连接建立完成，各种设置已经完成，可以通信状态
    DISCONNECTING // 待关闭状态
} ConnStatu;      // 服务器状态类型
class Poller;
class EventLoop;
class Connection;
using PtrConnection = std::shared_ptr<Connection>; // Connection的智能指针类型

/**
 * any类：想要实现一个类，能够存放任意类型的数据，同时也能够进行任意类型的赋值
 *      example：
 *      {
 *          Any a;
 *          a = 10;
 *          int *pa = a.get<int>();
 *          a = std::string("hello world");
 *          std::string *ps = a.get<std::string>();
 *      }
 * any类的设计思想：主要思想是利用继承和多态的机制，在any类中保存父类指针，父类和子类可以无痛转换，赋值的时候销毁之前的对象，然后保存当前对象的父类指针
 * 在any类中创建一个基类holder和一个派生类placeholder，
 * 基类holder声明为抽象类，把对应的clone和type函数设置为纯虚函数，以便确认后面在继承的时候一定要重写该函数
 * 在派生类中保存对应的对象，在any类中保存基类的指针，就可以通过这个指针找到基类对象，转化为派生类对象做相关操作
 */
class Any
{
public: // 暴露给外界的成员函数
    Any() : _content(nullptr) {} // 默认构造

    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {} // 通过对应类型构造

    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {} // 拷贝构造

    ~Any() { delete _content; } // 析构函数

    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }
    template <class T>
    Any &operator=(const T &val) // 运算符重载，通过T类型给Any赋值
    {
        Any(val).swap(*this);
        return *this;
    }
    Any &operator=(Any other) // 运算符重载，通过Any类型给Any赋值
    {
        swap(other);
        return *this;
    }
    template <class T>
    T *get() // 获取当前容器保存的对象指针
    {
        if (typeid(T) != _content->type())
            return nullptr;
        return &dynamic_cast<placeholder<T> *>(_content)->_val;
    }

private: // 内部类
    class holder
    {
    public:
        virtual ~holder() {}
        virtual const std::type_info &type() = 0;
        virtual holder *clone() = 0;
    };
    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T &val) : _val(val) {}
        ~placeholder() {}
        virtual const std::type_info &type() { return typeid(T); }
        virtual holder *clone() { return new placeholder(_val); }

    public:
        T _val;
    };

private: // 类内成员变量
    holder *_content;
};

/**
 * Buffer类是用来管理缓冲区的，内部使用一个vector容器来存放内容
 * 一直向后填充，当前面的空闲空间+后面的空闲空间足够下次填充的时候就移动元素到最前端
 *
 * 优化方向：使用环形队列的方式优化，可以避免出现移动元素的情况
 */
const static int DefaultBufferSize = 1024;
class Buffer
{
private:
    std::vector<char> _buffer;
    uint64_t _read_idx;
    uint64_t _write_idx;

public:
    Buffer() : _read_idx(0), _write_idx(0), _buffer(DefaultBufferSize) {}
    ~Buffer() {}
    // 获取_buffer的起始地址
    char *Begin() { return &*_buffer.begin(); }
    // const char *Begin() const { return &*_buffer.begin(); }
    // 获取当前读取空间地址
    // char *ReadPosition() const { return Begin() + _read_idx; }
    char *ReadPosition() { return Begin() + _read_idx; }
    // 获取当前写入空间地址
    char *WritePosition() { return Begin() + _write_idx; }
    // 获取前沿空闲空间大小
    uint64_t HeadSize() { return _read_idx; }
    // 获取后续空闲空间大小
    uint64_t TailSize() { return _buffer.size() - _write_idx; }
    // 获取可读数据大小
    uint64_t ReadableSize() const { return _write_idx - _read_idx; }
    // 读偏移向后移动
    void MoveReadOffset(uint64_t len)
    {
        if (len == 0)
            return;
        // 向后移动的大小一定小于可读数据的大小
        assert(len <= ReadableSize());
        _read_idx += len;
    }
    // 写偏移向后移动
    void MoveWriteOffset(uint64_t len)
    {
        assert(len <= TailSize());
        _write_idx += len;
    }
    // 确保可写空间足够
    void EnsureWriteSpace(uint64_t len)
    {
        if (len <= TailSize())
            return;
        else if (len <= HeadSize() + TailSize()) // 移动数据到最前面
        {
            uint64_t rsz = ReadableSize();
            std::copy(ReadPosition(), ReadPosition() + rsz, Begin());
            _read_idx = 0;
            _write_idx = rsz;
        }
        else // len > HeadSize() + TailSize() // 扩容
        {
            // 这里的策略就是直接调整_buffer的大小，不拷贝其他现有数据
            _buffer.resize(_write_idx + len);
        }
    }
    // 读取数据
    void Read(void *buf, uint64_t len)
    {
        assert(ReadableSize() >= len);
        std::copy(ReadPosition(), ReadPosition() + len, static_cast<char *>(buf));
    }
    void ReadAndPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadOffset(len);
    }
    std::string ReadAsString(uint64_t len)
    {
        assert(ReadableSize() >= len);
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    std::string ReadAsStringAndPop(uint64_t len)
    {
        std::string str = ReadAsString(len);
        MoveReadOffset(len);
        return str;
    }
    // 写入数据
    void Write(const void *data, uint64_t len)
    {
        if (len == 0)
            return;
        EnsureWriteSpace(len);
        const char *d = static_cast<const char *>(data);
        std::copy(d, d + len, WritePosition());
    }
    void WriteAndPush(const void *data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }
    void WriteString(const std::string &data)
    {
        Write(data.c_str(), data.size());
    }
    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }
    void WriteBuffer(Buffer &data)
    {
        Write(data.ReadPosition(), data.ReadableSize());
    }
    void WriteBufferAndPush(Buffer &data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadableSize());
    }
    char *FindCRLF()
    {
        // std::find(ReadPosition(), ReadPosition() + ReadableSize(), '\n');
        char *s = (char *)memchr(ReadPosition(), '\n', ReadableSize());
        return s;
    }
    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
            return "";
        return ReadAsString(pos - ReadPosition() + 1); // 这里+1是为了把换行符号也取出来
    }
    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        return str;
    }
    // 清空缓冲区
    void Clear() { _read_idx = _write_idx = 0; }
};

/**
 * Socket类设计：封装了Socket编程的相关操作
 * 创建socket
 * bind端口号和ip
 * 设置listen套接字
 * 发起连接
 * 接收连接
 * 发送数据和接收数据
 */
class Socket
{
private:
    int _sockfd;

public:
    Socket() : _sockfd(-1) {}
    Socket(int sockfd) : _sockfd(sockfd) {}
    ~Socket() { Close(); }
    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd == -1)
        {
            LOG(FATAL, "create socket error, code: %d, reson: %s", errno, strerror(errno));
            return false;
        }
        // LOG(NORMAL, "create socket success: %d", _sockfd);
        return true;
    }
    bool Bind(uint16_t port, const std::string &ip)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        int n = bind(_sockfd, (struct sockaddr *)&addr, sizeof addr);
        if (n < 0)
        {
            LOG(FATAL, "bind socket error, code: %d, reson: %s", errno, strerror(errno));
            return false;
        }
        // LOG(NORMAL, "bind socket success");
        return true;
    }
    bool Listen(int backlog = 1024)
    {
        int n = listen(_sockfd, backlog);
        // LOG(NORMAL, "listen socket success");
        return n == 0;
    }
    int Accept(std::string *clientip, uint16_t *clientport)
    {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);
        int newsock = accept(_sockfd, (struct sockaddr *)&peer, &len);
        if (newsock < 0)
        {
            LOG(ERROR, "accept socket error, code: %d, reson: %s", errno, strerror(errno));
            return false;
        }
        *clientport = ntohs(peer.sin_port);
        *clientip = inet_ntoa(peer.sin_addr);
        return newsock;
    }
    int Accept()
    {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);
        int newsock = accept(_sockfd, (struct sockaddr *)&peer, &len);
        if (newsock < 0)
        {
            LOG(ERROR, "accept socket error, code: %d, reson: %s", errno, strerror(errno));
        }
        return newsock;
    }
    void Accept(int *newsock)
    {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);
        *newsock = accept(_sockfd, (struct sockaddr *)&peer, &len);
        if (*newsock < 0)
        {
            LOG(ERROR, "accept socket error, code: %d, reson: %s", errno, strerror(errno));
        }
    }

    bool Connect(uint16_t port, const std::string &ip)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        int n = connect(_sockfd, (struct sockaddr *)&addr, sizeof addr);
        if (n < 0)
        {
            LOG(FATAL, "connect socket error, code: %d, reson: %s", errno, strerror(errno));
            return false;
        }
        return true;
    }
    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        ssize_t n = recv(_sockfd, buf, len, flag);
        if (n <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return 0;
            LOG(ERROR, "socket recv error, code:%d, reason:%s", errno, strerror(errno));
            return -1;
        }
        return n;
    }
    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        ssize_t n = send(_sockfd, buf, len, flag);
        if (n <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0;
            LOG(ERROR, "socket send error, code:%d, reason:%s", errno, strerror(errno));
            return -1;
        }
    }
    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT);
    }
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        if (len == 0)
            return 0;
        return Send(buf, len, MSG_DONTWAIT);
    }
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    int Fd() { return _sockfd; }
    bool NonBlack() // 设置非阻塞
    {
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
    bool ReuseAddress() // 设置端口重用
    {
        int opt = 1;
        int n = setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (n == 0)
        {
            // LOG(DEBUG, "设置端口和地址重用成功");
            return true;
        }
        else
        {
            LOG(DEBUG, "设置端口和地址重用失败");
            return false;
        }
        n = setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        if (n == 0)
        {
            // LOG(DEBUG, "设置端口和地址重用成功");
            return true;
        }
        else
        {
            LOG(DEBUG, "设置端口和地址重用失败");
            return false;
        }
    }
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        if (Create() == false)
            return false;
        ReuseAddress();
        if (Bind(port, ip) == false)
            return false;
        if (Listen() == false)
            return false;
        if (block_flag)
            NonBlack();
        // LOG(DEBUG, "create server success");
        return true;
    }
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        if (Create() == false)
            return false;
        if (Connect(port, ip) == false)
            return false;
        // ReuseAddress();
        return true;
    }
};

/**
 * Channel模块：管理套接字上的事件监听与处理
 * 实现思路：对于任意的一个文件描述符，都有要关心的事件_events，以及事件产生的通知_revents
 * 对于触发事件之后的操作，则需要一个回调函数，该回调函数会根据_revents的值，调用不同的函数，包括读事件、写事件、异常事件、连接断开事件和任意事件
 * 提供的功能：
 *  1. 获取关心的事件_events，文件描述符fd，以及发生的事件_revents
 *  2. 设置各种回调函数
 *  3. 对监控的事件进行控制，包括添加、删除、修改
 */
class Channel
{
    using EventCallback = std::function<void()>; // 回调函数类型

private:
    int _fd;
    uint32_t _events;
    uint32_t _revents;
    EventLoop *_loop;
    EventCallback _read_callback;   // 可读事件被触发的回调函数
    EventCallback _write_callback;  // 可写事件被触发的回调函数
    EventCallback _except_callback; // 异常事件被触发的回调函数
    EventCallback _close_callback;  // 连接断开事件被触发的回调函数
    EventCallback _event_callback;  // 任意事件被触发的回调函数
public:
    Channel(int fd, EventLoop *loop) : _fd(fd), _loop(loop) {}
    ~Channel() { close(_fd); }
    int Fd() { return _fd; }
    int Events() { return _events; } // 获取关心的events
    void SetRevents(uint32_t events) { _revents = events; }
    void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
    void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
    void SetExceptCallback(const EventCallback &cb) { _except_callback = cb; }
    void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
    void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }

    bool Readable() { return (_events & EPOLLIN); }
    bool Writeable() { return (_events & EPOLLOUT); }

    void EnableRead() // 开启可读
    {
        _events |= EPOLLIN;
        Update();
    }
    void EnableWrite() // 开启可写
    {
        _events |= EPOLLOUT;
        Update();
    }
    void DisableRead() // 关闭可读
    {
        _events &= ~EPOLLIN;
        // Remove();// 这里调用的不是Remove
        Update();
    }
    void DisableWrite() // 关闭可写
    {
        _events &= ~EPOLLOUT;
        // Remove();// 这里调用的不是Remove
        Update();
    }
    void DisableAll()
    {
        _events = 0;
        Update();
    } // 关闭所有事件监控

    // 这里由于会调用Poller类中的成员函数，在当前类里面无法得知Poller类中有什么成员，所以需要在类外面进行实现，所以这里只给出声明
    void Remove(); // 函数声明，移除监控
    void Update(); // 函数声明，添加、更新监控

    void HandleEvent() // 事件处理
    {
        if (_revents & EPOLLIN || _revents & EPOLLRDHUP || _revents & EPOLLPRI)
        {
            // LOG(DEBUG, "这是一个读事件");
            // 不管任何事件都调用的回调函数
            
            if (_read_callback)
                _read_callback();
        }
        // 这里的操作中，有可能会出现释放连接的操作，所以一次就只处理一个事件
        else if (_revents & EPOLLOUT)
        {
            // LOG(DEBUG, "这是一个写事件");
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            // LOG(DEBUG, "这是一个异常事件");
            if (_except_callback)
                _except_callback();
        }
        else if (_revents & EPOLLHUP)
        {
            // LOG(DEBUG, "这是一个断开事件");
            if (_close_callback)
                _close_callback();
        }
        if (_event_callback)
                _event_callback();
    }
};

/**
 * Poller模块：对整个epoll模型的封装，同时管理监听套接字的事件的指针（通过套接字描述符映射）
 * 实现思路：
 *
 * 理论上全局就一个Poller对象，但是为了方便，这里暂时没有设置单例模式，后面可以添加
 * 提供的功能：
 *  1. 创建epoll模型
 *  2. 封装对epoll模型直接操作借口
 *  3. 添加/更新、移除监控事件
 *  4. 监控事件，并将就绪事件的返回
 */
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel *> _channels;

private:
    void Update(Channel *channel, int op) // 对epoll模型直接操作
    {
        int fd = channel->Fd();
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();
        int n = epoll_ctl(_epfd, op, fd, &ev);
        if (n < 0)
            LOG(FATAL, "epoll model contrl error, code: %d, reason: %s", errno, strerror(errno));
        // assert(n == 0);
        // (void)n;
    }
    bool HaveChannel(Channel *channel)
    {
        auto iter = _channels.find(channel->Fd());
        return iter != _channels.end();
    }

public:
    Poller() : _epfd(-1)
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd < -1)
        {
            LOG(FATAL, "create epoll model error, code: %d, reason: %s", errno, strerror(errno));
            exit(-1);
        }
    }
    void UpdateEvent(Channel *channel) // 添加/修改监控事件
    {
        bool ret = HaveChannel(channel);
        if (ret == false)
        {
            _channels.insert(std::make_pair(channel->Fd(), channel));
            return Update(channel, EPOLL_CTL_ADD); // 不存在添加
        }
        return Update(channel, EPOLL_CTL_MOD); // 存在就更新
    }
    void RemoveEvent(Channel *channel) // 移除监控事件
    {
        auto iter = _channels.find(channel->Fd());
        if (iter != _channels.end())
        {
            _channels.erase(iter);
        }
        Update(channel, EPOLL_CTL_DEL);
    }
    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel *> *actions)
    {
        int timeout = -1;
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, timeout);
        if (nfds < 0)
        {
            if (errno == EINTR)
                return;
            LOG(ERROR, "epoll_wait error, code: %d, reason: %s", errno, strerror(errno));
            exit(-2);
        }
        for (int i = 0; i < nfds; ++i)
        {
            auto iter = _channels.find(_evs[i].data.fd);
            assert(iter != _channels.end());
            iter->second->SetRevents(_evs[i].events);
            actions->push_back(iter->second);
        }
    }
};

/**
 * 时间轮的设计思想：封装一个类，在定时任务创建的时候创建这个对象
 * 在定时任务将要被执行的时候，让这个对象被释放
 * 将定时任务需要执行的函数放在这个类的析构函数中调用，当对象被释放的时候会自动调用任务
 *
 * 手动管理比较麻烦，所以创建一个时间轮，存放某一时刻需要执行的定时任务
 * 当走到这个时刻的时候，执行这个时刻要执行的所有任务（释放这个时刻的所有对象）
 *
 * 由于存在延时、取消定时任务的需求，所以需要经常操作这个时间轮，比较麻烦，这里采用了智能指针管理
 * 使用shared_ptr管理对象指针，当引用计数为0时自动销毁
 *
 * 延时定时任务的时候就在时间轮上对应位置再加上一个该对象的shared_ptr，就能够实现延时的目的
 * 取消任务的时候
 */
using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask
{
private:
    uint64_t _id;         // 定时器任务对象id
    uint32_t _timeout;    // 定时任务超时时间
    bool _canceled;       // false表示没有被取消，true表示被取消了
    TaskFunc _task_cb;    // 定时器对象要执行的任务
    ReleaseFunc _release; // 用于删除TimeWheel中保存的定时器对象信息

public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb)
        : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
    ~TimerTask()
    {
        // LOG(DEBUG, "析构当前任务对象");
        if (_canceled == false)
            _task_cb();
        _release();
    }
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    uint32_t DelayTime() { return _timeout; }
    void Cancel() { _canceled = true; }
};
class TimerWheel
{
    using WeakTask = std::weak_ptr<TimerTask>;
    using PtrTask = std::shared_ptr<TimerTask>;

private:
    int _tick;                                      // 当前的秒针，走到哪里就释放哪里
    int _capacity;                                  // 表盘的最大数量（最大延迟时间）
    std::vector<std::vector<PtrTask>> _wheel;       // 时间轮
    std::unordered_map<uint64_t, WeakTask> _timers; // 保存定时任务
    EventLoop *_loop;                               // 事件管理模块的指针
    int _timerfd;                                   // 定时器描述符
    std::unique_ptr<Channel> _timer_channel;        // 定时器的channel
public:
    TimerWheel(EventLoop *loop)
        : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop), _timerfd(CreateTimerfd()),
          _timer_channel(new Channel(_timerfd, loop))
    {
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead();
    }
    ~TimerWheel() {}
    // 这里对于_timers和_wheel的操作要考虑线程安全问题，如果不想给每次操作都加锁的话，那就让这个函数只能够被EventLoop线程调用
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
    // 这个接口存在线程安全问题，所以只能够在EventLoop线程内调用
    bool HaveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return false;
        }
        return true;
    }

    void RunTimerTask() // 这个函数相当于每秒钟执行一次，相当于秒针向后走
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空当前位置的数组就会自动销毁对应的对象，此时如果有引用计数为0的对象，将会自动调用析构函数，执行定时任务
        // LOG(DEBUG, "执行了当前时间对应的定时任务");
    }

private:
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb) // 添加定时任务
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        _wheel[(_tick + delay) % _capacity].push_back(pt);
        _timers[id] = WeakTask(pt);
        // LOG(DEBUG, "添加定时任务成功");
    }
    void TimerRefreshInLoop(uint64_t id) // 刷新、延迟定时任务
    {
        // 通过保存的定时器对象_timers找到对应的weak_ptr，构造shared_ptr，添加到时间轮
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;                     // 不存在定时任务，没办法刷新延迟
        PtrTask pt = it->second.lock(); // 获取到weak_ptr对象对应的shared_ptr
        int delay = pt->DelayTime();
        _wheel[(_tick + delay) % _capacity].push_back(pt);
        // LOG(DEBUG, "刷新定时任务");
    }
    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
            return; // 不存在定时任务，没办法取消
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }
    static int CreateTimerfd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            LOG(ERROR, "timerfd create error, code:%d, reason: %s", errno, strerror(errno));
            abort();
        }
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0; // 第一次的超时时间为1s
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0; // 第一次超时之后，每次超时的时间间隔
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }
    /* 有可能因为其他事件的处理花费事件很长，在处理定时器描述符事件的时候，有可能已经超时了很多次了， 这里read的返回值就是超时的次数*/
    int ReadTimerFd()
    {
        uint64_t times;
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            LOG(ERROR, "read timerfd error");
            abort();
        }
        return times;
    }
    void OnTime()
    {
        // LOG(DEBUG, "触发了定时任务事件");
        int times = ReadTimerFd(); // 根据实际超时的次数执行超时任务
        for (int i = 0; i < times; ++i)
        {
            RunTimerTask();
        }
    }
    /* 用这个函数的时候，一定是shared_ptr的引用已经=0，此时weak_ptr也不需要存在了 */
    void RemoveTimer(uint64_t id) // 从_timers中删除定时任务
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
            _timers.erase(id);
    }
};

/**
 * 事件循环类：
 *  使用Poller类封装的方法，监控当前线程关心的事件
 *  如果事件到达，就让事件在当前线程中执行（利用任务池的概念）
 *  事件到达->处理事件（会把事件转化成任务，如果当前需要执行的任务是在当前这个EventLoop中的，就直接执行
 *  如果当前的任务是在其他线程的，就把这个任务放进任务池，当事件处理完之后，再回到当前线程执行任务池中的任务）
 */
class EventLoop
{
    using TaskFunc = std::function<void()>; // 任务队列中要执行的内容

private:
    std::thread::id _thread_id;              // 线程id
    int _event_fd;                           // eventfd唤醒IO事件监控所导致的阻塞
    std::unique_ptr<Channel> _event_channel; // eventfd对应的channel
    Poller _poller;                          // 要管理事件的epoll模型
    /*由于任务池有可能被多个线程所访问，所以在访问任务池的时候，要给任务池加锁*/
    std::mutex _mutex;                       // 任务池的锁
    std::vector<TaskFunc> _tasks;            // 任务池
    TimerWheel _timer_wheel;                 // 时间轮
private:
    void RunAllTask() // 执行任务池中的所有任务
    {
        std::vector<TaskFunc> functor;
        { // RAII的模式管理锁
            std::unique_lock<std::mutex> lck(_mutex);
            _tasks.swap(functor);
        }
        for (auto f : functor)
        {
            f();
        }
    }
    /*这里的eventfd的作用是唤醒IO事件监控所导致的阻塞，IO事件监控的时候，如果没有任务，会在调用Poller::Poll时阻塞，
    如果有任务，就向eventfd中写入一个1，表示出现了一个任务，唤醒eventfd，然后执行任务，把eventfd清空，这样下一次就阻塞在eventfd上了*/
    static int CreateEventfd() // 创建eventfd
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            LOG(FATAL, "create eventfd error, code:%d, reason: %s", errno, strerror(errno));
            abort();
        }
        return efd;
    }
    void ReadEventFd()
    {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret < 0)
        {
            if (ret == EINTR || errno == EAGAIN)
                return;
            LOG(FATAL, "read eventfd error, code:%d, reason:%s", errno, strerror(errno));
            abort();
        }
    }
    void WeakUpEventFd() // 唤醒eventfd
    {
        // 实际上就是向eventfd中写入一个数据
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (ret == EINTR)
                return;
            LOG(FATAL, "write eventfd error, code:%d, reason:%s", errno, strerror(errno));
            abort();
        }
    }

public:
    EventLoop()
        : _thread_id(std::this_thread::get_id()),
          _event_fd(CreateEventfd()),
          _event_channel(new Channel(_event_fd, this)),
          _timer_wheel(this)
    {
        // 给_event_channel添加读回调函数
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));
        // 开启读事件监控
        _event_channel->EnableRead();
    }
    void RunInLoop(const TaskFunc &cb) // 判断当前任务是否在当前线程，如果在就执行，不在就压入任务队列
    {
        if (IsInLoop())
        {
            // LOG(DEBUG, "当前任务在同一个线程，直接执行");
            return cb();
        }
        else
        {
            // LOG(DEBUG, "当前任务在不在同一个线程，压入任务池");
            QueueInLoop(cb);
        }
    }
    void QueueInLoop(const TaskFunc &cb) // 将操作压入任务队列
    {
        // 1. 将操作压入任务队列
        {
            std::unique_lock<std::mutex> lck(_mutex);
            _tasks.push_back(cb);
        }
        // 2. 唤醒由于没有事件发生导致的事件监控阻塞
        WeakUpEventFd();
    }
    bool IsInLoop() // 判断当前线程是否是EventLoop对应的线程
    {
        return (_thread_id == std::this_thread::get_id());
    }
    void AssertInLoop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }

    void Start() // 事件监控-》就绪事件处理-》执行任务
    {
        while (true)
        {
            // 1. 事件监控
            std::vector<Channel *> actives;
            _poller.Poll(&actives);
            // 2. 事件处理
            for (auto &a : actives)
            {
                a->HandleEvent();
            }
            // 3. 执行任务
            RunAllTask();
        }
    }
    void UpdateEvent(Channel *channel) { _poller.UpdateEvent(channel); } // 添加/更新事件监控
    void RemoveEvent(Channel *channel) { _poller.RemoveEvent(channel); } // 移除事件监控
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) { return _timer_wheel.TimerAdd(id, delay, cb); }
    void TimerRefresh(uint64_t id) { return _timer_wheel.TimerRefresh(id); }
    void TimerCancel(uint64_t id) { return _timer_wheel.TimerCancel(id); }
    bool HaveTimer(uint64_t id) { return _timer_wheel.HaveTimer(id); }
};

/**
 * LoopThread：一个Thread对应一个Loop，这也就是本项目设计的核心思想（One Thread One Loop）
*/
class LoopThread
{
private:
    std::thread _thread; // EventLoop对应的线程
    EventLoop *_loop;    // EventLoop对象的指针（在新线程内部实例化）
    std::mutex _mutex; // 一个互斥锁
    std::condition_variable _cond; // 条件变量

    private:
    // 这是一个线程入口函数，在这个函数里面实例化EventLoop对象，唤醒cond上有可能阻塞的线程
    void ThreadEntry()
    {
        EventLoop loop; // 这里把loop在栈上实例化，然后把指针赋值给_loop，是为了让loop的生命周期随栈
        {
            std::unique_lock<std::mutex> lck(_mutex);
            _loop = &loop;
            _cond.notify_all(); // 唤醒cond上阻塞的线程
        }
        loop.Start();
    }
public:
    LoopThread() : _loop(nullptr), _thread(std::thread(&LoopThread::ThreadEntry, this))
    {}
    EventLoop *GetLoop() 
    { 
        EventLoop *loop = nullptr;
        {
            std::unique_lock<std::mutex> lck(_mutex); // 加锁
            _cond.wait(lck, [&]() { return _loop != nullptr; }); // 循环等待
            loop = _loop;
        }
        return loop;
    }
};

/**
 * LoopThreadPool：一个线程池，里面管理当前进程的所有线程，包括一个主线程（用于接收连接）和若干个从属线程（用于处理事件）
 * 提供的功能：1. 创建从属线程，管理从属线程
 *           2. 为所有的从属线程分配对应的任务（这里采用轮转的策略分配）
 * 后续改进：这里的线程池应该是只有一个的，后面考虑设置为单例模式
*/
class LoopThreadPool
{
private:
    EventLoop *_main_loop; // 主线程
    int _next_loop_index; // 下一个从属线程的索引
    int _thread_num; // 从属线程个数
    std::vector<LoopThread *> _threads; // 从属线程指针
    std::vector<EventLoop *> _loops;

public:
    LoopThreadPool(EventLoop *main_loop) : _main_loop(main_loop), _next_loop_index(0), _thread_num(0) {}
    void SetThreadNum(int num) { _thread_num = num; } // 设置从属线程个数
    void Create() // 创建从属线程
    {
        if(_thread_num > 0) 
        {
            _threads.resize(_thread_num);
            _loops.resize(_thread_num);
            for(int i = 0; i < _thread_num; i++)
            {
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->GetLoop();
            }
        }
        
    }
    EventLoop *GetNextLoop()
    {
        EventLoop *loop = _main_loop;
        if (!_loops.empty())
        {
            loop = _loops[_next_loop_index];
            _next_loop_index = (_next_loop_index + 1) % _thread_num;
        }
        return loop;
    }

};

/**
 * Connection类:每个连接都有一个对应的连接对象，这里封装了网络连接的读写事件监控，定时器管理，以及读写事件的处理
 *             包括关联的loop和socket对象和连接对应的上下文
 * 提供的功能：对连接的管理（建立连接，关闭连接，发送消息，接收消息，非活跃连接的控制，获取上下文，切换应用层协议）
 * 
*/
class Connection : public std::enable_shared_from_this<Connection>
{
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;

private:
    uint64_t _conn_id;             // Connection对象的唯一id（同时作为timerid）
    int _sockfd;                   // 连接关联的文件描述符
    bool _enable_inactive_release; // 启动非活跃连接销毁标志，默认是false
    EventLoop *_loop;              // 连接所关联的一个loop
    ConnStatu _statu;              // 连接的状态
    Socket _socket;                // 连接的套接字管理
    Channel _channel;              // 连接的事件管理
    Buffer _in_buffer;             // 输入缓冲区
    Buffer _out_buffer;            // 输出缓冲区
    Any _context;                  // 请求处理的上下文

    // 回调函数
    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _anyEvent_callback;

    ClosedCallback _server_closed_callback; // 这个是组件内设置的

private: // 私有的成员方法
    /*channel事件回调函数*/
    void HandleRead()
    {
        // 1. 接收socket数据，放在缓冲区
        char buffer[65536];
        ssize_t ret = _socket.NonBlockRecv(buffer, sizeof(buffer) - 1);
        if (ret < 0)
        {
            return ShutdownInLoop();
        }
        else if (ret == 0)
        {
            return; // 这里ret==0不是连接断开，连接断开返回-1
        }
        _in_buffer.WriteAndPush(buffer, ret); // 把数据放入输入缓冲区
        // 2. 调用message_callback处理
        if (_in_buffer.ReadableSize() > 0)
        {
            // shared_from_this是从当前对象获取自身的shared_ptr对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }
    void HandleWrite()
    {
        // LOG(DEBUG, "HandleWrite in, 缓冲区大小%d", _out_buffer.ReadableSize());
        // 1.
        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPosition(), _out_buffer.ReadableSize());
        if (ret < 0)
        {
            // 此时发送失败，如果输入缓冲区有数据就先处理输入缓冲区数据，再关闭连接
            if (_in_buffer.ReadableSize() > 0)
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return Release(); // 这时候就是实际关闭了
        }
        _out_buffer.MoveReadOffset(ret);
        // LOG(DEBUG, "HandleWrite out, 缓冲区大小%d", _out_buffer.ReadableSize());
        // 如果当前连接是待关闭状态，并且发送缓冲区位0，就关闭连接
        if (_out_buffer.ReadableSize() == 0)
        {
            _channel.DisableWrite(); // 防止出现写事件busy
            if (_statu == DISCONNECTED)
                return Release();
        }
    }
    void HandleClosed() // 触发关闭事件
    {
        if (_in_buffer.ReadableSize() > 0) // 如果有数据就先处理一下，然后再关闭
        {
            _message_callback(shared_from_this(), &_in_buffer);
        }
        return Release(); // 关闭连接
    }
    void HandleExcept() // 触发错误事件
    {
        return HandleClosed();
    }
    void HandleEvent() // 触发任意事件
    {
        // 1. 延迟定时销毁任务
        if (_enable_inactive_release)
            _loop->TimerRefresh(_conn_id);
        // 2. 调用组件使用者的任意事件回调函数
        if (_anyEvent_callback)
            _anyEvent_callback(shared_from_this());
    }

    void EstablishedInLoop()
    {
        // 1. 修改连接状态
        assert(_statu == CONNECTING); // 当前状态一定是半连接的
        _statu = CONNECTED;
        // 2. 启动读事件监控
        _channel.EnableRead();
        // 3. 调用回调函数
        if (_connected_callback)
            _connected_callback(shared_from_this());
    }
    void ReleaseInLoop()
    {
        // LOG(DEBUG, "ReleaseInLoop in");
        // 1. 修改连接状态
        _statu = DISCONNECTED;
        // 2. 移除事件监控
        _channel.Remove();
        // 3. 关闭描述符
        _socket.Close();
        // 4. 如果当前定时器任务在timerwheel中，就取消任务
        if (_loop->HaveTimer(_conn_id))
            DisableInactiveReleaseInLoop();
        // 5. 调用关闭函数
        // 这里先调用用户的，为了避免先移除服务器的处理，会导致Connection对象被释放
        if (_closed_callback)
            _closed_callback(shared_from_this());
        if (_server_closed_callback)
            _server_closed_callback(shared_from_this());
    }
    void SendInLoop(Buffer &buf) // 发送数据，将要发送的数据拷贝到输出缓冲区，启动写事件监控
    {
        // LOG(DEBUG, "SendInLoop in");
        if (_statu == DISCONNECTED)
            return;
        _out_buffer.WriteBufferAndPush(buf);
        if (_channel.Writeable() == false)
        {
            _channel.EnableWrite();
        }
        // LOG(DEBUG, "SendInLoop out");
    }
    void ShutdownInLoop() // 关闭连接，实际上并不直接关闭，需要判断是否有数据待处理
    {
        // LOG(DEBUG, "ShutdownInLoop in");
        _statu == DISCONNECTING; // 设置连接为半关闭状态
        if (_in_buffer.ReadableSize() > 0)
        {
            if (_message_callback)
                _message_callback(shared_from_this(), &_in_buffer);
            // LOG(DEBUG, "ShutdownInLoop 1");
        }
        if (_out_buffer.ReadableSize() > 0)
        {
            // std::cout << "_out_buffer.ReadableSize()=" << _out_buffer.ReadableSize() << std::endl;
            if (_channel.Writeable() == false)
            {
                _channel.EnableWrite();
            }
            // LOG(DEBUG, "ShutdownInLoop 2");
            // std::cout << "_out_buffer.ReadableSize()=" << _out_buffer.ReadableSize() << std::endl;
        }
        if (_out_buffer.ReadableSize() == 0)
        {
            Release();
            // LOG(DEBUG, "ShutdownInLoop 3");
        }
        _statu = DISCONNECTED; // 这里等下注释
        // LOG(DEBUG, "ShutdownInLoop out");
    }

    void EnableInactiveReleaseInLoop(int sec) // 启动非活跃连接销毁
    {
        // 1.将标志位置为true
        _enable_inactive_release = true;
        // 2.添加或延时定时任务
        // 2.1如果存在，就延迟
        if (_loop->HaveTimer(_conn_id))
            _loop->TimerRefresh(_conn_id);
        // 2.2如果不存在就添加定时销毁任务
        else
            _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
    }
    void DisableInactiveReleaseInLoop() // 关闭非活跃连接销毁
    {
        // 1. 标志位置为false
        _enable_inactive_release = false;
        // 2. 取消定时任务
        if (_loop->HaveTimer(_conn_id))
            _loop->TimerCancel(_conn_id);
    }
    // 切换协议
    void UpgradeInLoop(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg,
                       const ClosedCallback &closed, const AnyEventCallback &event)
    {
        _context = context;
        _connected_callback = conn;
        _message_callback = msg;
        _closed_callback = closed;
        _anyEvent_callback = event;
    }

public: // 提供给用户的接口
    /* 测试接口 */
    Buffer &inbuffer() { return _in_buffer; }
    Buffer &outbuffer() { return _out_buffer; }
    /* end of  test */

    Connection(uint64_t id, int sockfd, EventLoop *loop)
        : _conn_id(id), _sockfd(sockfd), _loop(loop), _enable_inactive_release(false), _statu(CONNECTING), _socket(_sockfd), _channel(_sockfd, loop)
    {
        _channel.SetCloseCallback(std::bind(&Connection::HandleClosed, this));
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
        _channel.SetExceptCallback(std::bind(&Connection::HandleExcept, this));
    }
    ~Connection() { LOG(DEBUG, "release connection: %p", this); }
    int Fd() { return _sockfd; }
    uint64_t Id() { return _conn_id; }
    bool Connected() { return _statu == CONNECTED; }            // 是否处于连接状态
    void SetContext(const Any &context) { _context = context; } // 设置上下文
    Any *GetContext() { return &_context; }                     // 获取上下文

    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_callback = cb; }
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }
    void SetServerClosedCallback(const ClosedCallback &cb) { _server_closed_callback = cb; }
    void SetAnyEventCallback(const AnyEventCallback &cb) { _anyEvent_callback = cb; }
    void Established() // 连接建立后，设置和相关启动的函数
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
    }
    void Send(const char *data, size_t len) // 发送数据，将要发送的数据拷贝到输出缓冲区，启动写事件监控
    {
        // 这里的发送操作可能不会立刻被执行，只是把发送操作压入任务池，有可能在执行的时候，data指向的空间已经被释放了，所以这里需要构造一个临时对象来保存
        Buffer buf;
        buf.WriteAndPush(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf)));
    }
    void Shutdown() // 关闭连接，实际上并不直接关闭，需要判断是否有数据待处理
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }
    /* release操作不应该在事件处理的时候操作，而是压入任务池，等待本次的所有事件处理完毕，处理任务池的任务的时候再执行 */
    void Release()
    {
        _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }
    void EnableInactiveRelease(int sec) // 启动非活跃连接销毁
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }
    void DisableInactiveRelease() // 关闭非活跃连接销毁
    {
        _loop->RunInLoop(std::bind(&Connection::DisableInactiveReleaseInLoop, this));
    }
    // 切换协议
    // 这个接口一定要在EventLoop线程中执行，否则可能出现新收到的数据还在使用原协议处理
    void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg,
                 const ClosedCallback &closed, const AnyEventCallback &event)
    {
        _loop->AssertInLoop();
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn, msg, closed, event));
    }
};

/**
 * Acceptor对监听套接字进行封装和管理，对监听套接字进行事件监控，监听套接字有新连接事件时，调用回调函数，创建连接
*/
class Acceptor
{
    using AcceptCallback = std::function<void(int)>;

private:
    Socket _socket;                          // 创建监听套接字
    EventLoop *_loop;                        // 对监听套接字进行事件监控
    Channel _channel;                        // 对监听套接字进行事件管理
    AcceptCallback _new_connection_callback; // 新连接回调
private:
    void HandleRead() // 处理新连接到来的操作
    {
        // 1. 获取新连接
        int connfd = _socket.Accept();
        if (connfd < 0)
            return;
        // 2. 创建连接
        if (_new_connection_callback)
            _new_connection_callback(connfd);
    }
    int CreateServer(uint16_t port)
    {
        bool ret = _socket.CreateServer(port);
        assert(ret == true);
        return _socket.Fd();
    }

public:
    Acceptor(EventLoop *loop, int port)
        : _socket(CreateServer(port)), _loop(loop), _channel(_socket.Fd(), loop)
    {
        _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this)); // bind新连接到来时的操作
    }
    void SetNewConnectionCallback(const AcceptCallback &cb) { _new_connection_callback = cb; } // 设置新连接到来之后的回调函数

    void Listen() { _channel.EnableRead(); } // 启动listen套接字的读监控
};

/**
 * TcpServer对上面所有的模块进行封装和管理，提供一些方便的接口调用上述模块的接口，很方便的构建一个TcpServer服务器
 * 同时能够设置定时任务
*/
class TcpServer
{
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
private:
    int _port; // 监听端口
    int _timeout; // 多长时间没有连接认为是非活跃连接
    bool _enable_inactive_release; // 是否启动非活跃连接销毁
    uint64_t _conn_id; // 自增的连接唯一id
    Acceptor _acceptor; // 监听套接字的管理对象
    EventLoop _base_loop; // 主线程的eventloop对象，处理监听事件
    LoopThreadPool _threadpool; // 从属线程池
    std::unordered_map<uint64_t, PtrConnection> _conns; // 所有连接对象的shared_ptr管理

    // 回调函数
    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _anyEvent_callback;

private:
    void NewConnection(int fd)
    {
        _conn_id++;
        PtrConnection newconn(new Connection(_conn_id, fd, _threadpool.GetNextLoop()));

        newconn->SetMessageCallback(_message_callback);
        newconn->SetClosedCallback(_closed_callback);
        newconn->SetConnectedCallback(_connected_callback);
        newconn->SetAnyEventCallback(_anyEvent_callback);
        newconn->SetServerClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));

        if(_enable_inactive_release)
            newconn->EnableInactiveRelease(_timeout); // 非活跃连接的超时释放操作
        newconn->Established(); // 就绪初始化

        _conns.insert(std::make_pair(_conn_id, newconn));
        LOG(DEBUG, "新连接：%d", _conn_id);
    }
    void RemoveConnectionInLoop(const PtrConnection &conn)
    {
        int id = conn->Id();
        auto it = _conns.find(id);
        if(it != _conns.end())
            _conns.erase(id);
    }
    void RemoveConnection(const PtrConnection &conn)
    {
        _base_loop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
        _conns.erase(conn->Id());
    }
    void RunAfterInLoop(const TaskFunc &cb, int delay)
    {
        _base_loop.TimerAdd(_conn_id, delay, cb);
    }
public:
    TcpServer(uint16_t port, int thread_num = 0)
        : _port(port),_conn_id(0), _enable_inactive_release(false)
        , _acceptor(&_base_loop, port), _threadpool(&_base_loop) 
        {
            _threadpool.Create(); // 创从属线程池（这里是不是不能创建从属线程，由于在调用之前没有设置从属线程个数）
            _acceptor.Listen(); // 启动监听套接字的读监控
            _acceptor.SetNewConnectionCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        }

    void SetThreadNum(int num) { _threadpool.SetThreadNum(num); }

    /* 设置回调函数 */
    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_callback = cb; }
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }
    void SetAnyEventCallback(const AnyEventCallback &cb) { _anyEvent_callback = cb; }

    void EnableInactiveRelease(int sec) // 启动非活跃连接销毁
    {
        _timeout = sec;
        _enable_inactive_release = true;
    }
    void RunAfter(const TaskFunc &cb, int64_t delay)
    {
        _base_loop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, cb, delay));
    }
    void Start()
    {
        _base_loop.Start();
    }
    
};

// 这里是一些必须在所有类之后实现的函数，因为这些函数使用到了在后续定义的类中的成员函数，在类内实现将会出现xx方法味定义的情况
void Channel::Remove() { _loop->RemoveEvent(this); } // 移除监控
void Channel::Update() { _loop->UpdateEvent(this); } // 添加、更新监控

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

/* 全局初始化, 忽略SIGPIPE信号,防止退出*/
class NetWork
{
public:
    NetWork()
    {
        LOG(DEBUG, "SIGPIPE INIT");
        signal(SIGPIPE, SIG_IGN); 
    }
};
// 构建全局变量，在程序刚开始运行的时候就执行构造函数，在构造函数内忽略SIGPIPE信号，保证首先执行
static NetWork net; 