{ rsync, jdk }:

jdk11.overrideAttrs (old: {
  postPatch = ''
    ${rsync}/bin/rsync -a ${./caciocavallo/cacio-shared/src/main/java}/ src/java.desktop/unix/classes/
    ${rsync}/bin/rsync -a ${./caciocavallo/cacio-wayland/src/main/java}/ src/java.desktop/unix/classes/
    ${rsync}/bin/rsync -a ${./caciocavallo/cacio-wayland/src/main/include}/ src/java.desktop/unix/native/libcacio-wayland/
    ${rsync}/bin/rsync -a ${./caciocavallo/cacio-wayland/src/main/native}/ src/java.desktop/unix/native/libcacio-wayland/
  '';
})
