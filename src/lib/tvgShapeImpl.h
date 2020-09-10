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
#ifndef _TVG_SHAPE_IMPL_H_
#define _TVG_SHAPE_IMPL_H_

#include "tvgCommon.h"
#include "tvgShapePath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ShapeStroke
{
    float width = 0;
    uint8_t color[4] = {0, 0, 0, 0};
    float* dashPattern = nullptr;
    uint32_t dashCnt = 0;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;

    ~ShapeStroke()
    {
        if (dashPattern) free(dashPattern);
    }
};


struct Shape::Impl
{
    ShapePath *path = nullptr;
    Fill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a
    void *edata = nullptr;              //engine data
    Shape *shape = nullptr;
    uint32_t flag = RenderUpdateFlag::None;

    Impl(Shape* s) : path(new ShapePath), shape(s)
    {
    }

    ~Impl()
    {
        if (path) delete(path);
        if (fill) delete(fill);
        if (stroke) delete(stroke);
    }

    bool dispose(RenderMethod& renderer)
    {
        return renderer.dispose(*shape, edata);
    }

    bool render(RenderMethod& renderer)
    {
        return renderer.render(*shape, edata);
    }

    bool update(RenderMethod& renderer, const RenderTransform* transform, RenderUpdateFlag pFlag)
    {
        edata = renderer.prepare(*shape, edata, transform, static_cast<RenderUpdateFlag>(pFlag | flag));

        flag = RenderUpdateFlag::None;

        if (edata) return true;
        return false;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!path) return false;
        return path->bounds(x, y, w, h);
    }

    bool strokeWidth(float width)
    {
        //TODO: Size Exception?

        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->width = width;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeCap(StrokeCap cap)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->cap = cap;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeJoin(StrokeJoin join)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->join = join;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->color[0] = r;
        stroke->color[1] = g;
        stroke->color[2] = b;
        stroke->color[3] = a;

        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeDash(const float* pattern, uint32_t cnt)
    {
       if (!stroke) stroke = new ShapeStroke();
       if (!stroke) return false;

        if (stroke->dashCnt != cnt) {
            if (stroke->dashPattern) free(stroke->dashPattern);
            stroke->dashPattern = nullptr;
        }

        if (!stroke->dashPattern) stroke->dashPattern = static_cast<float*>(malloc(sizeof(float) * cnt));

        for (uint32_t i = 0; i < cnt; ++i)
            stroke->dashPattern[i] = pattern[i];

        stroke->dashCnt = cnt;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool duplicate(Shape *from) {
        if (!from || !from->pImpl) return false;

        if (from->pImpl->stroke) {
            if (stroke) delete(stroke);
            stroke = nullptr;

            stroke = new ShapeStroke();
            if (!stroke) return false;

            stroke->cap = from->pImpl->stroke->cap;
            stroke->join = from->pImpl->stroke->join;
            stroke->width = from->pImpl->stroke->width;

            memcpy(stroke->color, from->pImpl->stroke->color, sizeof(stroke->color));

            if (from->pImpl->stroke->dashPattern &&
                from->pImpl->stroke->dashCnt > 0) {
                stroke->dashCnt = from->pImpl->stroke->dashCnt;
                stroke->dashPattern = static_cast<float*> (malloc(sizeof(float) * stroke->dashCnt));
                memcpy(stroke->dashPattern, from->pImpl->stroke->dashPattern,
                       sizeof(float) * stroke->dashCnt);
            }

            flag |= RenderUpdateFlag::Stroke;
        }

        if (from->pImpl->path) {
            //duplicate override all stored shapes data
            if (path) delete path;
            path = nullptr;

            path = new ShapePath();
            if (!path) return false;

            path->cmdCnt = from->pImpl->path->cmdCnt;
            path->ptsCnt = from->pImpl->path->ptsCnt;

            path->reservedCmdCnt = from->pImpl->path->reservedCmdCnt;
            path->reservedPtsCnt = from->pImpl->path->reservedPtsCnt;

            path->cmds = static_cast<PathCommand*>(malloc(sizeof(PathCommand) * path->reservedCmdCnt));
            path->pts = static_cast<Point*>(malloc(sizeof(Point) * path->reservedPtsCnt));

            memcpy(path->cmds, from->pImpl->path->cmds, sizeof(PathCommand) * path->cmdCnt);
            memcpy(path->pts, from->pImpl->path->pts, sizeof(Point) * path->ptsCnt);

            flag |= RenderUpdateFlag::Path;
        }

        if (memcmp(color, from->pImpl->color, sizeof(color)))
        {
            memcpy(color, from->pImpl->color, sizeof(color));
            flag |= RenderUpdateFlag::Color;
        }

        return true;
    }
};

#endif //_TVG_SHAPE_IMPL_H_
