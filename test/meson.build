aggplatform_dep = dependency('libaggplatform')

demo_freetype_deps = [aggplatform_dep, agg_dep, freetype_dep]
demo_freetype_cppargs = []
if host_machine.system() == 'windows'
    demo_freetype_deps += cc.find_library('shcore', required : true)
    # Flags below to target Windows 8.1
    demo_freetype_cppargs += '-D_WIN32_WINNT=_WIN32_WINNT_WINBLUE'
endif

executable('demo-freetype-lcd',
    'freetype_lcd.cpp',
    link_with: libaggfreetype,
    dependencies: demo_freetype_deps,
    cpp_args: demo_freetype_cppargs,
    include_directories: agg_font_include,
    install: true,
)
