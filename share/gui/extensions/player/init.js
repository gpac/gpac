
obj = {
    whoami: "GPAC-GUI-Extension",
    name: "Player",
    icon: "applications-multimedia.svg",
    author: "JeanLF",
    description: "Nice test extension",
    url: "http://foo.bar",
    execjs: ["player.js", "fileopen.js", "playlist.js", "stats.js"],
    autostart: true,
    config_id: "Player",
    requires_gl: false,
    version_major: 1,
    version_minor: 0,
    clopts: [
     {name: "-stat", type: "", description: "show GUI stats"},
     {name: "-loop", type: "", description: "loop content"},
     {name: "-speed", type: "number", description: "set speed content"},
     {name: "-play-from", type: "number", description: "play from given time"},
     {name: "-service", type: "int", description: "set initial service ID for multi-service sources"},
     {name: "-addon", type: "URL", description: "inject specified URL as addon of main content"},
    ],
    clusage: "[options] URL [options] [URL2]",
};
