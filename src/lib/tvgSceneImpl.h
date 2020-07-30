/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SCENE_IMPL_H_
#define _TVG_SCENE_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Scene::Impl
{
    vector<Paint*> paints;
    RenderTransform *rTransform = nullptr;
    uint32_t flag = RenderUpdateFlag::None;
    unique_ptr<Loader> loader = nullptr;

    ~Impl()
    {
        //Are you sure clear() prior to this?
        assert(paints.empty());
        if (rTransform) delete(rTransform);
    }

    bool dispose(RenderMethod& renderer)
    {
        for (auto paint : paints) {
            paint->IMPL->method()->dispose(renderer);
            delete(paint);
        }
        paints.clear();

        return true;
    }

    bool updateInternal(RenderMethod &renderer, const RenderTransform* transform, uint32_t flag)
    {
        for(auto paint: paints) {
            if (!paint->IMPL->method()->update(renderer, transform, flag))  return false;
        }
        return true;
    }

    bool update(RenderMethod &renderer, const RenderTransform* pTransform, uint32_t pFlag)
    {
        if (loader) {
            auto scene = loader->data();
            if (scene) {
                auto p = scene.release();
                if (!p) return false;
                paints.push_back(p);
                loader->close();
            }
        }

        if (flag & RenderUpdateFlag::Transform) {
            if (!rTransform) return false;
            if (!rTransform->update()) {
                delete(rTransform);
                rTransform = nullptr;
            }
        }

        auto ret = true;

        if (rTransform && pTransform) {
            RenderTransform outTransform(pTransform, rTransform);
            ret = updateInternal(renderer, &outTransform, pFlag | flag);
        } else {
            auto outTransform = pTransform ? pTransform : rTransform;
            ret = updateInternal(renderer, outTransform, pFlag | flag);
        }

        flag = RenderUpdateFlag::None;

        return ret;
    }

    bool render(RenderMethod &renderer)
    {
        for(auto paint: paints) {
            if(!paint->IMPL->method()->render(renderer)) return false;
        }
        return true;
    }

    bool bounds(float* px, float* py, float* pw, float* ph)
    {
        if (loader) {
            if (px) *px = loader->vx;
            if (py) *py = loader->vy;
            if (pw) *pw = loader->vw;
            if (ph) *ph = loader->vh;
        } else {
            auto x = FLT_MAX;
            auto y = FLT_MAX;
            auto w = 0.0f;
            auto h = 0.0f;

            for(auto paint: paints) {
                auto x2 = FLT_MAX;
                auto y2 = FLT_MAX;
                auto w2 = 0.0f;
                auto h2 = 0.0f;

                if (paint->IMPL->method()->bounds(&x2, &y2, &w2, &h2)) return false;

                //Merge regions
                if (x2 < x) x = x2;
                if (x + w < x2 + w2) w = (x2 + w2) - x;
                if (y2 < y) y = x2;
                if (y + h < y2 + h2) h = (y2 + h2) - y;
            }

            if (px) *px = x;
            if (py) *py = y;
            if (pw) *pw = w;
            if (ph) *ph = h;
        }
        return true;
    }

    bool scale(float factor)
    {
        if (rTransform) {
            if (fabsf(factor - rTransform->scale) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(factor) <= FLT_EPSILON) return true;
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->scale = factor;
        flag |= RenderUpdateFlag::Transform;

        return true;
    }

    bool rotate(float degree)
    {
        if (rTransform) {
            if (fabsf(degree - rTransform->degree) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(degree) <= FLT_EPSILON) return true;
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->degree = degree;
        flag |= RenderUpdateFlag::Transform;

        return true;
    }

    bool translate(float x, float y)
    {
        if (rTransform) {
            if (fabsf(x - rTransform->x) <= FLT_EPSILON && fabsf(y - rTransform->y) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON) return true;
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->x = x;
        rTransform->y = y;
        flag |= RenderUpdateFlag::Transform;

        return true;
    }


    bool transform(const Matrix& m)
    {
        if (!rTransform) {
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->override(m);
        flag |= RenderUpdateFlag::Transform;

        return true;
    }


    Result load(const string& path)
    {
        if (loader) loader->close();
        loader = LoaderMgr::loader(path.c_str());
        if (!loader || !loader->open(path.c_str())) return Result::NonSupport;
        if (!loader->read()) return Result::Unknown;
        return Result::Success;
    }
};

#endif //_TVG_SCENE_IMPL_H_