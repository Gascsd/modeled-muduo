#include "echo.hpp"

int main()
{
    EchoServer server(8080);//创建服务器
    server.Start();//启动服务器
    return 0;
}