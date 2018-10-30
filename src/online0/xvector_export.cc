// online0/xvector_export.cc

// Copyright 2018	Alibaba Inc (author: Wei Deng)

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


#include "online0/xvector_export.h"
#include "online0/online-xvector-extractor.h"

using namespace kaldi;

void *CreateXvectorExtractor(const char *cfg_path) {
	#ifdef __DEBUG
	    printf("create decoder, config path: %s.\n", cfg_path);
	#endif
	std::string cfg(cfg_path);
	OnlineXvectorExtractor *extractor = nullptr;

    extractor = new OnlineXvectorExtractor(cfg);
    if (nullptr == extractor) {
        fprintf(stderr, "create decoder failed, config path: %s\n", cfg_path);
        printf("%s\n",strerror(errno));
        return nullptr;
    }

	extractor->InitExtractor();
	return (void *)extractor;
}

int	XvectorExtractorFeedData(void *lp_extractor, void *data, int nbytes, int type, int state) {
	OnlineXvectorExtractor *extractor = (OnlineXvectorExtractor *)lp_extractor;
    if (state==FEAT_START)
    	extractor->Reset();

    int num_samples;
    float *audio = nullptr;

    if (nbytes <= 0)
        return nbytes;

    if (0 == type) { // wav data
    	num_samples = nbytes / sizeof(short);
    	audio = new float[num_samples];
    	for (int i = 0; i < num_samples; i++)
    		audio[i] = ((short *)data)[i];
    } else { // default raw feature(fbank, plp, mfcc...)
    	num_samples = nbytes / sizeof(float);
    	audio = new float[num_samples];
        if (num_samples > 0)
    	    memcpy(audio, data, num_samples*sizeof(float));
    }

    extractor->FeedData(audio, num_samples*sizeof(float), (kaldi::FeatState)state);
    delete [] audio;
    return nbytes;
}

int GetXvectorDim(void *lp_extractor) {
    OnlineXvectorExtractor *extractor = (OnlineXvectorExtractor *)lp_extractor;
    return extractor->GetXvectorDim();
}

int GetCurrentXvector(void *lp_extractor, float *result, int type) {
	OnlineXvectorExtractor *extractor = (OnlineXvectorExtractor *)lp_extractor;
    int dim = 0;
	Xvector *xvector = extractor->GetCurrentXvector(type);

    if (nullptr != xvector) {
	    dim = xvector->xvector_.Dim();
        if (dim > 0)
            memcpy(result, xvector->xvector_.Data(), dim*sizeof(float));
    }
	return dim;
}

float GetScore(void *lp_extractor, float *enroll, float *eval,
		int size, int enroll_num, int type) {
	OnlineXvectorExtractor *extractor = (OnlineXvectorExtractor *)lp_extractor;
	SubVector<BaseFloat> vec1(enroll, size);
	SubVector<BaseFloat> vec2(eval, size);
	return extractor->GetScore(vec1, enroll_num, vec2, type);
}

int GetEnrollSpeakerXvector(void *lp_extractor, float *spk_ivec, float *ivecs,
		int ivec_dim, int num_ivec, int type) {
	// Unimplemented now
	return 0;
}

void ResetXvectorExtractor(void *lp_extractor) {
	OnlineXvectorExtractor *extractor = (OnlineXvectorExtractor *)lp_extractor;
	extractor->Reset();
}

void DestroyXvectorExtractor(void *lp_extractor) {
#ifdef __DEBUG
    printf("destroy xvector extractor instance.\n");
#endif
    if (lp_extractor != nullptr) {
        delete (OnlineXvectorExtractor *)lp_extractor;
        lp_extractor = nullptr;
    }
}

