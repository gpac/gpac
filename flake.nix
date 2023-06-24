{
  description = "GPAC Multimedia Open Source Project";
  inputs =  {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };
  outputs = { self, nixpkgs }@inputs: 
  let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
    buildInputs = with pkgs; [ 
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
    nativeBuildInputs = with pkgs; [ pkg-config ];
 in
  with pkgs; {  
    devShells.x86_64-linux.default = mkShell {
        inherit buildInputs nativeBuildInputs;
    };
    packages.x86_64-linux.default =  with import nixpkgs { system = "x86_64-linux"; };
      stdenv.mkDerivation {
        name = "gpac";
        src = self;
        inherit buildInputs nativeBuildInputs;
        configurePhase = ''
          runHook preConfigure
          ./configure --prefix=$out
          runHook postConfigure
        '';
        buildPhase = "make -j $NIX_BUILD_CORES";
        installPhase = "make install";
      }; 

  };
}
