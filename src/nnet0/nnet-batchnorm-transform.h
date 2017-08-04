// nnet/nnet-batchnorm-transform.h
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

#ifndef KALDI_NNET_NNET_BATCH_NORMALIZATION_H_
#define KALDI_NNET_NNET_BATCH_NORMALIZATION_H_

#include "nnet/nnet-component.h"
#include "nnet/nnet-various.h"
#include "cudamatrix/cu-math.h"
#include "cudamatrix/cu-matrix.h"
#include "cudamatrix/cu-vector.h"


namespace kaldi {

namespace nnet1 {

class BatchNormTransform: public UpdatableComponent{
public:
	friend class NnetModelSync;

	BatchNormTransform(int32 dim_in, int32 dim_out)
  	:UpdatableComponent(dim_in, dim_out), sigma_(1e-8), alpha_(0.0), time_steps_(0),count_(0), beta1_(0.9), beta2_(0.999), scale_(dim_out), scale_corr_(dim_out), shift_(dim_out), 
    shift_corr_(dim_out),mini_batch_mean_(dim_out),mean_average_(dim_out), mini_batch_variance_(dim_out), variance_pow_invert_average_(dim_out),
    normalize_x_(0,0), scale_first_(dim_out), scale_second_(dim_out),scale_first_corr_(dim_out), scale_second_corr_(dim_out), shift_first_(dim_out), shift_second_(dim_out),
    shift_first_corr_(dim_out), shift_second_corr_(dim_out)
    {}


  ~BatchNormTransform(){}
  void InitData(std::istream &is) {

    // define options

    float bias_mean = 1.0, bias_range = 0.0, param_stddev = 0.0;

    // parse config

    std::string token;

    while (!is.eof()) {

      ReadToken(is, false, &token);

      /**/ if (token == "<ParamStddev>") ReadBasicType(is, false, &param_stddev);

      else if (token == "<BiasMean>")    ReadBasicType(is, false, &bias_mean);

      else if (token == "<BiasRange>")    ReadBasicType(is, false, &bias_range);

      else KALDI_ERR << "Unknown token " << token << ", a typo in config?"

                     << " (ParamStddev|BiasMean|BiasRange)";

      is >> std::ws; // eat-up whitespace

    }

    // initialize

    Vector<BaseFloat> vec_scale(output_dim_);

    for (int32 i=0; i<output_dim_; i++) {

      // +/- 1/2*bias_range from bias_mean:
      vec_scale(i) = bias_mean + (RandUniform() - 0.5) * bias_range;

    }
    scale_ = vec_scale;

    Vector<BaseFloat> vec_shift(output_dim_);

    for (int32 r=0; r<output_dim_; r++) {

        vec_shift(r) = param_stddev * RandGauss(); // 0-mean Gauss with given std_dev

    }

    shift_ = vec_shift;

  }


  void ReadData(std::istream &is, bool binary) {

    if ('<' == Peek(is, binary)) {     
        ExpectToken(is, binary, "<Time>");
        ReadBasicType(is, binary, &time_steps_);
    }

    mean_average_.Read(is, binary);    

    variance_pow_invert_average_.Read(is, binary);

    scale_.Read(is, binary);

    shift_.Read(is, binary);

    KALDI_ASSERT(scale_.Dim() == output_dim_);

    KALDI_ASSERT(shift_.Dim() == output_dim_);

  }


  void WriteData(std::ostream &os, bool binary) const {

    WriteToken(os, binary, "<Time>");

    WriteBasicType(os, binary, time_steps_);

    mean_average_.Write(os, binary);

    variance_pow_invert_average_.Write(os, binary);

    scale_.Write(os, binary);

    shift_.Write(os, binary);

  }


  int32 NumParams() const { return mean_average_.Dim() + variance_pow_invert_average_.Dim() + scale_.Dim() + shift_.Dim(); }



  void GetParams(Vector<BaseFloat>* wei_copy) const {

    wei_copy->Resize(NumParams());

    int32 scale_num_elem = scale_.Dim() ;

    wei_copy->Range(0,scale_num_elem).CopyFromVec(Vector<BaseFloat>(scale_));

    wei_copy->Range(scale_num_elem, shift_.Dim()).CopyFromVec(Vector<BaseFloat>(shift_));

  }



  std::string Info() const {

    return std::string("\n  scale_") + MomentStatistics(scale_) +

           "\n  shift_" + MomentStatistics(shift_);

  }


  std::string InfoGradient() const {

    return std::string("\n  scale_grad") + MomentStatistics(scale_corr_) +

           "\n  shift_grad" + MomentStatistics(shift_corr_);

  }


  Component* Copy() const { return new BatchNormTransform(*this); }


  ComponentType GetType() const { return kBatchNormTransform; }



  void PropagateFnc(const CuMatrixBase<BaseFloat> &in, CuMatrixBase<BaseFloat> *out) {

    if(opts_.train_stage){
        time_steps_++;
        count_++;
        int32 input_rows = in.NumRows() ;

        int32 input_cols = in.NumCols() ;

        normalize_x_.Resize(input_rows, input_cols, kSetZero) ;

	    CuVector<BaseFloat> in_pow2_mean(input_cols, kSetZero) ;

        CuVector<BaseFloat> mini_batch_mean_pow2(input_cols, kSetZero) ;

	    //E(x)
        mini_batch_mean_.SetZero() ;
        mini_batch_mean_.AddRowSumMat(1.0/(BaseFloat)input_rows, in) ;

        //EX^2

        mini_batch_mean_pow2.CopyFromVec(mini_batch_mean_) ;
        mini_batch_mean_pow2.MulElements(mini_batch_mean_) ;

	    //E(x^2)

        CuMatrix<BaseFloat> in_copy(input_rows, input_cols, kSetZero) ;

        in_copy.CopyFromMat(in) ;

        in_copy.MulElements(in) ;

	    in_pow2_mean.AddRowSumMat(1.0/(BaseFloat)input_rows, in_copy) ;
    
	    //variance = E(x^2) - E(x)^2

	    mini_batch_variance_.CopyFromVec(in_pow2_mean) ;

	    mini_batch_variance_.AddVec(-1.0, mini_batch_mean_pow2) ;


        // normalized
        normalize_x_.CopyFromMat(in) ;

        normalize_x_.AddVecToRows(-1.0, mini_batch_mean_) ;

        mini_batch_variance_.Add(sigma_) ;

        mini_batch_variance_.ApplyPow(0.5) ;

        mini_batch_variance_.InvertElements() ;

        normalize_x_.MulColsVec(mini_batch_variance_) ;

        // output

        out->CopyFromMat(normalize_x_) ;

        out->MulColsVec(scale_) ;

        out->AddVecToRows(1.0, shift_) ;

        //moving average
        mean_average_.AddVec((1.0/count_), mini_batch_mean_, ((count_-1.0)/count_));
        
        variance_pow_invert_average_.AddVec((1.0/count_), mini_batch_variance_, ((count_-1.0)/count_));        

    }else{

        out->CopyFromMat(in) ;
        out->AddVecToRows(-1.0, mean_average_);
        out->MulColsVec(variance_pow_invert_average_); 
        out->MulColsVec(scale_) ;
        out->AddVecToRows(1.0, shift_) ; 
    }

  }



  void BackpropagateFnc(const CuMatrixBase<BaseFloat> &in, const CuMatrixBase<BaseFloat> &out,

                        const CuMatrixBase<BaseFloat> &out_diff, CuMatrixBase<BaseFloat> *in_diff) {

    int32 input_rows = in.NumRows() ;

    int32 input_cols = in.NumCols() ;

    CuMatrix<BaseFloat> normalize_x_diff(input_rows, input_cols, kSetZero) ;

    CuMatrix<BaseFloat> in_tmp(input_rows, input_cols, kSetZero) ;

    CuMatrix<BaseFloat> in_tmp_mean(input_rows, input_cols, kSetZero) ;

    CuVector<BaseFloat> variance_diff(input_cols, kSetZero) ;

    CuVector<BaseFloat> mean_diff(input_cols, kSetZero) ;

    CuVector<BaseFloat> in_sub_mean(input_cols, kSetZero) ;

    CuVector<BaseFloat> mini_batch_variance_tmp(input_cols, kSetZero) ;
    //backup mini_batch_variance_ for diff of mean
    mini_batch_variance_tmp.CopyFromVec(mini_batch_variance_) ;
    // diff of normalize_x_

    normalize_x_diff.CopyFromMat(out_diff) ;

    normalize_x_diff.MulColsVec(scale_) ;

    // diff of variance
    in_tmp.CopyFromMat(in) ;

    in_tmp.AddVecToRows(-1.0, mini_batch_mean_) ;
    // backup (x-u) for diff of mean
    in_tmp_mean.CopyFromMat(in_tmp) ;

    in_tmp.MulElements(normalize_x_diff) ;

    mini_batch_variance_.ApplyPow(3.0) ;

    in_tmp.MulColsVec(mini_batch_variance_) ;

    variance_diff.AddRowSumMat(-0.5 , in_tmp) ;

    // diff of mean
    mean_diff.AddRowSumMat(-1.0, normalize_x_diff) ;

    mean_diff.MulElements(mini_batch_variance_tmp) ;

    in_sub_mean.AddRowSumMat(-2.0/(BaseFloat)input_rows, in_tmp_mean) ;

    in_sub_mean.MulElements(variance_diff) ;

    mean_diff.AddVec(1.0, in_sub_mean) ;

    // input_diff
    normalize_x_diff.MulColsVec(mini_batch_variance_tmp) ;

    in_tmp_mean.Scale(2.0/(BaseFloat)input_rows) ;

    in_tmp_mean.MulColsVec(variance_diff) ;

    normalize_x_diff.AddMat(1.0, in_tmp_mean) ;

    normalize_x_diff.AddVecToRows(1.0/(BaseFloat)input_rows, mean_diff) ;

    in_diff->CopyFromMat(normalize_x_diff) ;

}

 void Gradient(const CuMatrixBase<BaseFloat> &input, const CuMatrixBase<BaseFloat> &diff){

      const std::string optimize_method = opts_.optimize_method;
    
      if(optimize_method == "sgd"){
        GradientSgd(input, diff);
      }else if(optimize_method == "adam"){
        GradientAdam(input, diff);
      }else{
        KALDI_LOG <<" unimplement optimize method : "<< optimize_method;
      }   

 }

 void GradientSgd(const CuMatrixBase<BaseFloat> &in, const CuMatrixBase<BaseFloat> &diff){


     const BaseFloat lr = -opts_.learn_rate ;
     const BaseFloat mmt = opts_.momentum;

     //BaseFloat lr_rescale = lr/in.NumRows();
     alpha_ = lr ;
     
     normalize_x_.MulElements(diff) ;

     scale_corr_.AddRowSumMat(1.0, normalize_x_, mmt) ;

     shift_corr_.AddRowSumMat(1.0, diff, mmt) ;


}

void GradientAdam(const CuMatrixBase<BaseFloat> &in, const CuMatrixBase<BaseFloat> &diff){
    
     const BaseFloat lr = -opts_.learn_rate ;
     alpha_ = lr * sqrt(1-pow(beta2_, time_steps_))/(1 - pow(beta1_, time_steps_)); 
    
     normalize_x_.MulElements(diff) ;
     scale_corr_.AddRowSumMat(1.0, normalize_x_, 0.0);
     shift_corr_.AddRowSumMat(1.0, diff, 0.0);

     scale_first_.Scale(beta1_);
     scale_first_.AddVec((1- beta1_), scale_corr_);     
     scale_second_.Scale(beta2_);
     scale_corr_.MulElements(scale_corr_);
     scale_second_.AddVec((1-beta2_), scale_corr_);
     
     shift_first_.Scale(beta1_);
     shift_first_.AddVec((1-beta1_), shift_corr_);
     shift_second_.Scale(beta2_);
     shift_corr_.MulElements(shift_corr_);
     shift_second_.AddVec((1-beta2_), shift_corr_);

     scale_first_corr_.CopyFromVec(scale_first_);
     scale_first_corr_.Scale(1/(1 - pow(beta1_, time_steps_)));
     scale_second_corr_.CopyFromVec(scale_second_);
     scale_second_corr_.Scale(1/(1 - pow(beta2_, time_steps_)));
     scale_second_corr_.ApplyPow(0.5);
     scale_second_corr_.Add(sigma_);
     scale_second_corr_.InvertElements();
     scale_first_corr_.MulElements(scale_second_corr_);
     scale_corr_.CopyFromVec(scale_first_corr_);

     
     shift_first_corr_.CopyFromVec(shift_first_);
     shift_first_corr_.Scale(1/(1 - pow(beta1_, time_steps_)));
     shift_second_corr_.CopyFromVec(shift_second_);
     shift_second_corr_.Scale(1/(1 - pow(beta2_, time_steps_)));
     shift_second_corr_.ApplyPow(0.5);
     shift_second_corr_.Add(sigma_);
     shift_second_corr_.InvertElements();
     shift_first_corr_.MulElements(shift_second_corr_);
     shift_corr_.CopyFromVec(shift_first_corr_);

 
}

void UpdateGradient(){

    //scale_.AddVec(-lr_rescale, scale_corr_) ;
    //shift_.AddVec(-lr_rescale, shift_corr_) ;
    scale_.AddVec(alpha_, scale_corr_) ;
    shift_.AddVec(alpha_, shift_corr_) ;

}

void Update(const CuMatrixBase<BaseFloat> &in, const CuMatrixBase<BaseFloat> &diff){

     const BaseFloat lr = opts_.learn_rate ;
     const BaseFloat mmt = opts_.momentum;

     BaseFloat lr_rescale = lr/in.NumRows();

     normalize_x_.MulElements(diff) ;

     scale_corr_.AddRowSumMat(1.0, normalize_x_, mmt) ;

     shift_corr_.AddRowSumMat(1.0, diff, mmt) ;

     scale_.AddVec(-lr_rescale, scale_corr_) ;

     shift_.AddVec(-lr_rescale, shift_corr_) ;
}

private:

  BaseFloat sigma_ ;

  BaseFloat lr_rescale;

  BaseFloat alpha_;

  int32 time_steps_;
  
  int32 count_ ;

  BaseFloat beta1_ ;
  BaseFloat beta2_ ;

  CuVector<BaseFloat> scale_ ;

  CuVector<BaseFloat> scale_corr_ ;

  CuVector<BaseFloat> shift_ ;

  CuVector<BaseFloat> shift_corr_ ;

  CuVector<BaseFloat> mini_batch_mean_ ;

  CuVector<BaseFloat> mean_average_ ;

  CuVector<BaseFloat> mini_batch_variance_ ;

  CuVector<BaseFloat> variance_pow_invert_average_ ;

  CuMatrix<BaseFloat> normalize_x_ ;

  //for adam
  CuVector<BaseFloat> scale_first_;
  CuVector<BaseFloat> scale_second_;
  CuVector<BaseFloat> scale_first_corr_;
  CuVector<BaseFloat> scale_second_corr_;

  CuVector<BaseFloat> shift_first_;
  CuVector<BaseFloat> shift_second_;
  CuVector<BaseFloat> shift_first_corr_;
  CuVector<BaseFloat> shift_second_corr_;

};


} // namespace nnet1

} // namespace kaldi


#endif
