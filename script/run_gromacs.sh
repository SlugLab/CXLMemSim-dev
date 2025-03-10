#!/bin/bash
wget https://files.rcsb.org/download/5pep.pdb
gmx pdb2gmx -f 5pep.pdb -o 5pep.gro -water spc
gmx editconf -f 5pep.gro -o 5pep-box.gro -c -d 1.0 -bt cubic
gmx solvate -cp 5pep-box.gro -cs spc216.gro -o 5pep-solv.gro -p topol.top
