project('ps5-app-crt', ['c', 'cpp'],
    default_options: {
        'default_library': 'static',
        'prefer_static':    true
    },
    meson_version: '>=1.2.0',
    version: '1.0'
)


ps5_app_crt_lib = static_library('ps5_app_crt',
    pic: true,
    sources: ps5_app_crt_srcs,
    c_args: [
        '-fms-extensions'
    ]
)

ps5_app_crt_dep = declare_dependency(
    link_whole: ps5_app_crt_lib
)

meson.override_dependency('ps5-app-crt', ps5_app_crt_dep)
