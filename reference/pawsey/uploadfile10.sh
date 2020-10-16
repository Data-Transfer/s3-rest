#!/bin/bash
P=$PWD/striped_10G_over_64
./s3bin/s3-upload -e https://nimbus.pawsey.org.au:8080 -f $P/10Ghpc$SLURM_NODEID -b transfer-test -k 10Ghpc$SLURM_NODEID -j 32