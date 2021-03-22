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
#include <setjmp.h>
#include <limits.h>
#include <memory.h>
#include <iostream>
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

constexpr auto MAX_SPANS = 256;
constexpr auto PIXEL_BITS = 8;   //must be at least 6 bits!
constexpr auto ONE_PIXEL = (1L << PIXEL_BITS);

using Area = long;

struct Band
{
    SwCoord min, max;
};

struct Cell
{
    SwCoord x;
    SwCoord cover;
    Area area;
    Cell *next;
};

struct RleWorker
{
    SwRleData* rle;

    SwPoint cellPos;
    SwPoint cellMin;
    SwPoint cellMax;
    SwCoord cellXCnt;
    SwCoord cellYCnt;

    Area area;
    SwCoord cover;

    Cell* cells;
    ptrdiff_t maxCells;
    ptrdiff_t cellsCnt;

    SwPoint pos;

    SwPoint bezStack[32 * 3 + 1];
    int levStack[32];

    SwOutline* outline;

    SwSpan spans[MAX_SPANS];
    int spansCnt;
    int ySpan;

    int bandSize;
    int bandShoot;

    jmp_buf jmpBuf;

    void* buffer;
    long bufferSize;

    Cell** yCells;
    SwCoord yCnt;

    SwSize clip;

    bool invalid;
    bool antiAlias;
};


static inline SwPoint UPSCALE(const SwPoint& pt)
{
    return {pt.x << (PIXEL_BITS - 6), pt.y << (PIXEL_BITS - 6)};
}


static inline SwPoint TRUNC(const SwPoint& pt)
{
    return  {pt.x >> PIXEL_BITS, pt.y >> PIXEL_BITS};
}


static inline SwCoord TRUNC(const SwCoord x)
{
    return  x >> PIXEL_BITS;
}


static inline SwPoint SUBPIXELS(const SwPoint& pt)
{
    return {pt.x << PIXEL_BITS, pt.y << PIXEL_BITS};
}


static inline SwCoord SUBPIXELS(const SwCoord x)
{
    return (x << PIXEL_BITS);
}

/*
 *  Approximate sqrt(x*x+y*y) using the `alpha max plus beta min'
 *  algorithm.  We use alpha = 1, beta = 3/8, giving us results with a
 *  largest error less than 7% compared to the exact value.
 */
static inline SwCoord HYPOT(SwPoint pt)
{
    if (pt.x < 0) pt.x = -pt.x;
    if (pt.y < 0) pt.y = -pt.y;
    return ((pt.x > pt.y) ? (pt.x + (3 * pt.y >> 3)) : (pt.y + (3 * pt.x >> 3)));
}

static void _genSpan(SwRleData* rle, const SwSpan* spans, uint32_t count)
{
    auto newSize = rle->size + count;

    /* allocate enough memory for new spans */
    /* alloc is required to prevent free and reallocation */
    /* when the rle needs to be regenerated because of attribute change. */
    if (rle->alloc < newSize) {
        rle->alloc = (newSize * 2);
        rle->spans = static_cast<SwSpan*>(realloc(rle->spans, rle->alloc * sizeof(SwSpan)));
    }

    //copy the new spans to the allocated memory
    SwSpan* lastSpan = rle->spans + rle->size;
    memcpy(lastSpan, spans, count * sizeof(SwSpan));

    rle->size = newSize;
}


static void _horizLine(RleWorker& rw, SwCoord x, SwCoord y, SwCoord area, SwCoord acount)
{
    x += rw.cellMin.x;
    y += rw.cellMin.y;

    //Clip Y range
    if (y < 0) return;
    if (y >= rw.clip.h) return;

    /* compute the coverage line's coverage, depending on the outline fill rule */
    /* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
    auto coverage = static_cast<int>(area >> (PIXEL_BITS * 2 + 1 - 8));    //range 0 - 256

    if (coverage < 0) coverage = -coverage;

    if (rw.outline->fillRule == FillRule::EvenOdd) {
        coverage &= 511;
        if (coverage > 256) coverage = 512 - coverage;
        else if (coverage == 256) coverage = 255;
    } else {
        //normal non-zero winding rule
        if (coverage >= 256) coverage = 255;
    }

    //span has ushort coordinates. check limit overflow
    if (x >= SHRT_MAX) {
        //LOG: x coordinate overflow!
        x = SHRT_MAX;
    }
    if (y >= SHRT_MAX) {
        //LOG: y coordinate overflow!
        y = SHRT_MAX;
    }

    if (coverage > 0) {
        if (!rw.antiAlias) coverage = 255;
        auto count = rw.spansCnt;
        auto span = rw.spans + count - 1;

        //see whether we can add this span to the current list
        if ((count > 0) && (rw.ySpan == y) &&
            (span->x + span->len == x) && (span->coverage == coverage)) {

            //Clip x range
            SwCoord xOver = 0;
            if (x + acount >= rw.clip.w) xOver -= (x + acount - rw.clip.w);
            if (x < 0) xOver += x;

            //span->len += (acount + xOver) - 1;
            span->len += (acount + xOver);
            return;
        }

        if (count >=  MAX_SPANS) {
            _genSpan(rw.rle, rw.spans, count);
            rw.spansCnt = 0;
            rw.ySpan = 0;
            span = rw.spans;
        } else {
            ++span;
        }

        //Clip x range
        SwCoord xOver = 0;
        if (x + acount >= rw.clip.w) xOver -= (x + acount - rw.clip.w);
        if (x < 0) {
            xOver += x;
            x = 0;
        }

        //Nothing to draw
        if (acount + xOver <= 0) return;

        //add a span to the current list
        span->x = x;
        span->y = y;
        span->len = (acount + xOver);
        span->coverage = coverage;
        ++rw.spansCnt;
        rw.ySpan = y;
    }
}


static void _sweep(RleWorker& rw)
{
    if (rw.cellsCnt == 0) return;

    rw.spansCnt = 0;
    rw.ySpan = 0;

    for (int y = 0; y < rw.yCnt; ++y) {
        auto cover = 0;
        auto x = 0;
        auto cell = rw.yCells[y];

        while (cell) {

            if (cell->x > x && cover != 0) _horizLine(rw, x, y, cover * (ONE_PIXEL * 2), cell->x - x);
            cover += cell->cover;
            auto area = cover * (ONE_PIXEL * 2) - cell->area;
            if (area != 0 && cell->x >= 0) _horizLine(rw, cell->x, y, area, 1);

            x = cell->x + 1;
            cell = cell->next;
        }

        if (cover != 0) _horizLine(rw, x, y, cover * (ONE_PIXEL * 2), rw.cellXCnt - x);
    }

    if (rw.spansCnt > 0) _genSpan(rw.rle, rw.spans, rw.spansCnt);
}


static Cell* _findCell(RleWorker& rw)
{
    auto x = rw.cellPos.x;
    if (x > rw.cellXCnt) x = rw.cellXCnt;

    auto pcell = &rw.yCells[rw.cellPos.y];

    while(true) {
        Cell* cell = *pcell;
        if (!cell || cell->x > x) break;
        if (cell->x == x) return cell;
        pcell = &cell->next;
    }

    if (rw.cellsCnt >= rw.maxCells) longjmp(rw.jmpBuf, 1);

    auto cell = rw.cells + rw.cellsCnt++;
    cell->x = x;
    cell->area = 0;
    cell->cover = 0;
    cell->next = *pcell;
    *pcell = cell;

    return cell;
}


static void _recordCell(RleWorker& rw)
{
    if (rw.area | rw.cover) {
        auto cell = _findCell(rw);
        cell->area += rw.area;
        cell->cover += rw.cover;
    }
}


static void _setCell(RleWorker& rw, SwPoint pos)
{
    /* Move the cell pointer to a new position.  We set the `invalid'      */
    /* flag to indicate that the cell isn't part of those we're interested */
    /* in during the render phase.  This means that:                       */
    /*                                                                     */
    /* . the new vertical position must be within min_ey..max_ey-1.        */
    /* . the new horizontal position must be strictly less than max_ex     */
    /*                                                                     */
    /* Note that if a cell is to the left of the clipping region, it is    */
    /* actually set to the (min_ex-1) horizontal position.                 */

    /* All cells that are on the left of the clipping region go to the
       min_ex - 1 horizontal position. */
    pos.y -= rw.cellMin.y;

    if (pos.x > rw.cellMax.x) pos.x = rw.cellMax.x;
    pos.x -= rw.cellMin.x;
    if (pos.x < 0) pos.x = -1;

    //Are we moving to a different cell?
    if (pos != rw.cellPos) {
        //Record the current one if it is valid
        if (!rw.invalid) _recordCell(rw);
    }

    rw.area = 0;
    rw.cover = 0;
    rw.cellPos = pos;
    rw.invalid = ((unsigned)pos.y >= (unsigned)rw.cellYCnt || pos.x >= rw.cellXCnt);
}


static void _startCell(RleWorker& rw, SwPoint pos)
{
    if (pos.x > rw.cellMax.x) pos.x = rw.cellMax.x;
    if (pos.x < rw.cellMin.x) pos.x = rw.cellMin.x;
    //if (pos.x < rw.cellMin.x) pos.x = (rw.cellMin.x - 1);

    rw.area = 0;
    rw.cover = 0;
    rw.cellPos = pos - rw.cellMin;
    rw.invalid = false;

    _setCell(rw, pos);
}


static void _moveTo(RleWorker& rw, const SwPoint& to)
{
    //record current cell, if any */
    if (!rw.invalid) _recordCell(rw);

    //start to a new position
    _startCell(rw, TRUNC(to));

    rw.pos = to;
}


static void _lineTo(RleWorker& rw, const SwPoint& to)
{
#define SW_UDIV(a, b) \
    static_cast<SwCoord>(((unsigned long)(a) * (unsigned long)(b)) >> \
    (sizeof(long) * CHAR_BIT - PIXEL_BITS))

    auto e1 = TRUNC(rw.pos);
    auto e2 = TRUNC(to);

    //vertical clipping
    if ((e1.y >= rw.cellMax.y && e2.y >= rw.cellMax.y) ||
        (e1.y < rw.cellMin.y && e2.y < rw.cellMin.y)) {
            rw.pos = to;
            return;
    }

    auto diff = to - rw.pos;
    auto f1 = rw.pos - SUBPIXELS(e1);
    SwPoint f2;

    //inside one cell
    if (e1 == e2) {
        ;
    //any horizontal line
    } else if (diff.y == 0) {
        e1.x = e2.x;
        _setCell(rw, e1);
    } else if (diff.x == 0) {
        //vertical line up
        if (diff.y > 0) {
            do {
                f2.y = ONE_PIXEL;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * f1.x * 2;
                f1.y = 0;
                ++e1.y;
                _setCell(rw, e1);
            } while(e1.y != e2.y);
        //vertical line down
        } else {
            do {
                f2.y = 0;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * f1.x * 2;
                f1.y = ONE_PIXEL;
                --e1.y;
                _setCell(rw, e1);
            } while(e1.y != e2.y);
        }
    //any other line
    } else {
        Area prod = diff.x * f1.y - diff.y * f1.x;

        /* These macros speed up repetitive divisions by replacing them
           with multiplications and right shifts. */
        auto dx_r = static_cast<long>(ULONG_MAX >> PIXEL_BITS) / (diff.x);
        auto dy_r = static_cast<long>(ULONG_MAX >> PIXEL_BITS) / (diff.y);

        /* The fundamental value `prod' determines which side and the  */
        /* exact coordinate where the line exits current cell.  It is  */
        /* also easily updated when moving from one cell to the next.  */
        do {
            auto px = diff.x * ONE_PIXEL;
            auto py = diff.y * ONE_PIXEL;

            //left
            if (prod <= 0 && prod - px > 0) {
                f2 = {0, SW_UDIV(-prod, -dx_r)};
                prod -= py;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {ONE_PIXEL, f2.y};
                --e1.x;
            //up
            } else if (prod - px <= 0 && prod - px + py > 0) {
                prod -= px;
                f2 = {SW_UDIV(-prod, dy_r), ONE_PIXEL};
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {f2.x, 0};
                ++e1.y;
            //right
            } else if (prod - px + py <= 0 && prod + py >= 0) {
                prod += py;
                f2 = {ONE_PIXEL, SW_UDIV(prod, dx_r)};
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {0, f2.y};
                ++e1.x;
            //down
            } else {
                f2 = {SW_UDIV(prod, -dy_r), 0};
                prod += px;
                rw.cover += (f2.y - f1.y);
                rw.area += (f2.y - f1.y) * (f1.x + f2.x);
                f1 = {f2.x, ONE_PIXEL};
                --e1.y;
            }

            _setCell(rw, e1);

        } while(e1 != e2);
    }

    f2 = {to.x - SUBPIXELS(e2.x), to.y - SUBPIXELS(e2.y)};
    rw.cover += (f2.y - f1.y);
    rw.area += (f2.y - f1.y) * (f1.x + f2.x);
    rw.pos = to;
}


static void _cubicTo(RleWorker& rw, const SwPoint& ctrl1, const SwPoint& ctrl2, const SwPoint& to)
{
    auto arc = rw.bezStack;
    arc[0] = to;
    arc[1] = ctrl2;
    arc[2] = ctrl1;
    arc[3] = rw.pos;

    //Short-cut the arc that crosses the current band
    auto min = arc[0].y;
    auto max = arc[0].y;

    SwCoord y;
    for (auto i = 1; i < 4; ++i) {
        y = arc[i].y;
        if (y < min) min = y;
        if (y > max) max = y;
    }

    if (TRUNC(min) >= rw.cellMax.y || TRUNC(max) < rw.cellMin.y) goto draw;

    /* Decide whether to split or draw. See `Rapid Termination          */
    /* Evaluation for Recursive Subdivision of Bezier Curves' by Thomas */
    /* F. Hain, at                                                      */
    /* http://www.cis.southalabama.edu/~hain/general/Publications/Bezier/Camera-ready%20CISST02%202.pdf */
    while (true) {
        {
            //diff is the P0 - P3 chord vector
            auto diff = arc[3] - arc[0];
            auto L = HYPOT(diff);

            //avoid possible arithmetic overflow below by splitting
            if (L > SHRT_MAX) goto split;

            //max deviation may be as much as (s/L) * 3/4 (if Hain's v = 1)
            auto sLimit = L * (ONE_PIXEL / 6);

            auto diff1 = arc[1] - arc[0];
            auto s = diff.y * diff1.x - diff.x * diff1.y;
            if (s < 0) s = -s;
            if (s > sLimit) goto split;

            //s is L * the perpendicular distance from P2 to the line P0 - P3
            auto diff2 = arc[2] - arc[0];
            s = diff.y * diff2.x - diff.x * diff2.y;
            if (s < 0) s = -s;
            if (s > sLimit) goto split;

            /* Split super curvy segments where the off points are so far
            from the chord that the angles P0-P1-P3 or P0-P2-P3 become
            acute as detected by appropriate dot products */
            if (diff1.x * (diff1.x - diff.x) + diff1.y * (diff1.y - diff.y) > 0 ||
                diff2.x * (diff2.x - diff.x) + diff2.y * (diff2.y - diff.y) > 0)
                goto split;

            //no reason to split
            goto draw;
        }
    split:
        mathSplitCubic(arc);
        arc += 3;
        continue;

    draw:
        _lineTo(rw, arc[0]);
        if (arc == rw.bezStack) return;
        arc -= 3;
    }
}


static bool _decomposeOutline(RleWorker& rw)
{
    auto outline = rw.outline;
    auto first = 0;  //index of first point in contour

    for (uint32_t n = 0; n < outline->cntrsCnt; ++n) {
        auto last = outline->cntrs[n];
        auto limit = outline->pts + last;
        auto start = UPSCALE(outline->pts[first]);
        auto pt = outline->pts + first;
        auto types = outline->types + first;

        /* A contour cannot start with a cubic control point! */
        if (types[0] == SW_CURVE_TYPE_CUBIC) goto invalid_outline;

        _moveTo(rw, UPSCALE(outline->pts[first]));

        while (pt < limit) {
            ++pt;
            ++types;

            //emit a single line_to
            if (types[0] == SW_CURVE_TYPE_POINT) {
                _lineTo(rw, UPSCALE(*pt));
            //types cubic
            } else {
                if (pt + 1 > limit || types[1] != SW_CURVE_TYPE_CUBIC)
                    goto invalid_outline;

                pt += 2;
                types += 2;

                if (pt <= limit) {
                    _cubicTo(rw, UPSCALE(pt[-2]), UPSCALE(pt[-1]), UPSCALE(pt[0]));
                    continue;
                }
                _cubicTo(rw, UPSCALE(pt[-2]), UPSCALE(pt[-1]), start);
                goto close;
            }
        }
        _lineTo(rw, start);
    close:
       first = last + 1;
    }

    return true;

invalid_outline:
    //LOG: Invalid Outline!
    return false;
}


static int _genRle(RleWorker& rw)
{
    if (setjmp(rw.jmpBuf) == 0) {
        auto ret = _decomposeOutline(rw);
        if (!rw.invalid) _recordCell(rw);
        if (ret) return 0;  //success
        else return 1;      //fail
    }
    return -1;              //lack of cell memory
}


SwSpan* _intersectSpansRegion(const SwRleData *clip, const SwRleData *targetRle, SwSpan *outSpans, uint32_t spanCnt)
{
    auto out = outSpans;
    auto spans = targetRle->spans;
    auto end = targetRle->spans + targetRle->size;
    auto clipSpans = clip->spans;
    auto clipEnd = clip->spans + clip->size;

    while (spanCnt > 0 && spans < end ) {
        if (clipSpans == clipEnd) {
            spans = end;
            break;
        }
        if (clipSpans->y > spans->y) {
            ++spans;
            continue;
        }
        if (spans->y != clipSpans->y) {
            ++clipSpans;
            continue;
        }
        auto sx1 = spans->x;
        auto sx2 = sx1 + spans->len;
        auto cx1 = clipSpans->x;
        auto cx2 = cx1 + clipSpans->len;

        if (cx1 < sx1 && cx2 < sx1) {
            ++clipSpans;
            continue;
        }
        else if (sx1 < cx1 && sx2 < cx1) {
            ++spans;
            continue;
        }
        auto x = sx1 > cx1 ? sx1 : cx1;
        auto len = (sx2 < cx2 ? sx2 : cx2) - x;
        if (len) {
            auto spansCorverage = spans->coverage;
            auto clipSpansCoverage = clipSpans->coverage;
            out->x = sx1 > cx1 ? sx1 : cx1;
            out->len = (sx2 < cx2 ? sx2 : cx2) - out->x;
            out->y = spans->y;
            out->coverage = (uint8_t)((spansCorverage * clipSpansCoverage) >> 8);
            ++out;
            --spanCnt;
        }
        if (sx2 < cx2) ++spans;
        else ++clipSpans;
    }
    return out;
}

SwSpan* _intersectMaskRegion(const SwRleData *clip, const SwRleData *targetRle, SwSpan *outSpans, uint32_t spanCnt)
{

    auto out = outSpans;
    auto spans = targetRle->spans;
    auto end = targetRle->spans + targetRle->size;
    auto clipSpans = clip->spans;
    auto clipSpans1 = clip->spans;
    auto clipEnd = clip->spans + clip->size;

    auto maskClipMin = clipSpans1->y;
    auto maskClipMax = clipSpans1->y;

    while (clipSpans1->y) {
        if (clipSpans1->y > maskClipMax)
            maskClipMax = clipSpans1->y;

        if (clipSpans1->y < maskClipMax)
            maskClipMin = clipSpans1->y;
        clipSpans1++;
    }

    while (spanCnt && spans < end ) {
        if (clipSpans > clipEnd) {
            spans = end;
            break;
        }


        if (spans->y < maskClipMin || spans->y > maskClipMax) {
            out->x = spans->x;
            out->y = spans->y;
            out->len = spans->len;
            out->coverage = spans->coverage;
            ++out;
        } 
        else {
            while (clipSpans->y) {
                auto sx1 = spans->x;
                auto sx2 = sx1 + spans->len;
                auto cx1 = clipSpans->x;
                auto cx2 = cx1 + clipSpans->len;
                auto x = sx1 > cx1 ? sx1 : cx1;
                auto len = (sx2 < cx2 ? sx2 : cx2) - x;

                if (len > 1) {
                    out->x = sx1;
                    out->y = clipSpans->y;
                    out->len = cx1-sx1;
                    out->coverage = spans->coverage;
                    ++out;

                    out->x = cx2;
                    out->y = clipSpans->y;
                    out->len = sx2 - cx2;
                    out->coverage = spans->coverage;
                    ++out;
                }
                clipSpans++; 
            }
        }
        --spanCnt;
        ++spans;
    }

    return out;
}


SwSpan* _intersectSpansRect(const SwBBox *bbox, const SwRleData *targetRle, SwSpan *outSpans, uint32_t spanCnt)
{
    auto out = outSpans;
    auto spans = targetRle->spans;
    auto end = targetRle->spans + targetRle->size;
    auto minx = static_cast<int16_t>(bbox->min.x);
    auto miny = static_cast<int16_t>(bbox->min.y);
    auto maxx = minx + static_cast<int16_t>(bbox->max.x - bbox->min.x) - 1;
    auto maxy = miny + static_cast<int16_t>(bbox->max.y - bbox->min.y) - 1;

    while (spanCnt && spans < end ) {
        if (spans->y > maxy) {
            spans = end;
            break;
        }
        if (spans->y < miny || spans->x > maxx || spans->x + spans->len <= minx) {
            ++spans;
            continue;
        }
        if (spans->x < minx) {
            out->len = (spans->len - (minx - spans->x)) < (maxx - minx + 1) ? (spans->len - (minx - spans->x)) : (maxx - minx + 1);
            out->x = minx;
        }
        else {
            out->x = spans->x;
            out->len = spans->len < (maxx - spans->x + 1) ? spans->len : (maxx - spans->x + 1);
        }
        if (out->len != 0) {
            out->y = spans->y;
            out->coverage = spans->coverage;
            ++out;
        }
        ++spans;
        --spanCnt;
    }
    return out;
}


void _replaceClipSpan(SwRleData *rle, SwSpan* clippedSpans, uint32_t size)
{
    free(rle->spans);
    rle->spans = clippedSpans;
    rle->size = rle->alloc = size;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRleData* rleRender(SwRleData* rle, const SwOutline* outline, const SwBBox& bbox, const SwBBox& viewport, bool antiAlias)
{
    constexpr auto RENDER_POOL_SIZE = 16384L;
    constexpr auto BAND_SIZE = 40;

    //TODO: We can preserve several static workers in advance
    RleWorker rw;
    Cell buffer[RENDER_POOL_SIZE / sizeof(Cell)];

    //Init Cells
    rw.buffer = buffer;
    rw.bufferSize = sizeof(buffer);
    rw.yCells = reinterpret_cast<Cell**>(buffer);
    rw.cells = nullptr;
    rw.maxCells = 0;
    rw.cellsCnt = 0;
    rw.area = 0;
    rw.cover = 0;
    rw.invalid = true;
    //FIXME: REARRANGE BOUNDARY
    rw.cellMin = bbox.min;
    rw.cellMax = bbox.max;
    rw.cellXCnt = rw.cellMax.x - rw.cellMin.x;
    rw.cellYCnt = rw.cellMax.y - rw.cellMin.y;
    rw.ySpan = 0;
    rw.outline = const_cast<SwOutline*>(outline);
    rw.bandSize = rw.bufferSize / (sizeof(Cell) * 8);  //bandSize: 64
    rw.bandShoot = 0;
    rw.clip = {viewport.max.x - viewport.min.x, viewport.max.y - viewport.min.y};
    rw.antiAlias = antiAlias;

    if (!rle) rw.rle = reinterpret_cast<SwRleData*>(calloc(1, sizeof(SwRleData)));
    else rw.rle = rle;

    //Generate RLE
    Band bands[BAND_SIZE];
    Band* band;

    /* set up vertical bands */
    auto bandCnt = static_cast<int>((rw.cellMax.y - rw.cellMin.y) / rw.bandSize);
    if (bandCnt == 0) bandCnt = 1;
    else if (bandCnt >= BAND_SIZE) bandCnt = (BAND_SIZE - 1);

    auto min = rw.cellMin.y;
    auto yMax = rw.cellMax.y;
    SwCoord max;
    int ret;

    for (int n = 0; n < bandCnt; ++n, min = max) {
        max = min + rw.bandSize;
        if (n == bandCnt -1 || max > yMax) max = yMax;

        bands[0].min = min;
        bands[0].max = max;
        band = bands;

        while (band >= bands) {
            rw.yCells = static_cast<Cell**>(rw.buffer);
            rw.yCnt = band->max - band->min;

            int cellStart = sizeof(Cell*) * (int)rw.yCnt;
            int cellMod = cellStart % sizeof(Cell);

            if (cellMod > 0) cellStart += sizeof(Cell) - cellMod;

            auto cellEnd = rw.bufferSize;
            cellEnd -= cellEnd % sizeof(Cell);

            auto cellsMax = reinterpret_cast<Cell*>((char*)rw.buffer + cellEnd);
            rw.cells = reinterpret_cast<Cell*>((char*)rw.buffer + cellStart);

            if (rw.cells >= cellsMax) goto reduce_bands;

            rw.maxCells = cellsMax - rw.cells;
            if (rw.maxCells < 2) goto reduce_bands;

            for (int y = 0; y < rw.yCnt; ++y)
                rw.yCells[y] = nullptr;

            rw.cellsCnt = 0;
            rw.invalid = true;
            rw.cellMin.y = band->min;
            rw.cellMax.y = band->max;
            rw.cellYCnt = band->max - band->min;

            ret = _genRle(rw);
            if (ret == 0) {
                _sweep(rw);
                --band;
                continue;
            } else if (ret == 1) {
                goto error;
            }

        reduce_bands:
            /* render pool overflow: we will reduce the render band by half */
            auto bottom = band->min;
            auto top = band->max;
            auto middle = bottom + ((top - bottom) >> 1);

            /* This is too complex for a single scanline; there must
               be some problems */
            if (middle == bottom) goto error;

            if (bottom - top >= rw.bandSize) ++rw.bandShoot;

            band[1].min = bottom;
            band[1].max = middle;
            band[0].min = middle;
            band[0].max = top;
            ++band;
        }
    }

    if (rw.bandShoot > 8 && rw.bandSize > 16)
        rw.bandSize = (rw.bandSize >> 1);

    return rw.rle;

error:
    free(rw.rle);
    rw.rle = nullptr;
    return nullptr;
}


void rleReset(SwRleData* rle)
{
    if (!rle) return;
    rle->size = 0;
}


void rleFree(SwRleData* rle)
{
    if (!rle) return;
    if (rle->spans) free(rle->spans);
    free(rle);
}


void rleClipPath(SwRleData *rle, const SwRleData *clip)
{
    if (rle->size == 0 || clip->size == 0) return;
    auto spanCnt = rle->size > clip->size ? rle->size : clip->size;
    auto spans = static_cast<SwSpan*>(malloc(sizeof(SwSpan) * (spanCnt)));
    if (!spans) return;
    auto spansEnd = _intersectSpansRegion(clip, rle, spans, spanCnt);

    _replaceClipSpan(rle, spans, spansEnd - spans);

#ifdef THORVG_LOG_ENABLED
    cout << "SW_ENGINE: Using ClipPath!" << endl;
#endif
}


void rleClipRect(SwRleData *rle, const SwBBox* clip)
{
    if (rle->size == 0) return;
    auto spans = static_cast<SwSpan*>(malloc(sizeof(SwSpan) * (rle->size)));
    if (!spans) return;
    auto spansEnd = _intersectSpansRect(clip, rle, spans, rle->size);

    _replaceClipSpan(rle, spans, spansEnd - spans);

#ifdef THORVG_LOG_ENABLED
    cout <<"SW_ENGINE: Using ClipRect!" << endl;
#endif
}


void rleAlphaMask(SwRleData *rle, const SwRleData *clip)
{
    if (rle->size == 0 || clip->size == 0) return;
    auto spanCnt = rle->size + clip->size;

    auto spans = static_cast<SwSpan*>(malloc(sizeof(SwSpan) * (spanCnt)));

    if (!spans) return;
    auto spansEnd = _intersectMaskRegion(clip, rle, spans, spanCnt);

    _replaceClipSpan(rle, spans, spansEnd - spans);
}