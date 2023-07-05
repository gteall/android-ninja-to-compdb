#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>
#include <array>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::ifstream;
using std::ofstream;
using std::ios;

static constexpr int PATH_LEN = 256;
static constexpr int JSON_UNIT_SIZE = 256 * 1024;
static const string COMPDB_FILE_NAME_NAME{"compile_commands.json"};

string generate_json_unit(const string &dir, const string &cmd, const string &opts)
{
    static int s_cnt{0};
    string ret;

    ret.reserve(JSON_UNIT_SIZE);

    //dir
    ret.append("  {\n");
    ret.append(R"(    "directory": ")").append(dir).append("\",\n");

    //cmd
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

    //file
    ret.append(R"(    "file": ")").append(strs.back()).append("\"\n");
    ret.append("  }");

    cout << ++s_cnt << " ===> " << strs.back() << endl;
    return ret;
}

int main(int argc, char *argv[])
{
    bool first_cmd_flag = true;
    int cnt = 0;

    if (argc < 2)
    {
        cerr << "params error" << endl;
        return -1;
    }

    std::array<char, PATH_LEN> cur_path;
    getcwd(cur_path.data(), cur_path.size());
    string dir{cur_path.data()};
    // cout << cur_path << endl;

    string COMPDB_FILE_NAME{"compile_commands.json"};
    ofstream stream_out(COMPDB_FILE_NAME);
    if (!stream_out.is_open())
    {
        cerr << "open file " << COMPDB_FILE_NAME << " failed!" << endl;
        return -1;
    }

    string ninja_file{argv[1]};
    ifstream stream_in(ninja_file, ios::in);
    if (!stream_in.is_open())
    {
        cerr << "open file " << ninja_file << " failed!" << endl;
        return -1;
    }

    stream_out << "[" << endl;

    string cur_line;
    while (getline(stream_in, cur_line))
    {
        std::smatch result;
        std::regex pattern("^ command = /bin/bash -c \"PWD=/proc/self/cwd.* "
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
                stream_out << "," << endl;  // 非首个cmd给上个cmd的}后增加一个,分隔符
            }

            stream_out << generate_json_unit(dir, result[1], result[3]);
        }
    }

    stream_out << endl << "]" << endl;
    return 0;
}

