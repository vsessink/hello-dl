#pragma once
#include "tensor2.hh"
#include <unistd.h>

template<typename T>
struct TensorLayer
{
  TensorLayer() {} 
  TensorLayer(const TensorLayer& rhs) = delete;
  TensorLayer& operator=(const TensorLayer&) = delete;
  
  virtual void randomize() = 0;
  void save(std::ostream& out) const
  {
    for(const auto& p : d_params)
      p->save(out);
  }
  void load(std::istream& in) 
  {
    for(auto& p : d_params)
      p->load(in);
  }
  void learn(float lr) 
  {
    for(auto& p : d_params) {
      auto grad1 = p->getAccumGrad();
      grad1 *= lr;
      *p -= grad1;
    }
  }
  std::vector<Tensor<T>*> d_params;
};

template<typename T, unsigned int IN, unsigned int OUT>
struct Linear : public TensorLayer<T>
{
  Tensor<T> d_weights{OUT, IN};
  Tensor<T> d_bias{OUT, 1};
  
  Linear()
  {
    randomize();
    this->d_params = {&d_weights, &d_bias};
  }
  void randomize() override// "Xavier initialization"  http://proceedings.mlr.press/v9/glorot10a/glorot10a.pdf
  {
    d_weights.randomize(1.0/sqrt(IN));
    d_bias.randomize(1.0/sqrt(IN));
  }

  auto forward(const Tensor<T>& in)
  {
    return d_weights * in + d_bias;
  }
};


template<typename T, unsigned int ROWS, unsigned int COLS, unsigned int KERNEL,
         unsigned int INLAYERS, unsigned int OUTLAYERS>
struct Conv2d : TensorLayer<T>
{
  std::array<Tensor<T>, OUTLAYERS> d_filters;
  std::array<Tensor<T>, OUTLAYERS> d_bias;

  Conv2d()
  {
    for(auto& f : d_filters) {
      f = Tensor<T>(KERNEL, KERNEL);
      this->d_members.push_back(&f);
    }
    for(auto& b : d_bias) {
      b = Tensor<T>(1,1);
      this->d_members.push_back(&b);
    }
        
    randomize();
  }

  void randomize()
  {
    for(auto& f : d_filters) {
      f.randomize(sqrt(1.0/(INLAYERS*KERNEL*KERNEL)));
    }
    for(auto& b : d_bias) {
      b.randomize(sqrt(1.0/(INLAYERS*KERNEL*KERNEL)));
    }
  }

  
  auto forward(Tensor<T>& in)
  {
    std::array<Tensor<T>, 1> a;
    a[0] = in;
    return forward(a);
  }
  
  auto forward(std::array<Tensor<T>, INLAYERS>& in)
  {
    std::array<Tensor<T>, OUTLAYERS> ret;

    for(auto& o : ret)
      o = Tensor<T>(1+ROWS-KERNEL, 1 + COLS - KERNEL);
    
    // The output layers of the next convo2d have OUT filters
    // these filters need to be applied to all IN input layers
    // and the output is the addition of the outputs of those filters
    
    unsigned int ctr = 0;
    for(auto& p : ret) { // outlayers long
      p.zero();
      for(auto& p2 : in)
        p = p +  p2.makeConvo(KERNEL, d_filters.at(ctr), d_bias.at(ctr));
      ctr++;
    }
    return ret;
  }
};

template<typename MS>
void saveModelState(const MS& ms, const std::string& fname)
{
  std::ofstream ofs(fname+".tmp");
  if(!ofs)
    throw std::runtime_error("Can't save model to file "+fname+".tmp: "+strerror(errno));
  ms.save(ofs);
  ofs.flush();
  ofs.close();
  unlink(fname.c_str());
  rename((fname+".tmp").c_str(), fname.c_str());
}

template<typename MS>
void loadModelState(MS& ms, const std::string& fname)
{
  std::ifstream ifs(fname);
  if(!ifs)
    throw std::runtime_error("Can't read model state from file "+fname+": "+strerror(errno));
  ms.load(ifs);
}