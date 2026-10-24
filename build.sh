export PSPDEV=$HOME/pspdev
export PATH=$PATH:$PSPDEV/bin
cd /home/familia_oso/geometrydash2.0-psp
make -f Makefile.psp -j8 2>&1 | tail -20
make -f Makefile.psp deploy DEPLOY="/mnt/e/Juegos PS2/gd20psp" 2>&1 | tail -10