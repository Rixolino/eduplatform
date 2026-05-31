{pkgs}: {
  deps = [
    pkgs.pkg-config
    pkgs.cmake
    pkgs.sqlite
    pkgs.libmicrohttpd
  ];
}
