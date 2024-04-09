#include "http.hpp"


#define WEBROOT "./webroot/"


std::string RequestStr(const HttpRequest &req)
{
    std::stringstream ss;
    ss << req._method << " " << req._path << " " << req._version << "\r\n";
    for (auto &item : req._params)
    {
        ss << item.first << ": " << item.second << "\r\n";
    }
    for(auto &it : req._headers)
    {
        ss << it.first << ": " << it.second << "\r\n";
    }
    ss << "\r\n";
    ss << req._body;
    return ss.str();
}

void Hello(const HttpRequest &req, HttpResponse &resp)
{
    std::string str = RequestStr(req);
    resp.SetContent(str, "text/plain");
    // sleep(15);
}
void Login(const HttpRequest &req, HttpResponse &resp)
{
    std::string str = RequestStr(req);
    resp.SetContent(str, "text/plain");
}
void PutFile(const HttpRequest &req, HttpResponse &resp)
{
    std::string str = WEBROOT + req._path;
    Util::WriteFile(str, req._body);
}
void DeleteFile(const HttpRequest &req, HttpResponse &resp)
{
    std::string str = RequestStr(req);
    resp.SetContent(str, "text/plain");
}
int main()
{
    HttpServer server(8080);
    server.SetThreadNum(3);
    server.SetBasePath(WEBROOT); // 设置静态文件路径
    server.Get("/hello", Hello);
    server.Post("/login", Login);
    server.Put("/1234.txt", PutFile);
    server.Delete("/1234.txt", DeleteFile);
    server.Listen();
    return 0;
}