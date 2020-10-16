#!/bin/bash
P=$PWD/striped_4G_over_32
./s3bin/s3-upload -e https://nimbus.pawsey.org.au:8080 -f $P/4Ghpc$SLURM_NODEID -b transfer-test -k 4Ghpc$SLURM_NODEID -j 32