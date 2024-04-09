// 测试连续给服务器发送多次请求，能够正常处理

#include "../source/http/http.hpp"
int main()
{
    Socket sock;
    sock.CreateClient(8080, "127.0.0.1");
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    req += req;
    req += req;
    req += req;
    while(1) 
    {
        assert(sock.Send(req.c_str(), req.size()) != -1);
        char buffer[1024] = {0};
        assert(sock.Recv(buffer, 1023) != -1);
        LOG(DEBUG, "[%s]", buffer);
        sleep(15);
    }
    sock.Close();
    return 0;
}