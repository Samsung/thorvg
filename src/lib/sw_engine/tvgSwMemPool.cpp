/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <vector>
#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static vector<SwOutline> outline;
static vector<SwOutline> strokeOutline;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwOutline* mpoolReqOutline(unsigned idx)
{
    return &outline[idx];
}


void mpoolRetOutline(unsigned idx)
{
    outline[idx].cntrsCnt = 0;
    outline[idx].ptsCnt = 0;
}


SwOutline* mpoolReqStrokeOutline(unsigned idx)
{
    return &strokeOutline[idx];
}


void mpoolRetStrokeOutline(unsigned idx)
{
    strokeOutline[idx].cntrsCnt = 0;
    strokeOutline[idx].ptsCnt = 0;
}


bool mpoolInit(unsigned threads)
{
    if (threads == 0) threads = 1;

    outline.reserve(threads);
    outline.resize(threads);

    for (auto& outline : outline) {
        outline.cntrs = nullptr;
        outline.pts = nullptr;
        outline.types = nullptr;
        outline.cntrsCnt = outline.reservedCntrsCnt = 0;
        outline.ptsCnt = outline.reservedPtsCnt = 0;
    }

    strokeOutline.reserve(threads);
    strokeOutline.resize(threads);

    for (auto& outline : strokeOutline) {
        outline.cntrs = nullptr;
        outline.pts = nullptr;
        outline.types = nullptr;
        outline.cntrsCnt = outline.reservedCntrsCnt = 0;
        outline.ptsCnt = outline.reservedPtsCnt = 0;
    }

    return true;
}


bool mpoolClear()
{
    for (auto& outline : outline) {
        if (outline.cntrs) {
            free(outline.cntrs);
            outline.cntrs = nullptr;
        }
        if (outline.pts) {
            free(outline.pts);
            outline.pts = nullptr;
        }
        if (outline.types) {
            free(outline.types);
            outline.types = nullptr;
        }
        outline.cntrsCnt = outline.reservedCntrsCnt = 0;
        outline.ptsCnt = outline.reservedPtsCnt = 0;
    }

    for (auto& outline : strokeOutline) {
        if (outline.cntrs) {
            free(outline.cntrs);
            outline.cntrs = nullptr;
        }
        if (outline.pts) {
            free(outline.pts);
            outline.pts = nullptr;
        }
        if (outline.types) {
            free(outline.types);
            outline.types = nullptr;
        }
        outline.cntrsCnt = outline.reservedCntrsCnt = 0;
        outline.ptsCnt = outline.reservedPtsCnt = 0;
    }

    return true;
}


bool mpoolTerm()
{
    return mpoolClear();
}