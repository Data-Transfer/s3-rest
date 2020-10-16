#!/bin/bash
P=$PWD/striped_4G_over_32
mc cp $P/10Ghpc$SLURM_NODEID ceph/transfer-test/mc$SLURM_NODEID