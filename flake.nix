{
  description = "GPAC Multimedia Open Source Project";
  inputs =  {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
    outputs = { self, nixpkgs, flake-utils }: 
    flake-utils.lib.eachDefaultSystem (system:
      let
          name = "gpac";
          src = self;
          pkgs = nixpkgs.legacyPackages.${system};
          buildInputs = with pkgs; [ 
            zlib 
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
            xvidcore
            openssl
            alsa-lib
            SDL2
          ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
            darwin.CarbonHeaders
          ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
            mesa
            mesa-demos
            pulseaudio
            jack2
            xorg.libX11
            xorg.libXv
            xorg.xorgproto
          ];
          nativeBuildInputs = with pkgs; [ git pkg-config ];
      in
      with pkgs;
      {
        packages.default = stdenv.mkDerivation {
          inherit buildInputs name src nativeBuildInputs;
        };

        devShells.default = mkShell {
          inherit buildInputs nativeBuildInputs;
        };

        meta = with nixpkgs.lib; {
          homepage = "https://gpac.io";
          description =
            "For Video Streaming and Next-Gen Multimedia Transcoding, Packaging and Delivery.";
          platforms = platforms.all;
        };
      }
    );
  
}
