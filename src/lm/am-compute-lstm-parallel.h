// lm/am-compute-lstm-parallel.h

// Copyright 2015-2016   Shanghai Jiao Tong University (author: Wei Deng)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#ifndef KALDI_LM_AM_COMPUTE_LSTM_PARALLEL_H_
#define KALDI_LM_AM_COMPUTE_LSTM_PARALLEL_H_

#include "hmm/transition-model.h"

#include <string>
#include <iomanip>
#include <mpi.h>

#include "cudamatrix/cu-device.h"

#include "nnet0/nnet-trnopts.h"
#include "nnet0/nnet-randomizer.h"
#include "nnet0/nnet-loss.h"
#include "nnet0/nnet-nnet.h"
#include "nnet0/nnet-compute-lstm-asgd.h"

#include "lm/lm-model-sync.h"

namespace kaldi {
namespace lm {

typedef nnet0::NnetLstmUpdateOptions NnetLstmUpdateOptions;
typedef nnet0::NnetStats NnetStats;

void AmLstmUpdateParallel(const NnetLstmUpdateOptions *opts,
		std::string	model_filename,
		std::string	target_model_filename,
		std::string feature_rspecifier,
		std::string targets_rspecifier,
		Nnet *nnet,
		NnetStats *stats);


} // namespace lm
} // namespace kaldi

#endif // KALDI_LM_AM_COMPUTE_LSTM_PARALLEL_H_
