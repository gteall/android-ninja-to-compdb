#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

using namespace std;

int main(int argc, char *argv[])
{
    bool first_cmd_flag = true;
    int cnt = 0;

    if (argc < 2)
    {
        cerr << "params error" << endl;
        return -1;
    }

    char cur_path[512] = {0};
    getcwd(cur_path, sizeof(cur_path));
    // cout << cur_path << endl;

    string compdb_file{"compile_commands.json"};
    ofstream stream_out(compdb_file);
    if (stream_out.is_open() == false)
    {
        cerr << "open file " << compdb_file << " failed!" << endl;
        return -1;
    }

    string ninja_file{argv[1]};
    ifstream stream_in(ninja_file, ios::in);
    if (stream_in.is_open() == false)
    {
        cerr << "open file " << ninja_file << " failed!" << endl;
        return -1;
    }

    stream_out << "[" << endl;

    string cur_line;
    while (getline(stream_in, cur_line))
    {
        std::smatch result;
        std::regex pattern(
            "^ command = /bin/bash -c \"PWD=/proc/self/cwd.* "
            "(\\S*/bin/(clang|clang\\+\\+)) (.*\\.(cpp|c))\"$");
        // "prebuilts/\\S+ (.*\\.(cpp|c))\"$");
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
            stream_out << "  {" << endl;
            stream_out << "    \"directory\": \"" << cur_path << "\"," << endl;

            string clang_path{result[1]};
            string input_str{result[3]};

            std::vector<string> strs;
            std::regex ws_re("\\s+");  // 空白符

            // std::copy( std::sregex_token_iterator(input_str.begin(),
            // input_str.end(), ws_re, -1),
            //            std::sregex_token_iterator(),
            //            strs.begin());
            bool cat_flag = false;
            for (auto it = std::sregex_token_iterator(input_str.begin(), input_str.end(), ws_re, -1);
                 it != std::sregex_token_iterator(); it++)
            {
                if (it->compare("-fdebug-prefix-map=\\$$PWD/=") == 0)
                {
                    continue;
                }

                if (it->compare("\\$$(cat") == 0)
                {
                    cat_flag = true;
                    continue;
                }

                if (cat_flag)
                {
                    cat_flag = false;
                    string tmp = *it;
                    tmp.pop_back();
                    // cout << tmp << endl;

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
                        // cout << *it2 << endl;
                        strs.push_back(*it2);
                    }
                    continue;
                }
                strs.push_back(*it);
            }

            stream_out << "    \"command\": \"" << clang_path;
            for (auto &it : strs)
            {
                stream_out << " " << it;
            }
            stream_out << "\"," << endl;

            stream_out << "    \"file\": \"" << strs.back() << "\"" << endl;
            stream_out << "  }";

            cout << ++cnt << " ===> " << strs.back() << endl;
        }
    }

    stream_out << endl << "]" << endl;
    return 0;
}
