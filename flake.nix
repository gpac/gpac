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
            SDL2
          ] ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [
            darwin.CarbonHeaders
            darwin.apple_sdk.frameworks.Carbon
          ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
            mesa
            mesa-demos
            alsa-lib
            pulseaudio
            jack2
            xorg.libX11
            xorg.libXv
            xorg.xorgproto
          ];
          nativeBuildInputs = with pkgs; [ 
            git 
            pkg-config 
          ];
      in
      with pkgs;
      {
        packages.default = stdenv.mkDerivation {
          inherit buildInputs name nativeBuildInputs;
          src = self;
          meta = with nixpkgs.lib; {
            homepage = "https://gpac.io";
            description =
              "For Video Streaming and Next-Gen Multimedia Transcoding, Packaging and Delivery.";
            platforms = platforms.darwin ++ platforms.linux;
            mainProgram = "gpac";
            #TODO support Nix uses glibtool instead of Apple libtool, we do not support it   
            darwinConfigureFlags = [ "--disable-static" ];
          };
        };

        devShells.default = mkShell {
          inherit buildInputs nativeBuildInputs;
        };

      }
    );
  
}
