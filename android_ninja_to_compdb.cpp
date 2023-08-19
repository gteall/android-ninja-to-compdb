#include <dirent.h>
#include <sys/signal.h>
#include <unistd.h>
#include <termios.h>    // struct termios, tcgetattr(), tcsetattr()
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include "cmd_out.hpp"

using std::string;

static constexpr int PATH_LEN = 256;
static constexpr int JSON_UNIT_SIZE = 256 * 1024;
static const std::string COMPDB_FILE_NAME{"compile_commands.json"};


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

std::string generate_json_unit(const string &dir, const string &cmd, const string &opts)
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

            std::ifstream stream_extra;
            stream_extra.open(tmp, std::ios::in);

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

class InputFile
{
public:
    InputFile(const std::string file)
    {
        _stream.open(file, std::ios::in);
        if(!_stream.is_open())
        {
            gtea::CmdOut::print("open ifile %s failed", file.c_str());
            return;
        }

        _stream.seekg(0, std::ios_base::end);

        gtea::CmdOut::ProgressBarInfo bar_info;
        bar_info.total = _stream.tellg();
        gtea::CmdOut::createProgressBar(bar_info);

        _stream.seekg(0, std::ios_base::beg);

    }

    ~InputFile()
    {
        if(_stream.is_open())
        {
            _stream.close();
        }
    }

    bool getLine(std::string &str)
    {
        bool ret = false;
        std::unique_lock<std::mutex> lck(_mutex);
        if(_stream.is_open() && getline(_stream, str))
        {
            _done_size += str.size() + 1;
            gtea::CmdOut::updateProgressBar(_done_size);
            ret = true;
        }
        return ret;
    }

private:
    std::ifstream    _stream;
    std::mutex  _mutex;
    int         _done_size{0};
};

class OutputFile
{
public:
    OutputFile(const string &filename)
    {
        _ostream.open(filename);
        if(!_ostream.is_open())
        {
            gtea::CmdOut::print("open ofile %s failed!", filename.c_str());        
        }
        else
        {
            _ostream << '[' << std::endl;        
        }
    }

    ~OutputFile()
    {
        if(_ostream.is_open())
        {
            _ostream << std::endl << ']' << std::endl;        
            _ostream.close();
        }
    }

    void write(const std::string &str)
    {
        std::unique_lock<std::mutex> lck(_mutex);
        if (_ostream.is_open())
        {
            if (_first_flag)
            {
                _first_flag = false;
            }
            else
            {
                _ostream << "," << std::endl;
            }
            _ostream << str;
        }
    }

private:
    std::ofstream    _ostream;
    std::mutex  _mutex;
    bool        _first_flag{true};
};


int main(int argc, char *argv[])
{
    bool running_flag = true;
    int cnt = 0;
    int thread_num = 32; //常用服务器是64核的 默认开32个线程

    if (argc < 2)
    {
        gtea::CmdOut::print("params error!");
        return -1;
    }

    if(argc >= 3)
    {
        thread_num = atoi(argv[2]);
    }

    std::array<char, PATH_LEN> cur_path;
    getcwd(cur_path.data(), cur_path.size());
    string dir{cur_path.data()};
    // cout << cur_path << endl;

    OutputFile  ofile{COMPDB_FILE_NAME};
    InputFile   ifile{argv[1]};

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

    const auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    for(int i = 0; i < thread_num; i++)
    {
        std::thread thr([&](int index)
        {
            int cnt = 0;
            std::chrono::duration<double> difftime;
            std::string cur_line;
            while(ifile.getLine(cur_line))
            {
                const auto start = std::chrono::high_resolution_clock::now();
                std::smatch result;
                std::regex pattern(
                    "^ command = /bin/bash -c \"PWD=/proc/self/cwd.* "
                    "(\\S*/bin/(clang|clang\\+\\+)) (.*\\.(cpp|c))\"$");
                if (std::regex_match(cur_line, result, pattern))
                {
                    ofile.write(generate_json_unit(dir, result[1], result[3]));
                }
                cnt++;

                const auto end = std::chrono::high_resolution_clock::now();
                difftime += (end - start);

            }

            gtea::CmdOut::print("thread %d parse cnt %d %lfs", index, cnt, difftime.count());
        }, i);
        threads.push_back(std::move(thr));
    }

    for(auto &it : threads)
    {
        if(it.joinable())
        {
            it.join();
        }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    // const std::chrono::duration<double, std::milli> elapsed = end - start;
    const std::chrono::duration<double> elapsed = end - start;
 
    // std::cout << elapsed.count() << "s" << std::endl;
    gtea::CmdOut::print("used time : %lfs", elapsed.count());

    return 0;
}
