examples_dep = dependency('elementary', required : true)

source_file = [
    'Arc.cpp',
    'Async.cpp',
    'Blending.cpp',
    'BlendingModes.cpp',
    'Boundary.cpp',
    'CustomTransform.cpp',
    'DirectUpdate.cpp',
    'Duplicate.cpp',
    'FillRule.cpp',
    'GradientStroke.cpp',
    'GradientTransform.cpp',
    'LinearGradient.cpp',
    'MultiCanvas.cpp',
    'MultiShapes.cpp',
    'Opacity.cpp',
    'PathCopy.cpp',
    'Path.cpp',
    'PictureJpg.cpp',
    'PicturePng.cpp',
    'PictureRaw.cpp',
    'RadialGradient.cpp',
    'Scene.cpp',
    'SceneTransform.cpp',
    'Shape.cpp',
    'Stacking.cpp',
    'Stress.cpp',
    'Stroke.cpp',
    'StrokeLine.cpp',
    'Svg.cpp',
    'Svg2.cpp',
    'Transform.cpp',
    'Tvg.cpp',
    'Update.cpp',
    'ClipPath.cpp',
    'Masking.cpp',
    'InvMasking.cpp',
    'GradientMasking.cpp',
    'AnimateMasking.cpp',
]

foreach current_file : source_file
    name = current_file.split('.')[0]
    executable(name, current_file,
        include_directories : headers,
        link_with : thorvg_lib,
        dependencies : examples_dep)
endforeach


if get_option('bindings').contains('capi') == true
    capi_source_file = [
        'Capi.cpp'
    ]

    foreach current_file : capi_source_file
        name = current_file.split('.')[0]
        executable(name, current_file,
            include_directories : headers,
            link_with : thorvg_lib,
            dependencies : examples_dep)
    endforeach
endif
