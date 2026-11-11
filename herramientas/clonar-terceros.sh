#!/usr/bin/env bash
# Clona (o actualiza) el código de otros que usamos para estudiar y probar, en terceros/.
# Nada de esto entra en nuestro Git. Uso: herramientas/clonar-terceros.sh
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p terceros/rescate
clonar() {  # clonar <usuario/repo> <carpeta> [--recursive]
  local repo="$1" dest="$2"; shift 2
  if [ -d "$dest/.git" ]; then git -C "$dest" pull -q --ff-only && echo "actualizado  $dest"
  else git clone -q --depth 1 "$@" "https://github.com/$repo.git" "$dest" && echo "clonado      $dest"; fi
}
# El emulador (Rust): web, fm1-live (Linux) y plugin CLAP/VST3
clonar joakimeriksson/fm1-emu    terceros/fm1-emu
# Las bases para escribir un firmware
clonar charlesvestal/fm1-base    terceros/fm1-base --recursive   # MPL-2.0: la app puede llevar cualquier licencia
clonar zvenson/dinghy            terceros/dinghy                  # el firmware mínimo, todo documentado
clonar hugelton/Felucca          terceros/Felucca --recursive     # el original; motores y bsp como submódulos
clonar m-vave/fm1_open           terceros/fm1_open                # el oficial de M-VAVE (motor en binario)
# Firmwares para estudiar
clonar charlesvestal/fm1-amb     terceros/fm1-amb --recursive     # AMB-1, generativo ambient
clonar BlackKittenDevices/kitten-rack terceros/kitten-rack        # un modular con cables dentro del FM-1
clonar TexasRADIO/bb1-firmware   terceros/bb1-firmware            # lleva un bajo derivado de Open303
clonar npi3pak/fm1-gator         terceros/fm1-gator               # Plaits de Mutable entero
clonar isod89/sloop-fm1          terceros/sloop-fm1               # el más popular
# Ingeniería inversa
clonar AL-255/FM-1-RE            terceros/FM-1-RE
# Rescate de un FM-1 bloqueado
clonar Quixotic7/MvaveFM1Unbricker      terceros/rescate/MvaveFM1Unbricker
clonar GoodPerson42/FM1-Unbricking-For-Dummies terceros/rescate/FM1-Unbricking-For-Dummies
clonar HuskiePJN/fm1-mac-recovery       terceros/rescate/fm1-mac-recovery
