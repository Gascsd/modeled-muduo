#include "../source/http/http.hpp"

// static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> &result)
void TestSplit()
{
    std::string str = "abc,, ,,de,fg,hij,,";
    std::vector<std::string> vec;
    Util::Split(str, " ", vec);
    for (auto &i : vec)
    {
        std::cout << i << std::endl;
    }
}

void TestReadFile()
{
    std::string content;
    Util::ReadFile("./request.cpp", content);
    std::cout << content << std::endl;

    Util::WriteFile("./request_back.cpp", content);
}

void TestUrl()
{
    std::string url = "C++";
    std::string url1 = "C  ";

    std::string str1 = "C%2B%2B";
    std::string str2 = "C++";

    std::cout << Util::UrlDecode(str1, false) << std::endl;
    std::cout << Util::UrlDecode(str2, true) << std::endl;
}

void TestStatu()
{
    std::cout << Util::GetStatusCodeDesc(404) << std::endl;
}

void TestMime()
{
    std::cout << Util::GetFileMime("login.xxx") << std::endl;
}

void TestISDIR()
{
    std::cout << Util::IsDirectory("testdir") << std::endl;
    std::cout << Util::IsDirectory("request.cpp") << std::endl;
    std::cout << Util::IsRegular("testdir") << std::endl;
    std::cout << Util::IsRegular("request.cpp") << std::endl;
}

void TestIsValidPath()
{
    std::cout << Util::IsValidPath("../login.txt") << std::endl;
    std::cout << Util::IsValidPath("login/xxx/../xxx/html") << std::endl;
    std::cout << Util::IsValidPath("login/xxx/../../../xxx/html") << std::endl;
    std::cout << Util::IsValidPath("html/../../index.html") << std::endl;
}

int main()
{
    TestIsValidPath();
    return 0;
}