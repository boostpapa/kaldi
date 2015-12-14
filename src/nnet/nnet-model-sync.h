// nnet/nnet-model-sync.h

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

#ifndef NNET_NNET_MODEL_SYNC_H_
#define NNET_NNET_MODEL_SYNC_H_

#include "thread/kaldi-semaphore.h"
#include "thread/kaldi-mutex.h"
#include "nnet/nnet-nnet.h"

#include "cudamatrix/cu-device.h"
#include <mpi.h>

namespace kaldi {
namespace nnet1 {

struct NnetParallelOptions{
	int32 num_threads;
	int merge_size;
	int num_merge;
	int num_procs;
	int myid;
	int thread_level;
	bool asgd_lock;
	std::string merge_func;
	std::string log_file;


	NnetParallelOptions():
									 num_threads(1),
									 merge_size(400000),
									 num_merge(0),
									 num_procs(-1),
									 myid(0),
									 thread_level(0),
									 asgd_lock(true),
									 merge_func("globalada"),
									 log_file("")
									 { }

	  void Register(OptionsItf *po) {
		  po->Register("num-threads", &num_threads, "Number of threads(GPUs) to use");
		  po->Register("asgd-lock", &asgd_lock, "Apply lock on asgd training.");

	      if (this->num_procs >= 1)
	      {
	          po->Register("merge-size",&merge_size, "Multi-machine merge size");
	          po->Register("merge-function", &merge_func, "Multi-machine merge function");
	          po->Register("log-file", &log_file, "Each job log.");
	      }

	  }
};

class ModelAverageMerge;
class ModelGlobalSumMerge;
class ModelGlobalAdagradMerge;
class ModelMergeFunction;

class NnetModelSync{
public:
	NnetModelSync(Nnet *nnet, const NnetParallelOptions *opts=NULL):
		initialized_(false),data_(NULL),free_data_(NULL),dim_(0),nnet(nnet),
		thread_idx_(0),data_thread_idx_(0),opts_(opts),p_merge_func_(NULL)
	{
		//Init(nnet);
		MultiMachineInit();
	}

	~NnetModelSync(){Destory();}

	void LockModel() {
		model_mutex_.Lock();
	}
	void UnlockModel(){
		model_mutex_.Unlock();
	}

	void LockStates() {
		stats_mutex_.Lock();
	}
	void UnlockStates(){
		stats_mutex_.Unlock();
	}

	void GetWeight(Nnet *nnet);

	void SetWeight(Nnet *nnet);

	void Destory();

	int32 Dim(){return this->dim_;};

	void CopyToHost(Nnet *nnet)
	{
		*(this->nnet) = *nnet;
	}

	int32 GetThreadIdx()
	{
		return thread_idx_++;
	}

	int32 GetDataThreadIdx()
	{
		return data_thread_idx_++;
	}

	ModelMergeFunction *GetModelMergeFunction()
	{
		return p_merge_func_;
	}

	void MultiMachineInit();

	void Initialize(Nnet *nnet)
	{
		model_mutex_.Lock();
		if (!initialized_)
		{
			this->GetWeight(nnet);
			InitMergeFunction();
			initialized_ = true;
		}
		model_mutex_.Unlock();
	}

private:
	friend class ModelAverageMerge;
	friend class ModelGlobalSumMerge;
	friend class ModelGlobalAdagradMerge;


	int32 GetDim(Nnet *nnet);
	void Init(Nnet *nnet);
	void InitMergeFunction();

	bool	initialized_;
	Mutex model_mutex_;
	Mutex stats_mutex_;
	BaseFloat *data_;
	BaseFloat *free_data_;
	int32 dim_;
	Nnet *nnet;
	int32 thread_idx_;
	int32 data_thread_idx_;
	const NnetParallelOptions *opts_;
	ModelMergeFunction *p_merge_func_;

public:

#if HAVE_CUDA == 1
  kaldi::MPIGpuInfo *gpuinfo_;
  MPI_Win win;
#endif
};



class NnetParallelUtil{
public:
	std::string AddSuffix(std::string filename, int idx);
	std::string FAddSuffix(std::string filename, int idx);
	std::string GetFilename(std::string filename);
	int NumofMerge(std::string fn, int merge_size);
	int NumofCEMerge(std::string fn, int merge_size);
};



} // namespace nnet
} // namespace kaldi

#endif /* NNET_NNET_MODEL_SYNC_H_ */
