#!/bin/sh
# ASS-ID_FM1 : entrée de build CI et locale. Récupère fm1-base (la couche matériel
# MPL-2.0, base/ -> terceros/fm1-base) s'il n'est pas là, puis compile ATEK303 :
#   ./build.sh --release 1.0     # atek303/app/build/ass-id-1.0.fwsc (toolchain JieLi requis)
#   ./build.sh --host            # le simulateur host
#   ./build.sh --web             # la version navigateur
# Un tag vX.Y sur GitHub lance la même chose signée : .github/workflows/release.yml
set -e
cd "$(dirname "$0")"
if [ ! -f terceros/fm1-base/tools/build.py ]; then
    git clone -q --depth 1 --recursive https://github.com/charlesvestal/fm1-base.git terceros/fm1-base
fi
exec atek303/build.sh "$@"
