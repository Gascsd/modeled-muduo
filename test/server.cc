#include "../source/server.hpp"
#include <iostream>

// std::unordered_map<uint64_t, PtrConnection> _conns;
// EventLoop baseloop;
// LoopThreadPool *loop_pool;
// uint64_t conn_id = 0;
// int i = 1;


// void ConnectionDestory(const PtrConnection &conn)
// {
//     LOG(DEBUG, "关闭连接%d", conn->Fd());
//     _conns.erase(conn->Id());
// }
// void OnConnected(const PtrConnection &conn)
// {
//     LOG(DEBUG, "new conneciont: %p", conn.get());
// }
// void OnMessage(const PtrConnection &conn, Buffer *buf)
// {
//     LOG(DEBUG, "%s", buf->ReadPosition());
//     buf->MoveReadOffset(buf->ReadableSize());
//     std::string str = "Hello World";
//     str += std::to_string(i++);
//     conn->Send(str.c_str(), str.size());
//     // LOG(DEBUG, "发送信息成功，下面直接关闭连接，此时outbuffer内数据大小：%d, 内容为：%s", conn->outbuffer().ReadableSize(), conn->outbuffer().ReadPosition());
//     conn->Shutdown(); // 关闭连接，实际上并不直接关闭，需要判断是否有数据待处理
// }


// void NewConnection(int fd)  
// {
//     conn_id++;

//     PtrConnection newconn(new Connection(conn_id, fd, loop_pool->GetNextLoop()));
//     newconn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1, std::placeholders::_2));
//     newconn->SetServerClosedCallback(std::bind(ConnectionDestory, std::placeholders::_1));
//     newconn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
    
//     newconn->EnableInactiveRelease(5); // 非活跃连接的超时释放操作
//     newconn->Established(); // 就绪初始化

//     _conns.insert(std::make_pair(conn_id, newconn));
//     LOG(DEBUG, "新连接：%d", conn_id);
// }

// int main()
// {
//     loop_pool = new LoopThreadPool(&baseloop);
//     loop_pool->SetThreadNum(2);
//     loop_pool->Create();
//     Acceptor acceptor(&baseloop , 8080);
//     acceptor.SetNewConnectionCallback(std::bind(NewConnection, std::placeholders::_1)); // 设置新连接处理的函数
//     acceptor.Listen(); 
//     baseloop.Start();
//     return 0;
// }



void OnConnected(const PtrConnection &conn)
{
    LOG(DEBUG, "new conneciont: %p", conn.get());
}
void OnClosed(const PtrConnection &conn)
{
    LOG(DEBUG,  "close conneciont: %p", conn.get());
}
void OnMessage(const PtrConnection &conn, Buffer *buf)
{
    LOG(DEBUG, "new message: %s", buf->ReadPosition());
    buf->MoveReadOffset(buf->ReadableSize());
    std::string str = "Hello World";
    conn->Send(str.c_str(), str.size());
    // conn->Shutdown();
}
int main()
{
    TcpServer server(8080);
    server.SetThreadNum(2);
    // server.EnableInactiveRelease(5);

    server.SetMessageCallback(OnMessage);
    server.SetConnectedCallback(OnConnected);
    server.SetClosedCallback(OnClosed);
    server.Start();

    return 0;
}

