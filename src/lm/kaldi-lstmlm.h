// lm/kaldi-lstmlm.h

// Copyright 2018-2019   Alibaba Inc (author: Wei Deng)

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

#ifndef KALDI_LM_KALDI_NNLM_H_
#define KALDI_LM_KALDI_NNLM_H_

#include <string>
#include <vector>

#include "base/kaldi-common.h"
#include "fstext/deterministic-fst.h"
#include "util/common-utils.h"
#include "nnet0/nnet-nnet.h"

namespace kaldi {

struct LstmlmHistroy {
	LstmlmHistroy(std::vector<int> &rdim, std::vector<int> &cdim,
                    MatrixResizeType resize_type = kSetZero) {
        his_recurrent.resize(rdim.size());
        for (int i = 0; i < rdim.size(); i++)
            his_recurrent[i].Resize(rdim[i], resize_type);
        his_cell.resize(cdim.size());
        for (int i = 0; i < cdim.size(); i++)
            his_cell[i].Resize(cdim[i], resize_type);
    }
    LstmlmHistroy() {}

	std::vector<Vector<BaseFloat> > his_recurrent; //  each hidden lstm layer recurrent history
	std::vector<Vector<BaseFloat> > his_cell; //  each hidden lstm layer cell history
};

struct Sequence {
	Sequence(LstmlmHistroy *h, int blank = 0) {
		pred.clear();
		k.push_back(blank);
		lmhis = h;
		logp = 0;
	}

	std::vector<Vector<BaseFloat>* > pred; 	// rnnt language model output
	std::vector<int> k;						// decoded word list
	LstmlmHistroy *lmhis;					// rnnt language model history
	BaseFloat logp;							// probability of this sequence, in log scale

	std::string tostring() {
		return "";
	}
};

struct PrefixSeq {
	PrefixSeq(LstmlmHistroy *h, int blank = 0) {
		prefix.push_back(blank);
		lmhis = h;
		logp_blank = kLogZeroFloat;
		logp_nblank = kLogZeroFloat;
	}

	PrefixSeq(LstmlmHistroy *h, const std::vector<int> &words) {
		lmhis = h;
		prefix = words;
		logp_blank = kLogZeroFloat;
		logp_nblank = kLogZeroFloat;
	}

	PrefixSeq(const std::vector<int> &words) {
		prefix = words;
		lmhis = NULL;
		logp_blank = kLogZeroFloat;
		logp_nblank = kLogZeroFloat;
	}

	// decoded word list
	std::vector<int> prefix;

	// rnnt language model history
	LstmlmHistroy *lmhis;

	// log probabilities for the prefix given that
	// it ends in a blank and dose not end in a blank at this time step.
	BaseFloat logp_blank;
	BaseFloat logp_nblank;

	std::string tostring() {
		return "";
	}
};

struct LstmlmUtil {
	static bool compare_len(const Sequence *a, const Sequence *b) {
		return a->k.size() < b->k.size();
	}

	static bool compare_len_reverse(const Sequence *a, const Sequence *b) {
		return a->k.size() > b->k.size();
	}

	static bool compare_logp(const Sequence *a, const Sequence *b) {
		return a->logp < b->logp;
	}

	static bool compare_logp_reverse(const Sequence *a, const Sequence *b) {
		return a->logp > b->logp;
	}

	static bool compare_PrefixSeq_reverse(const PrefixSeq *a, const PrefixSeq *b) {
		return LogAdd(a->logp_blank,a->logp_nblank) > LogAdd(b->logp_blank,b->logp_nblank);
	}

	static bool isprefix(const std::vector<int> &a, const std::vector<int> &b) {
		int lena = a.size();
		int lenb = b.size();
		if (lena >= lenb) return false;
		for (int i = 0; i <= lena; i++)
			if (a[i] != b[i]) return false;
		return true;
	}

};

struct KaldiLstmlmWrapperOpts {
  std::string unk_symbol;
  std::string sos_symbol;
  std::string eos_symbol;
  int num_stream;
  bool remove_head;

  KaldiLstmlmWrapperOpts() : unk_symbol("<unk>"), sos_symbol("<s>"), eos_symbol("</s>"),
		  num_stream(1), remove_head(false) {}

  void Register(OptionsItf *opts) {
    opts->Register("unk-symbol", &unk_symbol, "Symbol for out-of-vocabulary "
                   "words in neural network language model.");
    opts->Register("sos-symbol", &sos_symbol, "Start of sentence symbol in "
                   "neural network language model.");
    opts->Register("eos-symbol", &eos_symbol, "End of sentence symbol in "
                   "neural network language model.");
    opts->Register("num-stream", &num_stream, "Number of utterance process in parallel in "
                   "neural network language model.");
  }
};

class KaldiLstmlmWrapper {
 public:
  KaldiLstmlmWrapper(const KaldiLstmlmWrapperOpts &opts,
                    const std::string &word_symbol_table_rxfilename,
					const std::string &lm_word_symbol_table_rxfilename,
                    const std::string &nnlm_rxfilename);

  int GetEos() const { return eos_; }
  int GetSos() const { return sos_; }
  int GetUnk() const { return unk_; }

  std::vector<int> &GetRDim() { return recurrent_dim_; }
  std::vector<int> &GetCDim() { return cell_dim_; }
  int GetVocabSize() { return nnlm_.OutputDim();}

  void GetLogProbParallel(const std::vector<int> &curt_words,
  										 const std::vector<LstmlmHistroy*> &context_in,
  										 std::vector<LstmlmHistroy*> &context_out,
  										 std::vector<BaseFloat> &logprob);

  BaseFloat GetLogProb(int curt_words, LstmlmHistroy* context_in,
		  	  	  	  	  	  	  	  	  LstmlmHistroy* context_out);

  void Forward(int words_in, LstmlmHistroy& context_in,
		  	   Vector<BaseFloat> *nnet_out, LstmlmHistroy *context_out);

  void ForwardMseq(const std::vector<int> &in_words,
		  	  	  	  	  	  	  	  	  const std::vector<LstmlmHistroy*> &context_in,
										  std::vector<Vector<BaseFloat>*> &nnet_out,
										  std::vector<LstmlmHistroy*> &context_out);

  inline int GetWordId(int wid) { return label_to_lmwordid_[wid];}
  inline int GetWordId(std::string word) { return word_to_lmwordid_[word];}

 private:
  nnet0::Nnet nnlm_;
  std::vector<std::string> label_to_word_;
  std::vector<int> label_to_lmwordid_;
  std::unordered_map<std::string, int> word_to_lmwordid_;
  std::vector<int> recurrent_dim_;
  std::vector<int> cell_dim_;
  int unk_;
  int sos_;
  int eos_;

  int num_stream_;
  Matrix<BaseFloat> out_linearity_;
  Vector<BaseFloat> out_bias_;
  Matrix<BaseFloat> class_linearity_;
  Vector<BaseFloat> class_bias_;

  Vector<BaseFloat> in_words_;
  Matrix<BaseFloat> in_words_mat_;
  CuMatrix<BaseFloat> words_;
  CuMatrix<BaseFloat> hidden_out_;

  std::vector<Matrix<BaseFloat> > his_recurrent_; // current hidden lstm layers recurrent history
  std::vector<Matrix<BaseFloat> > his_cell_;	// current hidden lstm layers cell history
  KALDI_DISALLOW_COPY_AND_ASSIGN(KaldiLstmlmWrapper);
};


}  // namespace kaldi

#endif  // KALDI_LM_KALDI_NNLM_H_