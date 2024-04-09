#include "../source/server.hpp"
#include <iostream>
int main()
{
    Socket sock;
    sock.CreateClient(8080, "127.0.0.1");
    for(int i = 0; i < 3; ++i)
    {
        std::string str = "hello world!!!";
        int ret = sock.Send(str.c_str(), str.size());
        if(ret < 0) LOG(DEBUG, "code:%d, reason: %s", errno, strerror(errno));
        // LOG(DEBUG, "send success");
        char buf[1024] = {0};
        ret = sock.Recv(buf, 1023);
        if(ret < 0) LOG(DEBUG, "code:%d, reason: %s", errno, strerror(errno));
        LOG(DEBUG, "client recv: %s", buf);
        sleep(1);
    }
    while (true) sleep(1);
    sock.Close();
    return 0;
}