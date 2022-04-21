#pragma once

#include <string>

inline const std::string AUTOCONFIG = R"#(
########################################################################################
AUTOGENERATED HYPR CONFIG.
PLEASE USE THE CONFIG PROVIDED IN THE GIT REPO /examples/hypr.conf AND EDIT IT,
OR EDIT THIS ONE ACCORDING TO THE WIKI INSTRUCTIONS.
########################################################################################

autogenerated=1 # remove this line to get rid of the warning on top.

monitor=,1920x1080@60,0x0,1

input {
    kb_layout=
    kb_variant=
    kb_model=
    kb_options=
    kb_rules=
    repeat_rate=
    repeat_delay=

    touchpad {
        disable_while_typing=
    }
}

general {
    max_fps=60
    sensitivity=0.25
    main_mod=SUPER

    gaps_in=5
    gaps_out=20
    border_size=2
    col.active_border=0x66ee1111
    col.inactive_border=0x66333333
}

decoration {
    rounding=10
}

animations {
    enabled=1
    speed=7
    windows_speed=6 # specific speeds for components can be made with name_speed=float. 0 means use global (speed=float). If not set, will use the global value.
    windows=1
    borders=1
    borders_speed=20
    fadein=1 # fade in AND out
    fadein_speed=20
}

# basic binds
bind=SUPER,return,exec,kitty
bind=SUPER,T,exec,alacritty
bind=SUPER,C,killactive,
bind=SUPER,M,exec,pkill Hyprland
bind=SUPER,V,togglefloating,

bind=SUPER,1,workspace,1
bind=SUPER,2,workspace,2
bind=SUPER,3,workspace,3
bind=SUPER,4,workspace,4
bind=SUPER,5,workspace,5
bind=SUPER,6,workspace,6
bind=SUPER,7,workspace,7
bind=SUPER,8,workspace,8
bind=SUPER,9,workspace,9
bind=SUPER,0,workspace,10
)#";