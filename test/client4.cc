// 业务处理超时，查看服务器的情况
/**
 * 当服务器达到性能瓶颈时
 */

#include "../source/http/http.hpp"
int main()
{
    for(int i = 0; i < 10; i++)
    {
        pid_t pid = fork();
        if(pid < 0)
        {
            LOG(ERROR, "fork error");
            exit(0);
        }
        else if(pid == 0)
        {
            Socket sock;
            sock.CreateClient(8080, "127.0.0.1");
            std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
            while (1)
            {
                assert(sock.Send(req.c_str(), req.size()) != -1);
                char buffer[1024] = {0};
                assert(sock.Recv(buffer, 1023) != -1);
                LOG(DEBUG, "[%s]", buffer);
            }
            sock.Close();
            exit(0);
        }
    }
    while (1)
    {
        sleep(1);
    }
    
    return 0;
}
