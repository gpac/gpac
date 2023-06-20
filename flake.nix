{
  description = "GPAC Multimedia Open Source Project";

  outputs = { self, nixpkgs }: {

    packages.x86_64-linux.default =  with import nixpkgs { system = "x86_64-linux"; };
      stdenv.mkDerivation {
        name = "gpac";
        src = self;
        nativeBuildInputs = [ pkg-config ];
        buildInputs = [ 
          zlib 
          git
          freetype 
          libjpeg_turbo 
          libpng 
          libmad 
          faad2 
          libogg 
          libvorbis 
          libtheora
          a52dec
          ffmpeg
          xorg.libX11
          xorg.libXv
          xorg.xorgproto
          mesa
          xvidcore
          openssl
          alsa-lib
          jack2
          pulseaudio
          SDL2
          mesa-demos
        ];

        configurePhase = ''
          runHook preConfigure
          ./configure --prefix=$out
          runHook postConfigure
        '';
        buildPhase = "make";
        installPhase = "make install";
      }; 

  };
}
