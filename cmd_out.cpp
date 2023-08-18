#include "cmd_out.hpp"
#include <_types/_uint64_t.h>
#include <sys/_types/_int64_t.h>

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>

using namespace gtea;

std::mutex                               CmdOut::s_mutex;
std::unique_ptr<CmdOut::ProgressBar>     CmdOut::s_bar;

void CmdOut::createProgressBar(const ProgressBarInfo &info)
{
    std::unique_lock<std::mutex> lck{s_mutex};
    if(s_bar)
    {
        s_bar = nullptr;
    }

    if(info.total <= 0)
    {
        std::cout << "ProgressBar total size error" << std::endl;
        return;
    }

    s_bar = std::make_unique<ProgressBar>(info);
}

void CmdOut::destroyProgressBar()
{
    std::unique_lock<std::mutex> lck{s_mutex};
    s_bar = nullptr;
}

void CmdOut::updateProgressBar(int value)
{
    std::unique_lock<std::mutex> lck{s_mutex};
    if(s_bar)
    {
        s_bar->update(value);
        if(s_bar->isCompleted())
        {
            s_bar = nullptr;
        }
    }
    else
    {
        std::cout << "no ProgressBar!" << std::endl;
    }
}

void CmdOut::print(const char *fmt, ...)
{
    char    buf[2048] = {0}; 
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    std::unique_lock<std::mutex> lck{s_mutex};
    if(s_bar)
    {
        rollback();
    }

    std::cout << buf << std::endl;

    if(s_bar)
    {
        s_bar->forceShow();
    }
}

void CmdOut::rollback()
{
    std::cout << "\r\033[K";
}

CmdOut::ProgressBar::ProgressBar(const ProgressBarInfo &info) : _info{info} 
{
    update(0);
}

CmdOut::ProgressBar::~ProgressBar()
{
    std::cout << std::endl;
}

void CmdOut::ProgressBar::update(int value)
{
    if(value < 0)
    {
        return;
    }

    if (_info.total == 0)
    {
        return;
    }

    if (_completed)
    {
        return;
    }

    if (value >= _info.total)
    {
        _completed = true;
        value = _info.total;
    }

    std::string cur_content = generateBarContent(value);
    if(cur_content == _last_content)
    {
        return;
    }

    _last_content = cur_content;

    rollback();
    std::cout << cur_content << std::flush;

    // std::cout << std::flush; 
}

void CmdOut::ProgressBar::forceShow()
{
    std::cout << _last_content << std::flush;
}

std::string CmdOut::ProgressBar::generateBarContent(int value)
{
    std::string ret;

    ret.push_back(' ');
    if(_info.title.size() > 0 && _info.title_show)
    {
        ret.append(_info.title);
    }

    if(_info.bar_show && _info.bar_len > 0)
    {
        int done_cnt = (int64_t)_info.bar_len * (int64_t)value / (int64_t)_info.total;
        int todo_cnt = _info.bar_len - done_cnt;

        ret.push_back(' ');
        ret.push_back('[');
        for(int i = 0; i < done_cnt; i++)
        {
            ret.push_back(_done_flag);
        }
        for(int i = 0; i < todo_cnt; i++)
        {
            ret.push_back(_todo_flag);
        }
        ret.push_back(']');
    }

    if(_info.percent_show)
    {
        ret.push_back(' ');
        int percent = (int64_t)value * 100 / (int64_t)_info.total;
        ret.append(std::to_string(percent));
        ret.push_back('%');
    }

    return ret;
}

