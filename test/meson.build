aggplatform_dep = dependency('libaggplatform')

executable('demo-freetype-lcd',
    'freetype_lcd.cpp',
    link_with: libaggfreetype,
    dependencies: [aggplatform_dep, agg_dep, freetype_dep],
    include_directories: agg_font_include,
    install: true,
)
