// 创建一个客户端，持续给服务器发送消息，直到超时时间看看是否正常
#include "../source/http/http.hpp"


int main()
{
    Socket sock;
    sock.CreateClient(8080, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    while(1) 
    {
        assert(sock.Send(req.c_str(), req.size()));
        char buffer[1024] = {0};
        assert(sock.Recv(buffer, 1023));
        LOG(DEBUG, "[%s]", buffer);
        sleep(3);
    }
    sock.Close();
    return 0;
}