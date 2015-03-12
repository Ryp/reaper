/*
 * Copyright � 2012-2013 Graham Sellers
 *
 * This code is part of the OpenGL SuperBible, 6th Edition.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __SB6_H__
#define __SB6_H__

#include "pipeline/gl.hpp"
#include "glcontext.hpp"

#include <stdio.h>
#include <string.h>

namespace sb6
{
    class application
    {
    public:
        application() {}
        virtual ~application() {}
        virtual void run(GLContext& ctx)
        {
            info.windowWidth = ctx.getWindowSize().x;
            info.windowHeight = ctx.getWindowSize().x;
            startup();
            do
            {
                render(ctx.getTime());
                ctx.swapBuffers();

            } while(ctx.isOpen());
        }
        virtual void startup() = 0;
        virtual void render(double currentTime) = 0;
        virtual void onKey(int key, int action) = 0;

    public:
        struct APPINFO
        {
            char title[128];
            int windowWidth;
            int windowHeight;
            int majorVersion;
            int minorVersion;
            int samples;
            union
            {
                struct
                {
                    unsigned int    fullscreen  : 1;
                    unsigned int    vsync       : 1;
                    unsigned int    cursor      : 1;
                    unsigned int    stereo      : 1;
                    unsigned int    debug       : 1;
                };
                unsigned int        all;
            } flags;
        };

    protected:
        APPINFO     info;
    };
};

#endif /* __SB6_H__ */

