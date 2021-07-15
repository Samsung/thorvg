subloader_dep = []

if get_option('loaders').contains('svg') == true
    subdir('svg')
endif

if get_option('loaders').contains('png') == true
    subdir('png')
endif

if get_option('loaders').contains('tvg') == true
    subdir('tvg')
endif

if get_option('loaders').contains('jpg') == true
    if get_option('alternative-jpg-loader') == true
        subdir('jpg-alternative')
        message('Loaded alternative JPG Loader')
    else
        subdir('jpg')
        message('Loaded main JPG Loader')
    endif
endif

subdir('raw')

loader_dep = declare_dependency(
   dependencies: subloader_dep,
   include_directories : include_directories('.'),
)
