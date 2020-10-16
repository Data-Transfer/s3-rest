#!/bin/bash
P=$PWD/striped_4G_over_32
mc cp $P/4Ghpc$SLURM_NODEID ceph/transfer-test/mc4$SLURM_NODEID