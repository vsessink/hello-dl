#pragma once
#include <vector>
#include <random>
#include "tracked.hh"

template<typename T, unsigned int ROWS, unsigned int COLS>
struct SArray
{
  SArray()
  {
    d_store.resize(ROWS*COLS);
  }
  std::vector<T> d_store;
  T& operator()(int x, int y)
  {
    return d_store.at(x*COLS + y);
  }
  const T& operator()(int x, int y) const
  {
    return d_store.at(x*COLS + y);
  }

  auto operator+=(const SArray<T, ROWS, COLS>& rhs)
  {
    for(size_t pos = 0 ; pos < rhs.d_store.size(); ++pos)
      d_store[pos] += rhs.d_store[pos];
    return *this;
  }

  auto operator/=(float val)
  {
    for(auto& v : d_store)
      v/=val;
    return *this;
  }
  auto operator*=(float val)
  {
    for(auto& v : d_store)
      v*=val;
    return *this;
  }
  
};


template<typename T, unsigned int ROWS, unsigned int COLS>
struct NNArray
{
  NNArray()
  {
    d_store.resize(ROWS*COLS);
  }
  std::vector<TrackedNumber<T>> d_store;

  TrackedNumber<T>& operator()(int x, int y)
  {
    return d_store.at(x*COLS + y);
  }

  
  const TrackedNumber<T>& operator()(int x, int y) const
  {
    return d_store.at(x*COLS + y);
  }

  auto getCols() const
  {
    return COLS;
  }
  auto getRows() const
  {
    return ROWS;
  }

  auto getGrad() const
  {
    SArray<T, ROWS, COLS> ret;
    ret.d_store.resize(d_store.size());
    for(size_t pos = 0 ; pos < d_store.size(); ++pos)
      ret.d_store[pos] = d_store[pos].getGrad();
    return ret;
  }

  auto& operator-=(const SArray<T, ROWS, COLS>& rhs)
  {
    // this changes the contents of weights to a new numerical value, based on the old one
    // by doing it like this, tracking is retained
    for(size_t pos = 0 ; pos < d_store.size(); ++pos)
      d_store[pos] = d_store[pos].getVal() - rhs.d_store[pos];

    return *this;
  }

  
  void randomize(T fact=1.0)
  {
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<> d{0, 1};

    for(auto& item : d_store) {
      item = (float)d(gen)*fact;
    }
  }

  void zero()
  {
    constant(0);
  }
  void constant(T val)
  {
    for(auto& item : d_store) {
      item = val;
    }
  }

  template<typename F>
  auto applyFunc([[maybe_unused]] const F& f)
  {
    NNArray<T, ROWS, COLS> ret;
    
    ret.d_store.clear(); 
    for(const auto& v : d_store)
      ret.d_store.push_back(makeFunc(v, f));
    return ret;
  }

  // does it in ALL fields, not by row
  auto norm() 
  {
    NNArray<T, ROWS, COLS> ret;
    TrackedNumber<T> sum=0;

    for(const auto& v : d_store)
      sum = sum + v;
    for(unsigned int pos = 0 ; pos < ret.d_store.size() ; ++pos)
      ret.d_store[pos] = d_store[pos]/sum;
    return ret;
  }

  
  // does it in ALL fields, not by row
  auto logSoftMax() 
  {
    NNArray<T, ROWS, COLS> ret;
    TrackedNumber<T> sum=0;

    // has the problem that it can exceed the max exponentiable value
    //c = x.max()
    //logsumexp = np.log(np.exp(x - c).sum())
    //return x - c - logsumexp

    TrackedNumber<T> lemax=d_store.at(0);
    for(size_t pos = 1; pos < d_store.size(); ++pos)
      lemax = makeMax(lemax, d_store[pos]);
    
    for(const auto& v : d_store)
      sum = sum + makeFunc(v - lemax, ExpFunc());
    TrackedNumber<T> logsum = makeFunc(sum, LogFunc());
    for(unsigned int pos = 0 ; pos < ret.d_store.size() ; ++pos)
      ret.d_store[pos] = d_store[pos] - lemax - logsum;
    return ret;
  }
  
  auto flatViewRow() const
  {
    NNArray<T, ROWS*COLS, 1> ret;
    ret.d_store = d_store;
    return ret;
  }
  auto flatViewCol() const
  {
    NNArray<T, 1, ROWS*COLS> ret;
    ret.d_store = d_store;
    return ret;
  }
  
  TrackedNumber<T> sum()
  {
    TrackedNumber<T> ret{0};
    for(auto& item : d_store) {
      ret = ret + item;
    }
    return ret;
  }

  TrackedNumber<T> mean()
  {
    return sum() / TrackedNumber<T>((float)d_store.size());
  }

  auto getMeanStd() // numerical recipes 14.1
  {
    std::pair<T, T> ret{0,0};
    for(auto& item : d_store) {
      ret.first += item.getVal();
    }
    ret.first /= d_store.size(); // have mean now
    T diffsum=0, diff2sum=0;
    for(auto& item : d_store) {
      auto diff= (item.getVal() - ret.first);
      diff2sum += diff*diff;
      diffsum += diff;

    }
    diffsum *= diffsum;
    diffsum /= d_store.size();
    ret.second = sqrt( (diff2sum - diffsum) / (d_store.size() -1));
    return ret; 
  }
  
  // goes down a column to find the row with the x-est value
  unsigned int xValueIndexOfColumn(int col, float fact)
  {
    float xval=fact*(*this)(0, col).getVal();
    int xrow=0;
    for(unsigned int r=0; r < ROWS; ++r) {
      //      std::cout<<r<<std::endl;
      float val = fact*(*this)(r, col).getVal();
      if(val > xval) {
        xval = val;
        xrow=r;
      }
    }
    return xrow;
  }

  // goes down a column to find the row with the highest value
  unsigned int maxValueIndexOfColumn(int col)
  {
    return xValueIndexOfColumn(col, 1.0);
  }
  // goes down a column to find the row with the highest value
  unsigned int minValueIndexOfColumn(int col)
  {
    return xValueIndexOfColumn(col, -1.0);
  }

  
  // *this is ROWS*COLS
  // a is COLS*N
  
  template<unsigned int N>
  NNArray<T, ROWS, N>
  operator*(const NNArray<T, COLS, N>& a) const
  {
    NNArray<T, ROWS, N> ret;
    //    std::cout << "N " <<N << " ROWS " << ROWS <<" COLS "<<COLS<<std::endl;

    for (size_t i = 0; i < ROWS; ++i) {
      for (size_t j = 0; j < N; ++j) {
        ret(i,j) = 0;
        for (size_t k = 0; k < COLS; ++k) {
          //          std::cout << i<<","<<j<<","<<k<< std::endl;
          // XXX bit worried over the below
          ret(i,j) = ret(i,j) + (*this)(i,k) * a(k,j);
          //std::cout<< ret(i,j).getVal() << std::endl;
        }
      }
    }
    
    return ret;
  }

  void zeroGrad()
  {
    for(auto& v : d_store)
      v.zeroGrad();
  }

  NNArray<T, ROWS, COLS> elMult(NNArray<T, ROWS, COLS>& w)
  {
    NNArray<T, ROWS, COLS> ret;
    for(size_t pos = 0 ; pos < d_store.size(); ++pos)
      ret.d_store[pos] = d_store[pos] * w.d_store[pos];
    return ret;
  }
  
  template<unsigned int KERNEL>
  NNArray<T, 1+ROWS-KERNEL, 1+COLS-KERNEL>
  Convo2d(NNArray<T, KERNEL, KERNEL>& weights, NNArray<T, 1, 1>& bias)
  {
    NNArray<T, 1+ROWS-KERNEL, 1+COLS-KERNEL> ret;
    NNArray<T, KERNEL, KERNEL> kernel;

    for(unsigned int r=0; r < 1+ROWS-KERNEL; ++r) {
      for(unsigned int c=0; c < 1+COLS-KERNEL; ++c) {
        for(unsigned int kr=0; kr < KERNEL; ++kr) {
          for(unsigned int kc=0; kc < KERNEL; ++kc) { 
            kernel(kr,kc) = (*this)(r + kr, c + kc);
          }
        }
        ret(r,c) = kernel.elMult(weights).sum() + bias(0,0);
      }
    }
    
    return ret;
  }

  template<unsigned int KERNEL>
  NNArray<T, ROWS/KERNEL, COLS/KERNEL>
  Max2d()
  {
    NNArray<T, ROWS/KERNEL, ROWS/KERNEL> ret;
    NNArray<T, KERNEL, KERNEL> kernel;

    for(unsigned int r=0; r < ROWS/KERNEL; ++r) {
      for(unsigned int c=0; c < COLS/KERNEL; ++c) {
        TrackedNumber<T> max = (*this)(r*KERNEL, c*KERNEL);
        for(unsigned int kr=0; kr < KERNEL; ++kr) {
          for(unsigned int kc=0; kc < KERNEL; ++kc) { 
            max = makeMax(max, (*this)(r*KERNEL+kr,c*KERNEL+kc));
          }
        }
        ret(r,c) = max;
      }

              
    }
    
    return ret;
  }

  
};

template<typename T, unsigned int ROWS, unsigned int COLS>
NNArray<T, ROWS, COLS> operator-(const NNArray<T, ROWS, COLS>& lhs, const NNArray<T, ROWS, COLS>& rhs)
{
  NNArray<T, ROWS, COLS> ret;
  for(size_t pos = 0 ; pos < lhs.d_store.size(); ++pos)
    ret.d_store[pos] = lhs.d_store[pos]  - rhs.d_store[pos];
  
  return ret;
}

template<typename T, unsigned int ROWS, unsigned int COLS>
NNArray<T, ROWS, COLS> operator+(const NNArray<T, ROWS, COLS>& lhs, const NNArray<T, ROWS, COLS>& rhs)
{
  NNArray<T, ROWS, COLS> ret;
  for(size_t pos = 0 ; pos < lhs.d_store.size(); ++pos)
    ret.d_store[pos] = lhs.d_store[pos] + rhs.d_store[pos];
  
  return ret;
}

template<typename T, unsigned int ROWS, unsigned int COLS>
std::ostream& operator<<(std::ostream& os, const NNArray<T, ROWS, COLS>& ns)
{
  for(size_t r = 0; r < ROWS; ++r) {
    for(size_t c = 0; c < COLS; ++c) {
      if(c)
        os<<' ';
      os<< ns(r,c).getVal();
    }
    os<<'\n';
  }
      

  return os;
}


template<typename T, unsigned int ROWS, unsigned int COLS>
std::ostream& operator<<(std::ostream& os, const SArray<T, ROWS, COLS>& ns)
{
  for(size_t r = 0; r < ROWS; ++r) {
    for(size_t c = 0; c < COLS; ++c) {
      if(c)
        os<<' ';
      os<< ns(r,c);
    }
    os<<'\n';
  }
      

  return os;
}

