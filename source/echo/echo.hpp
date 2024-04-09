#pragma once

#include "../server.hpp"


class EchoServer 
{
private:
    TcpServer _server;
private:
    void OnConnected(const PtrConnection &conn) // 创建连接
    {
        LOG(DEBUG, "new conneciont: %p", conn.get());
    }
    void OnClosed(const PtrConnection &conn)
    {
        LOG(DEBUG,  "close conneciont: %p", conn.get());
    }
    void OnMessage(const PtrConnection &conn, Buffer *buf)
    {
        // LOG(NORMAL, "new message: %s", buf->ReadPosition()); // 打印收到的消息
        // std::cout << "收到消息# " << buf->ReadPosition() << std::endl;
        conn->Send(buf->ReadPosition(), buf->ReadableSize());
        buf->MoveReadOffset(buf->ReadableSize());
        conn->Shutdown();
    }
public:
    EchoServer(int port) : _server(port)
    {
        _server.SetThreadNum(2);
        _server.EnableInactiveRelease(5);
        _server.SetConnectedCallback(std::bind(&EchoServer::OnConnected, this, std::placeholders::_1));
        _server.SetClosedCallback(std::bind(&EchoServer::OnClosed, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&EchoServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void Start()
    {
        _server.Start();
    }
};