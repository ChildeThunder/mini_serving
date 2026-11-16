// 推理业务层
// 负责 parse_input() 和 mock_inference()
#pragma once
#include <iostream>
#include <vector>
using namespace std;

// 解析输入数字
bool parse_input(
    const std::string &body,
    std::vector<int32_t> &input);
// 执行模拟推理
int64_t mock_inference(
    const std::vector<int32_t> &input);
