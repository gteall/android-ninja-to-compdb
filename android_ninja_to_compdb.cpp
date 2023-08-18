#include <dirent.h>
#include <sys/signal.h>
#include <unistd.h>
#include <termios.h>    // struct termios, tcgetattr(), tcsetattr()
#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include "cmd_out.hpp"

using std::endl;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::string;

static constexpr int PATH_LEN = 256;
static constexpr int JSON_UNIT_SIZE = 256 * 1024;
static const string COMPDB_FILE_NAME{"compile_commands.json"};

namespace
{
ofstream s_stream_out;
}  // namespace


void set_key_mode()
{
    struct termios attr;

    // Set terminal to single character mode.
    tcgetattr(fileno(stdin), &attr);
    attr.c_lflag &= (~ICANON & ~ECHO);
    attr.c_cc[VTIME] = 0;
    attr.c_cc[VMIN] = 1;
    if (tcsetattr(fileno(stdin), TCSANOW, &attr) < 0)
    {
        gtea::CmdOut::print("Unable to set terminal to single character mode");
    }
}

string generate_json_unit(const string &dir, const string &cmd, const string &opts)
{
    static int s_cnt{0};
    string ret;

    ret.reserve(JSON_UNIT_SIZE);

    // dir
    ret.append("  {\n");
    ret.append(R"(    "directory": ")").append(dir).append("\",\n");

    // cmd
    std::vector<string> strs;
    std::regex ws_re("\\s+");  // 空白符

    // std::copy( std::sregex_token_iterator(input_str.begin(),
    // input_str.end(), ws_re, -1),
    //            std::sregex_token_iterator(),
    //            strs.begin());
    bool cat_flag = false;
    for (auto itor = std::sregex_token_iterator(opts.begin(), opts.end(), ws_re, -1);
         itor != std::sregex_token_iterator(); itor++)
    {
        if (itor->compare("-fdebug-prefix-map=\\$$PWD/=") == 0)
        {
            continue;
        }

        if (itor->compare("\\$$(cat") == 0)
        {
            cat_flag = true;
            continue;
        }

        if (cat_flag)
        {
            cat_flag = false;
            string tmp = *itor;
            tmp.pop_back();

            ifstream stream_extra;
            stream_extra.open(tmp, ios::in);

            string tmp_line;
            string tmp_total_str;
            while (getline(stream_extra, tmp_line))
            {
                tmp_total_str.append(tmp_line).append(" ");
            }

            for (auto it2 = std::sregex_token_iterator(tmp_total_str.begin(), tmp_total_str.end(), ws_re, -1);
                 it2 != std::sregex_token_iterator(); it2++)
            {
                strs.push_back(*it2);
            }
            continue;
        }
        strs.push_back(*itor);
    }

    ret.append(R"(    "command": ")").append(cmd);
    for (auto &itor : strs)
    {
        ret.append(" ").append(itor);
    }
    ret.append("\",\n");

    // file
    ret.append(R"(    "file": ")").append(strs.back()).append("\"\n");
    ret.append("  }");

    // gtea::CmdOut::print("%d ===> %s", ++s_cnt, strs.back().c_str());
    return ret;
}

uint32_t get_file_size(const std::string &file)
{
    uint32_t ret = 0;
    struct stat file_stat;
    if(0 == stat(file.c_str(), &file_stat))
    {
        ret = file_stat.st_size;
    }
    return ret;
}

int main(int argc, char *argv[])
{
    bool running_flag = true;
    bool first_cmd_flag = true;
    int cnt = 0;

    if (argc < 2)
    {
        gtea::CmdOut::print("params error!");
        return -1;
    }

    std::array<char, PATH_LEN> cur_path;
    getcwd(cur_path.data(), cur_path.size());
    string dir{cur_path.data()};
    // cout << cur_path << endl;

    // string COMPDB_FILE_NAME{"compile_commands.json"};
    // ofstream stream_out(COMPDB_FILE_NAME);
    // if (!stream_out.is_open())
    // {
    //     cerr << "open file " << COMPDB_FILE_NAME << " failed!" << endl;
    //     return -1;
    // }
    s_stream_out.open(COMPDB_FILE_NAME);
    if (!s_stream_out.is_open())
    {
        gtea::CmdOut::print("open file %s failed!", COMPDB_FILE_NAME.c_str());
        return -1;
    }

    string ninja_file{argv[1]};
    ifstream stream_in(ninja_file, ios::in);
    if (!stream_in.is_open())
    {
        gtea::CmdOut::print("open file %s failed!", ninja_file.c_str());
        return -1;
    }

    //按q键中断ninja文件解析
    std::thread key_handler{[&running_flag](){
        // Read single characters from cin.
        set_key_mode();
        std::streambuf *pbuf = std::cin.rdbuf();
        // cout << "Enter q or Q to quit " << endl;
        gtea::CmdOut::print("Enter q or Q to quit!");
        while (running_flag) {
            char c;
            c = static_cast<char>(pbuf->sbumpc());
            if (c == 'q' || c == 'Q') {
                gtea::CmdOut::print("quit!");
                running_flag = false;
            } 
        }
    }};
    key_handler.detach();

    s_stream_out << "[" << endl;

    int file_size = get_file_size(ninja_file);
    gtea::CmdOut::ProgressBarInfo bar_info;
    bar_info.total = file_size;
    gtea::CmdOut::createProgressBar(bar_info);
    // gtea::ProgressBar bar{"", file_size};
    int cur_size = 0;

    string cur_line;
    while (running_flag && getline(stream_in, cur_line))
    {
        cur_size += cur_line.size() + 1;
        gtea::CmdOut::updateProgressBar(cur_size);
        // bar.update(cur_size);
        // std::cout << file_size << " : " << cur_size << std::endl;

        std::smatch result;
        std::regex pattern(
            "^ command = /bin/bash -c \"PWD=/proc/self/cwd.* "
            "(\\S*/bin/(clang|clang\\+\\+)) (.*\\.(cpp|c))\"$");
        if (std::regex_match(cur_line, result, pattern))
        {
            // for regex debug
            //  for(int i = 0; i < result.size(); i++)
            //  {
            //    cout << "result["  << i << "] = " << result[i] << endl;
            //  }

            if (first_cmd_flag)
            {
                first_cmd_flag = false;
            }
            else
            {
                s_stream_out << "," << endl;  // 非首个cmd给上个cmd的}后增加一个,分隔符
            }

            s_stream_out << generate_json_unit(dir, result[1], result[3]);

            // debug
            // std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    s_stream_out << endl << "]" << endl;
    return 0;
}
