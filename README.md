#MATE Xfce4 panel plugin loader

##Applet for MATE panel which can load Xfce4 panel plugins like xfce4-whiskermenu-plugin or xfce4-weather-plugin!

###Known bugs and limitations:
- this applet uses internal "xfce4-panel" and "mate-panel" structures, so it may be incompatible between versions,
- crash is possible after removing some Xfce4 panel plugins like "xfce4-clipman-plugin" (sometimes after a while),
- about context menu doesn't exists due to problems when more then one Xfce4 panel plugin is loaded,
- bug in Xfce4 panel plugin can crash all Xfce4 panel plugins loaded into Mate panel,
- adding item to panel from context menu in "xfce4-whiskermenu-plugin" doesn't work,
- cannot load more then one the same Xfce4 panel plugin,

###MATE commands for Whisker Menu:
- All Settings: "mate-control-center",
- Lock Screen: "mate-screensaver-command -l",
- Switch Users: "mate-session-save --logout-dialog",
- Log Out: "mate-session-save --shutdown-dialog",
- Edit Applications: "mozo".
