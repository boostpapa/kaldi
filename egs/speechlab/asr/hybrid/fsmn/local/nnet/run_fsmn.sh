#!/bin/bash

# This example script trains a LSTM network on FBANK features.

. ./cmd.sh
. ./path.sh


stage=0
. utils/parse_options.sh || exit 1;

dir=exp/fsmn_c5
ali=exp/tri_gmm_ali
train=data/train/train
cuda_cmd=run.pl
#cuda_cmd="queue.pl -l hostname=chengdu"

#false && \
{
if [ $stage -le 1 ]; then
  # Train the DNN optimizing per-frame cross-entropy.
  # Train
  # --train-tool "nnet-train-lstm-streams-asgd --num-stream=40 --batch-size=20 --targets-delay=5" \
  feature_transform=$dir/final.feature_transform
  # --feature-transform $feature_transform
  $cuda_cmd $dir/log/train_nnet.log \
    steps/nnet/train_faster.sh --learn-rate 0.00001 --copy-feats false --nnet-proto $dir/nnet.proto \
      --feat-type plain --splice 5 --cmvn-opts "--norm-means=true --norm-vars=false" \
      --start_half_lr 5 --momentum 0.9 \
      --train-tool "nnet-train-fsmn-streams" --train-tool-opts "--minibatch-size=4096 --loss-report-frames=360000 " \
    ${train}_tr90 ${train}_cv10 data/lang $ali $ali $dir || exit 1;

  # --cmvn-opts "--norm-means=true --norm-vars=false"
  # --splice 0 
  # --splice-left 0 --splice_right 7
  # --feature-transform $feature_transform 
  # --skip-opts "--skip-frames=2"
  # --skip-frames 3 --skip-inner true --sweep-loop false
  # --online true --cmvn-opts "--cmn-window=10000 --min-cmn-window=100 --norm-vars=false"
  # --delta-opts "--delta-order=2"
  
fi
}

echo Success
exit 0
