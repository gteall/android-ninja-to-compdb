#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>
// #include "log.hpp"

namespace gtea{


class CmdOut
{
public:
    struct ProgressBarInfo
    {
        std::string title;
        int         title_show{0};  //0:不显示 1:显示

        int         bar_len{100};   //进度条长度
        int         bar_show{1};    //0:不显示 1:显示

        int         percent_show{1};//0:不显示 1:显示

        int         total{0};       //must > 0

    };

    static void createProgressBar(const ProgressBarInfo &info);
    static void destroyProgressBar();

    static void updateProgressBar(int value);

    static void print(const char *fmt, ...);

private:
    static void rollback();

    class ProgressBar
    {
    public:
        ProgressBar(const ProgressBarInfo &info);
        ~ProgressBar();

        void    update(int value);
        void    forceShow();

        bool    isCompleted()
        {
            return _completed;
        }

    private:
        std::string     generateBarContent(int value);

        ProgressBarInfo     _info;

        std::string         _last_content;
        // std::string _title;
        // uint32_t    _total{0};
        bool        _completed{false};
        // uint32_t    _parts_num{100};
        // uint32_t    _last_completed_parts{0};
        // uint32_t    _last_percent{0};
        // bool        _first_flag{true};

        char                _done_flag{'#'};
        char                _todo_flag{'-'};
        std::vector<char>   _doing_flag{'\\', '|', '/','-'};
        int                 _doing_index{0};
    };

    static std::mutex                       s_mutex;
    static std::unique_ptr<ProgressBar>     s_bar;
};

}

