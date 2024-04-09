// 大文件


#include "../source/http/http.hpp"
int main()
{
    Socket sock;
    sock.CreateClient(8080, "127.0.0.1");
    std::string req = "PUT /1234.txt HTTP/1.1\r\nConnection: keep-alive\r\n";
    std::string body;
    Util::ReadFile("hello.txt", body);
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    while(1) 
    {
        assert(sock.Send(req.c_str(), req.size()) != -1);
        assert(sock.Send(body.c_str(), body.size()) != -1);
        char buffer[1024] = {0};
        assert(sock.Recv(buffer, 1023) != -1);
        LOG(DEBUG, "[%s]", buffer);
        sleep(15);
    }
    sock.Close();
    return 0;
}