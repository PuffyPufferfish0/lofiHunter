{ pkgs ? import <nixpkgs> {} }:

let
  # Fetch nixGL to bridge the gap between Nix and host OS graphics drivers
  nixgl = import (fetchTarball "https://github.com/nix-community/nixGL/archive/main.tar.gz") { enable32bits = false; };
in
pkgs.mkShell {
  name = "music-idle-env";

  # Development tools and compilers
  nativeBuildInputs = with pkgs; [
    cmake
    gnumake
    pkg-config
    cargo
    rustc
  ];

  # Libraries we need to link against
  buildInputs = with pkgs; [
    glfw
    libGL
    nixgl.auto.nixGLDefault # The magic wrapper!
    
    # Standard X11 libraries
    xorg.libX11
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXi
  ];

  shellHook = ''
    export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [ pkgs.libGL pkgs.glfw pkgs.xorg.libX11 ]}:$LD_LIBRARY_PATH
    
    echo "======================================================"
    echo "nixGL has been installed to bridge your graphics drivers."
    echo ""
    echo "nixGL ./MusicIdleGame"
    echo "======================================================"
  '';
}
