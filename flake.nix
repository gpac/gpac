{
  description = "GPAC Multimedia Open Source Project";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
        buildInputs = with pkgs; [
          zlib
          freetype
          libjpeg_turbo
          libpng
          libmad
          faad2
          libaom
          libogg
          libvorbis
          libtheora
          a52dec
          ffmpeg
          xvidcore
          openssl
          SDL2
        ] ++ (lib.optionals stdenv.isLinux [
          xorg.libX11
          xorg.libXv
          xorg.xorgproto
          mesa
          alsa-lib
          jack2
          pulseaudio
          mesa-demos
        ]) ++ (lib.optionals stdenv.isDarwin [
          darwin.apple_sdk_11_0.frameworks.Carbon
        ]);
        nativeBuildInputs = with pkgs; [ pkg-config git ];
      in
      with pkgs; {
        devShells.default = mkShell {
          inherit buildInputs nativeBuildInputs;
        };
        packages.default = stdenv.mkDerivation {
          name = "gpac";
          src = self;
          enableParallelBuilding = true;
          inherit buildInputs nativeBuildInputs;
        };
      });
}
