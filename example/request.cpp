#include <iostream>
#include <regex>
#include <string>


int main()
{
    std::smatch matches;
    std::string line = "GET / HTTP/1.1\r\n";
    std::regex reg("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase); // 正则表达式解析首行
    bool ret = std::regex_match(line, matches, reg);
    std::cout << ret << std::endl;
    for(auto &str : matches)
    {
        std::cout << str << std::endl;
    }

    return 0;
}