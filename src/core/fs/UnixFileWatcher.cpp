/*
    Copyright (C) 2014-2015 Islog

    This file is part of Leosac.

    Leosac is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leosac is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * \file unixfilewatcher.cpp
 * \author Thibault Schueller <ryp.sqrt@gmail.com>
 * \brief UnixFileWatcher class implementation
 */

#include "UnixFileWatcher.h"

extern "C" {
#include <sys/inotify.h>
#include <unistd.h>
}

#include <cstring>

static std::string getErrorString(const std::string& func, int errNo)
{
    return (func + "() failed (" + std::string(strerror(errNo)) + ")");
}

UnixFileWatcher::UnixFileWatcher()
:   _isRunning(false)
{
    if ((_inotifyFd = inotify_init()) == -1)
        throw (std::runtime_error(getErrorString("inotify_init", errno)));
}

UnixFileWatcher::~UnixFileWatcher()
{
    if (close(_inotifyFd) == -1)
        Assert(false, getErrorString("close", errno));
}

void UnixFileWatcher::start()
{
    _isRunning = true;
    _thread = std::thread([this] () {
        run();
    } );
}

void UnixFileWatcher::stop()
{
    _isRunning = false;
    _thread.join();
    for (auto watch : _watches)
    {
        if (inotify_rm_watch(_inotifyFd, watch.first) == -1)
            throw (std::runtime_error(getErrorString("inotify_rm_watch", errno)));
    }
    _watches.clear();
}

void UnixFileWatcher::watchFile(const std::string& path)
{
    std::uint32_t   mask = IN_CLOSE_WRITE;
    WatchParams     params;
    UnixFd          watch;

    if ((watch = inotify_add_watch(_inotifyFd, path.c_str(), mask)) == -1)
        throw (std::runtime_error(getErrorString("inotify_add_watch", errno)));
    params.path = path;
    params.mask = 0;
    _watches[watch] = params;
}

bool UnixFileWatcher::fileHasChanged(const std::string& path) const
{
    for (auto& param : _watches)
    {
        if (param.second.path == path)
            return ((param.second.mask & IN_CLOSE_WRITE) > 0);
    }
    throw (std::runtime_error("no registered watch for path:" + path));
}

void UnixFileWatcher::fileReset(const std::string& path)
{
    for (auto& param : _watches)
    {
        if (param.second.path == path)
        {
            param.second.mask = 0;
            return;
        }
    }
    throw (std::runtime_error("no registered watch for path:" + path));
}

std::size_t UnixFileWatcher::size() const
{
    return (_watches.size());
}

void UnixFileWatcher::run()
{
    struct timeval  timeoutStruct;
    int             ret;
    long            timeout = DefaultTimeoutMs;
    fd_set          readSet;
    std::size_t     buflen = 1024 * (sizeof(struct inotify_event) + 16);
    char            buf[buflen];
    int             len;
    inotify_event*  event;

    while (_isRunning)
    {
        FD_ZERO(&readSet);
        FD_SET(_inotifyFd, &readSet);
        timeoutStruct.tv_sec = timeout / 1000;
        timeoutStruct.tv_usec = (timeout % 1000) * 1000;
        if ((ret = ::select(_inotifyFd + 1, &readSet, nullptr, nullptr, &timeoutStruct)) == -1)
        {
            if (errno != EINTR)
                throw (std::runtime_error(getErrorString("select", errno)));
            else
                throw (std::runtime_error(getErrorString("select", errno)));
        }
        else if (ret > 0)
        {
            if (!FD_ISSET(_inotifyFd, &readSet))
                throw (std::runtime_error("unexpected file descriptor set"));
            if ((len = read(_inotifyFd, buf, buflen)) == -1)
            {
                if (errno != EINTR)
                    throw (std::runtime_error(getErrorString("read", errno)));
            }
            else if (!len)
                throw (std::runtime_error("nothing was read"));
            else
            {
                for (int i = 0; i < len;)
                {
                    event = reinterpret_cast<inotify_event*>(&buf[i]); // NOTE Alignment should not be an issue here
                    _watches.at(event->wd).mask |= event->mask;
                    i += sizeof(inotify_event) + event->len;
                }
            }
        }
    }
}
