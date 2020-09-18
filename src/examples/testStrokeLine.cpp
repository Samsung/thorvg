#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Test for Stroke Width
    for (int i = 0; i < 10; ++i) {
        auto shape = tvg::Shape::gen();
        shape->moveTo(50, 50 + (25 * i));
        shape->lineTo(750, 50 + (25 * i));
        shape->stroke(255, 255, 255, 255);       //color: r, g, b, a
        shape->stroke(i + 1);                    //stroke width
        shape->stroke(tvg::StrokeCap::Round);    //default is Square
        if (canvas->push(move(shape)) != tvg::Result::Success) return;
    }

    //Test for StrokeJoin & StrokeCap
    auto shape1 = tvg::Shape::gen();
    shape1->moveTo(20, 350);
    shape1->lineTo(250, 350);
    shape1->lineTo(220, 500);
    shape1->lineTo(70, 470);
    shape1->lineTo(70, 330);
    shape1->stroke(255, 0, 0, 255);
    shape1->stroke(10);
    shape1->stroke(tvg::StrokeJoin::Round);
    shape1->stroke(tvg::StrokeCap::Round);
    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    auto shape2 = tvg::Shape::gen();
    shape2->moveTo(270, 350);
    shape2->lineTo(500, 350);
    shape2->lineTo(470, 500);
    shape2->lineTo(320, 470);
    shape2->lineTo(320, 330);
    shape2->stroke(255, 255, 0, 255);
    shape2->stroke(10);
    shape2->stroke(tvg::StrokeJoin::Bevel);
    shape2->stroke(tvg::StrokeCap::Square);
    if (canvas->push(move(shape2)) != tvg::Result::Success) return;

    auto shape3 = tvg::Shape::gen();
    shape3->moveTo(520, 350);
    shape3->lineTo(750, 350);
    shape3->lineTo(720, 500);
    shape3->lineTo(570, 470);
    shape3->lineTo(570, 330);
    shape3->stroke(0, 255, 0, 255);
    shape3->stroke(10);
    shape3->stroke(tvg::StrokeJoin::Miter);
    shape3->stroke(tvg::StrokeCap::Butt);
    if (canvas->push(move(shape3)) != tvg::Result::Success) return;

    //Test for Stroke Dash
    auto shape4 = tvg::Shape::gen();
    shape4->moveTo(20, 600);
    shape4->lineTo(250, 600);
    shape4->lineTo(220, 750);
    shape4->lineTo(70, 720);
    shape4->lineTo(70, 580);
    shape4->stroke(255, 0, 0, 255);
    shape4->stroke(5);
    shape4->stroke(tvg::StrokeJoin::Round);
    shape4->stroke(tvg::StrokeCap::Round);

    float dashPattern1[2] = {10, 10};
    shape4->stroke(dashPattern1, 2);
    if (canvas->push(move(shape4)) != tvg::Result::Success) return;

    auto shape5 = tvg::Shape::gen();
    shape5->moveTo(270, 600);
    shape5->lineTo(500, 600);
    shape5->lineTo(470, 750);
    shape5->lineTo(320, 720);
    shape5->lineTo(320, 580);
    shape5->stroke(255, 255, 0, 255);
    shape5->stroke(5);
    shape5->stroke(tvg::StrokeJoin::Bevel);
    shape5->stroke(tvg::StrokeCap::Butt);

    float dashPattern2[2] = {10, 10};
    shape5->stroke(dashPattern2, 2);
    if (canvas->push(move(shape5)) != tvg::Result::Success) return;

    auto shape6 = tvg::Shape::gen();
    shape6->moveTo(520, 600);
    shape6->lineTo(750, 600);
    shape6->lineTo(720, 750);
    shape6->lineTo(570, 720);
    shape6->lineTo(570, 580);
    shape6->stroke(255, 255, 255, 255);
    shape6->stroke(5);
    shape6->stroke(tvg::StrokeJoin::Miter);
    shape6->stroke(tvg::StrokeCap::Square);

    float dashPattern3[2] = {10, 10};
    shape6->stroke(dashPattern3, 2);
    if (canvas->push(move(shape6)) != tvg::Result::Success) return;
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(swCanvas.get());
}

void drawSwView(void* data, Eo* obj)
{
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Evas_Object *obj)
{
    static constexpr auto BPP = 4;

    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();
    glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(glCanvas.get());
}

void drawGLview(Evas_Object *obj)
{
    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Initialize ThorVG Engine
    if (tvgEngine == tvg::CanvasEngine::Sw) {
        cout << "tvg engine: software" << endl;
    } else {
        cout << "tvg engine: opengl" << endl;
    }

    //Threads Count
    auto threads = std::thread::hardware_concurrency();

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {


        elm_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
