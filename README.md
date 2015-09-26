#MATE Xfce4 panel plugin loader

##Applet for MATE panel which can load external Xfce4 panel plugins like xfce4-whiskermenu-plugin or xfce4-weather-plugin!

###Known bugs and limitations:
- this applet uses internal "xfce4-panel" and "mate-panel" structures, so it may be incompatible between versions,
- adding item to panel from context menu in "xfce4-whiskermenu-plugin" doesn't work,
- about context menu doesn't work correctly when more then one applet is loaded,
- "xfce4-whiskermenu-plugin" can crash when more then one instance is created.

###MATE commands for Whisker Menu:
- All Settings: "mate-control-center",
- Lock Screen: "mate-screensaver-command -l",
- Switch Users: "mate-session-save --logout-dialog",
- Log Out: "mate-session-save --shutdown-dialog".
