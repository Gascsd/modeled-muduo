// 超时连接客户端

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
        sleep(15);
    }
    sock.Close();
    return 0;
}