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
#ifndef _TVG_PICTURE_IMPL_H_
#define _TVG_PICTURE_IMPL_H_

#include <string>
#include "tvgPaint.h"
#include "tvgLoaderMgr.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Picture::Impl
{
    unique_ptr<Loader> loader = nullptr;
    Paint* paint = nullptr;
    uint32_t *pixels = nullptr;
    Picture *picture = nullptr;
    void *edata = nullptr;              //engine data

    uint32_t w = 0, h = 0;
    bool resizing = false;

    Impl(Picture* p) : picture(p)
    {
    }

    bool dispose(RenderMethod& renderer)
    {
        if (paint) {
            paint->pImpl->dispose(renderer);
            delete(paint);

            return true;
        }
        else if (pixels) {
            return renderer.dispose(edata);
        }
        return false;
    }

    void resize()
    {
        uint32_t w = 0, h = 0;
        if (this->w != 0 && this->h != 0) {
            w = this->w;
            h = this->h;
        }
        else if (loader->w != 0 && loader->h != 0) {
            w = loader->w;
            h = loader->h;
        }
        if (w != 0 && h != 0) {
            auto sx = w / (loader->vw + (loader->vx > 0 ? loader->vx : -1 * loader->vx));
            auto sy = h / (loader->vh + (loader->vy > 0 ? loader->vy : -1 * loader->vy));
            if (!loader->preserveAspect) {
                Matrix m = {sx,  0, -loader->vx,
                             0, sy, -loader->vy,
                             0,  0, 1};
                paint->transform(m);
            }
            else {
                auto scale = sx < sy ? sx : sy;
                paint->translate(((w - loader->vw) * scale) / 2.0, ((h - loader->vh) * scale) / 2.0);
                paint->scale(scale);
                paint->translate(-loader->vx, -loader->vy);
            }
            resizing = false;
        }
    }

    uint32_t reload()
    {
        if (loader) {
            if (!paint) {
                auto scene = loader->scene();
                if (scene) {
                    paint = scene.release();
                    resizing = true;
                    loader->close();
                    if (paint) return RenderUpdateFlag::None;
                }
            }
            if (!pixels) {
                pixels = (uint32_t*)loader->pixels();
                if (pixels) return RenderUpdateFlag::Image;
            }
        }
        return RenderUpdateFlag::None;
    }

    void* update(RenderMethod &renderer, const RenderTransform* transform, uint32_t opacity, vector<Composite>& compList, RenderUpdateFlag pFlag)
    {
        uint32_t flag = reload();

        if (pixels) edata = renderer.prepare(*picture, edata, pixels, transform, opacity, compList, static_cast<RenderUpdateFlag>(pFlag | flag));
        else if (paint) {
            if (resizing) resize();
            edata = paint->pImpl->update(renderer, transform, opacity, compList, static_cast<RenderUpdateFlag>(pFlag | flag));
        }
        return edata;
    }

    bool render(RenderMethod &renderer)
    {
        if (pixels) return renderer.render(*picture, edata);
        else if (paint) return paint->pImpl->render(renderer);
        return false;
    }

    bool viewbox(float* x, float* y, float* w, float* h)
    {
        if (!loader) return false;
        if (x) *x = loader->vx;
        if (y) *y = loader->vy;
        if (w) *w = loader->vw;
        if (h) *h = loader->vh;
        return true;
    }

    bool size(uint32_t w, uint32_t h)
    {
        this->w = w;
        this->h = h;
        resizing = true;
        return true;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!paint) return false;
        return paint->pImpl->bounds(x, y, w, h);
    }

    Result load(const string& path)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(path);
        if (!loader) return Result::NonSupport;
        if (!loader->read()) return Result::Unknown;
        return Result::Success;
    }

    Result load(const char* data, uint32_t size)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(data, size);
        if (!loader) return Result::NonSupport;
        if (!loader->read()) return Result::Unknown;
        return Result::Success;
    }

    Result load(uint32_t* data, uint32_t w, uint32_t h, bool copy)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(data, w, h, copy);
        if (!loader) return Result::NonSupport;
        return Result::Success;
    }

    Paint* duplicate()
    {
        reload();

        if (!paint) return nullptr;
        auto ret = Picture::gen();
        if (!ret) return nullptr;
        auto dup = ret.get()->pImpl;
        dup->paint = paint->duplicate();

        return ret.release();
    }
};

#endif //_TVG_PICTURE_IMPL_H_
