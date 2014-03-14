#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cblas.h>

#include "nn.h"
#include "layer.h"
#include "common.h"

void NN::Init() {     
  for (int i = 0; i < nn_para_->layer_para_.size(); ++i) {
    Layer* new_layer = GetLayer(nn_para_, nn_para_->layer_para_[i]);    
    layers_.push_back(new_layer);    
  }

  for (int i = 0; i < nn_para_->layer_para_.size(); ++i) {
    if (i == 0) {    
      layers_[i]->Init(NULL, layers_[i+1]);  
    } else if (i == nn_para_->layer_para_.size() - 1) {
      layers_[i]->Init(layers_[i-1], NULL);  
    } else {      
      layers_[i]->Init(layers_[i-1], layers_[i+1]);  
    }
  }

  std::cout << "Network Initialization succeeded" << std::endl;
}


void NN::RandPerm(int *randperm, int size) {  
  for (int i = 0; i < size; ++i) {
    randperm[i] = i;
  }
  for (int i = 0; i < size; ++i) {
    int j = rand()%(size - i) + i;
    int t = randperm[j];
    randperm[j] = randperm[i];
    randperm[i] = t;
  }
}


void NN::Train() {
  for (int e = 0; e < epoch_num_; ++e) {
    for (int i = 0; i < sample_size_; ++i) {                    

      layers_[0]->input_ = data_im_[i];
      for (int j = 0; j < layers_.size(); ++j) {        
        layers_[j]->Forward();               
      }
        
      layers_[layers_.size() - 1]->label_ = labels_[i];
      for (int j = layers_.size() - 1; j >= 0; --j) {
        layers_[j]->Backward();            
      }

      for (int j = 0; j < layers_.size(); ++j) {   
        layers_[j]->GetGradient();                             
      }
      
      if (i % nn_para_->mb_size_ == 0) {
        for (int j = 0; j < layers_.size(); ++j) {   
          layers_[j]->Update();               
        }
        std::cout << dynamic_cast<FullLayer*>(layers_[4])->bias_[0] << std::endl;      
      }      
    }

    std::cout << "train!" << std::endl;
  }  
}



