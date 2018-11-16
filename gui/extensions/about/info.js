extension = {
    start: function () {

        var str = 'Version ' +  gpac.get_option('General', 'Version');
        str += '\nMore info: http://gpac.io';
        str += '\nDistributed under LGPL v2.1 or any later version';
        str += '\n(c) 2002-2018 Telecom ParisTech';
        str += '\n\nThanks to all great OSS tools used in GPAC: FreeType, SpiderMonkey, OpenHEVC, ffmpeg, libjpeg, libpng, faad2, libmad, SDL, ...';
        notif = gw_new_message(null, 'About GPAC', str, 0);
        notif.set_size(gw_display_width, gw_display_height);
        notif.set_alpha(0.8);
        notif.show();
    }
};

