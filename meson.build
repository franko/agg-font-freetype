project('agg-font-freetype', 'cpp', version: '0.1', default_options : ['cpp_std=c++03'])

cc = meson.get_compiler('cpp')
libm = cc.find_library('m', required : false)

agg_dep = dependency('libagg')
freetype_dep = dependency('freetype2')

agg_font_include = include_directories('include')

subdir('src')
subdir('test')

install_headers('include/agg_font_freetype.h') #, install_dir : 'include/agg2')
