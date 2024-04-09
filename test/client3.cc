// 给服务器发送数据，在报头中描述body的长度很大，但是实际的body很小，查看服务器结果
/**
 * 1. 结果1：如果client只发送一次数据，服务器永远收不到一个完整请求，在超时时间之后就会断开连接
 * 2. 结果2：如果client发送很多次数据，服务器会将后面的请求当作前面请求的正文，后面有可能会因为处理错误断开连接
*/

#include "../source/http/http.hpp"
int main()
{
    Socket sock;
    sock.CreateClient(8080, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 30\r\n\r\nHello World";
    while(1) 
    {
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(req.c_str(), req.size()) != -1);
        char buffer[1024] = {0};
        assert(sock.Recv(buffer, 1023) != -1);
        LOG(DEBUG, "[%s]", buffer);
        sleep(15);
    }
    sock.Close();
    return 0;
}