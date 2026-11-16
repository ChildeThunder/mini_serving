#include "InferenceHandler.h"
#include <string>
#include <limits>
#include <thread>
#include <chrono>
using namespace std;

bool parse_input(
    const std::string &body,
    std::vector<int32_t> &input)
{
    input.clear();
    size_t start = 0;
    size_t pos;
    while (start < body.size())
    {
        // 跳过空格
        while (start < body.size() && body[start] == ' ')
        {
            start++;
        }
        if (start >= body.size())
        {
            break;
        }
        pos = body.find(' ', start);
        string num_str;
        if (pos == string::npos)
        {
            num_str = body.substr(start);
            start = body.size();
        }
        else
        {
            num_str = body.substr(start, pos - start);
            start = pos + 1;
        }
        int32_t val;
        size_t used = 0;
        try
        {
            //&used 的作用是：让 stoi() 告诉你，这次转换实际使用了字符串中的多少个字符。
            val = stoi(num_str, &used);
        }
        catch (const logic_error &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        // 防止输入0～9之外的其他字符
        if (used != num_str.size())
        {
            return false;
        }
        input.push_back(val);
        // 防止数字过大
        if (input.size() > 64)
        {
            return false;
        }
    }
    return true;
}
int64_t mock_inference(
    const std::vector<int32_t> &input)
{
    // this_thread::sleep_for(chrono::seconds(1));

    int64_t sum = 0;
    for (auto i : input)
    {
        sum += i;
    }
    return sum;
}
