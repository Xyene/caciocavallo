{ lib, buildMaven, stdenv, wayland, libxkbcommon, cairo, maven, jdk11 }:

let
  mavenStuff = buildMaven ./project-info.json;

in stdenv.mkDerivation {
  pname = "caciocavallo-wayland";
  version = "1.10-jdk11-dev";

  src = lib.cleanSource ./.;

  postPatch = ''
    sed -i cacio-wayland/pom.xml \
      -e "s;\(<wayland.include>\).*\(</wayland.include>\);\1${wayland}/include\2;" \
      -e "s;\(<xkbcommon.include>\).*\(</xkbcommon.include>\);\1${libxkbcommon.dev}/include\2;" \
      -e "s;\(<cairo.include>\).*\(</cairo.include>\);\1${cairo.dev}/include\2;" \
      -e "s;\(<wayland.lib>\).*\(</wayland.lib>\);\1${wayland}/lib\2;" \
      -e "s;\(<xkbcommon.lib>\).*\(</xkbcommon.lib>\);\1${libxkbcommon}/lib\2;" \
      -e "s;\(<cairo.lib>\).*\(</cairo.lib>\);\1${cairo}/lib\2;"
  '';

  buildInputs = [ maven jdk11 ];

  buildPhase = ''
    mvn --offline --settings ${mavenStuff.settings} compile
  '';

  installPhase = ''
    mvn --offline --settings ${mavenStuff.settings} package -Dmaven.test.skip=true

    install -D cacio-wayland/target/nar/*/lib/*/jni/libcacio-wayland-*.so $out/lib/libcacio-wayland.so

    install -Dm644 -t $out/share cacio-*/target/*.jar
  '';
}
