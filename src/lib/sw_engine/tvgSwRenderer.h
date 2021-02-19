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
#ifndef _TVG_SW_RENDERER_H_
#define _TVG_SW_RENDERER_H_

#include "tvgRender.h"

struct SwSurface;
struct SwTask;
struct SwCompositor;

namespace tvg
{

class SwRenderer : public RenderMethod
{
public:
    RenderData prepare(const Shape& shape, RenderData data, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flags) override;
    RenderData prepare(const Picture& picture, RenderData data, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flags) override;
    bool preRender() override;
    bool renderShape(RenderData data) override;
    bool renderImage(RenderData data) override;
    bool postRender() override;
    bool dispose(RenderData data) override;
    bool region(RenderData data, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) override;

    bool clear() override;
    bool sync() override;
    bool target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs);

    Compositor* target(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
    bool beginComposite(Compositor* cmp, CompositeMethod method, uint32_t opacity) override;
    bool endComposite(Compositor* cmp) override;

    static SwRenderer* gen();
    static bool init(uint32_t threads);
    static bool term();

private:
    SwSurface*           surface = nullptr;           //active surface
    Array<SwTask*>       tasks;                       //async task list
    Array<SwSurface*>    compositors;                 //render targets cache list

    SwRenderer(){};
    ~SwRenderer();

    RenderData prepareCommon(SwTask* task, const RenderTransform* transform, uint32_t opacity, const Array<RenderData>& clips, RenderUpdateFlag flags);
};

}

#endif /* _TVG_SW_RENDERER_H_ */
