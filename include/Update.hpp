/*
 * Update.hpp
 *
 *  Created on: Feb 9, 2014
 *      Author: Bloeschm
 */

#ifndef UPDATEMODEL_HPP_
#define UPDATEMODEL_HPP_

#include <Eigen/Dense>
#include <iostream>
#include "kindr/rotations/RotationEigen.hpp"
#include "ModelBase.hpp"
#include "Prediction.hpp"
#include "State.hpp"

namespace LWF{

template<typename Innovation, typename State, typename Meas, typename Noise>
class Update: public ModelBase<State,Innovation,Meas,Noise>{
 public:
  typedef State mtState;
  typedef typename mtState::mtCovMat mtCovMat;
  typedef Innovation mtInnovation;
  typedef Meas mtMeas;
  typedef Noise mtNoise;
  typename ModelBase<State,Innovation,Meas,Noise>::mtJacInput H_;
  typename ModelBase<State,Innovation,Meas,Noise>::mtJacNoise Hn_;
  typename mtNoise::mtCovMat updnoiP_;
  mtInnovation y_;
  typename mtInnovation::mtCovMat Py_;
  typename mtInnovation::mtCovMat Pyinv_;
  typename mtInnovation::mtDiffVec innVector_;
  const mtInnovation yIdentity_;
  typename mtState::mtDiffVec updateVec_;
  Eigen::Matrix<double,mtState::D_,mtInnovation::D_> K_;
  Eigen::Matrix<double,mtState::D_,mtInnovation::D_> Pxy_;
  SigmaPoints<mtState,2*mtState::D_+1,2*(mtState::D_+mtNoise::D_)+1,0> stateSigmaPoints_;
  SigmaPoints<mtNoise,2*mtNoise::D_+1,2*(mtState::D_+mtNoise::D_)+1,2*mtState::D_> stateSigmaPointsNoi_;
  SigmaPoints<mtInnovation,2*(mtState::D_+mtNoise::D_)+1,2*(mtState::D_+mtNoise::D_)+1,0> innSigmaPoints_;
  SigmaPoints<LWF::VectorState<mtState::D_>,2*mtState::D_+1,2*mtState::D_+1,0> updateVecSP_;
  SigmaPoints<mtState,2*mtState::D_+1,2*mtState::D_+1,0> posterior_;
  Update(){
    resetUpdate();
  };
  void resetUpdate(){
    updateVec_.setIdentity();
    updnoiP_ = mtNoise::mtCovMat::Identity()*0.0001;
    stateSigmaPoints_.computeParameter(1e-3,2.0,0.0);
    stateSigmaPointsNoi_.computeParameter(1e-3,2.0,0.0);
    innSigmaPoints_.computeParameter(1e-3,2.0,0.0);
    updateVecSP_.computeParameter(1e-3,2.0,0.0);
    posterior_.computeParameter(1e-3,2.0,0.0);
    stateSigmaPointsNoi_.computeFromZeroMeanGaussian(updnoiP_);
  }
  virtual ~Update(){};
  int updateEKF(mtState& state, mtCovMat& cov, const mtMeas& meas){
    H_ = this->jacInput(state,meas);
    Hn_ = this->jacNoise(state,meas);
    y_ = this->eval(state,meas);

    // Update
    Py_ = H_*cov*H_.transpose() + Hn_*updnoiP_*Hn_.transpose();
    y_.boxMinus(yIdentity_,innVector_);
    Pyinv_.setIdentity();
    Py_.llt().solveInPlace(Pyinv_);

    // Kalman Update
    K_ = cov*H_.transpose()*Pyinv_;
    cov = cov - K_*Py_*K_.transpose();
    updateVec_ = -K_*innVector_;
    state.boxPlus(updateVec_,state);
    state.fix();
    return 0;
  }
  int updateUKF(mtState& state, mtCovMat& cov, const mtMeas& meas){
    stateSigmaPoints_.computeFromGaussian(state,cov);

    // Update
    for(unsigned int i=0;i<stateSigmaPoints_.L_;i++){
      innSigmaPoints_(i) = this->eval(stateSigmaPoints_(i),meas,stateSigmaPointsNoi_(i));
    }
    y_ = innSigmaPoints_.getMean();
    Py_ = innSigmaPoints_.getCovarianceMatrix(y_);
    Pxy_ = (innSigmaPoints_.getCovarianceMatrix(stateSigmaPoints_)).transpose();
    y_.boxMinus(yIdentity_,innVector_);
    Pyinv_.setIdentity();
    Py_.llt().solveInPlace(Pyinv_); // alternative way to calculate the inverse

    // Kalman Update
    K_ = Pxy_*Pyinv_;
    cov = cov - K_*Py_*K_.transpose();
    updateVec_ = -K_*innVector_;

    // Adapt for proper linearization point
    updateVecSP_.computeFromZeroMeanGaussian(cov);
    for(unsigned int i=0;i<2*mtState::D_+1;i++){
      state.boxPlus(updateVec_+updateVecSP_(i).vector_,posterior_(i));
    }
    state = posterior_.getMean();
    cov = posterior_.getCovarianceMatrix(state);
    return 0;
  }
};

template<typename Innovation, typename State, typename Meas, typename Noise, typename Prediction>
class PredictionUpdate: public ModelBase<State,Innovation,Meas,Noise>{
 public:
  typedef State mtState;
  typedef typename mtState::mtCovMat mtCovMat;
  typedef Innovation mtInnovation;
  typedef Meas mtMeas;
  typedef typename Prediction::mtMeas mtPredictionMeas;
  typedef Noise mtNoise;
  typedef typename Prediction::mtNoise mtPredictionNoise;
  typename ModelBase<State,Innovation,Meas,Noise>::mtJacInput H_;
  typename ModelBase<State,Innovation,Meas,Noise>::mtJacNoise Hn_;
  typename Prediction::mtJacInput F_;
  typename Prediction::mtJacNoise Fn_;
  typename mtNoise::mtCovMat updnoiP_;
  Eigen::Matrix<double,mtPredictionNoise::D_,mtNoise::D_> preupdnoiP_;
  Eigen::Matrix<double,mtPredictionNoise::D_+mtNoise::D_,mtPredictionNoise::D_+mtNoise::D_> noiP_;
  Eigen::Matrix<double,mtState::D_,mtInnovation::D_> C_;
  mtInnovation y_;
  typename mtInnovation::mtCovMat Py_;
  typename mtInnovation::mtCovMat Pyinv_;
  typename mtInnovation::mtDiffVec innVector_;
  const mtInnovation yIdentity_;
  typename mtState::mtDiffVec updateVec_;
  Eigen::Matrix<double,mtState::D_,mtInnovation::D_> K_;
  Eigen::Matrix<double,mtState::D_,mtInnovation::D_> Pxy_;
  SigmaPoints<mtState,2*mtState::D_+1,2*(mtState::D_+mtNoise::D_+mtPredictionNoise::D_)+1,0> stateSigmaPoints_;
  SigmaPoints<mtState,2*(mtState::D_+mtNoise::D_+mtPredictionNoise::D_)+1,2*(mtState::D_+mtNoise::D_+mtPredictionNoise::D_)+1,0> stateSigmaPointsPre_;
  SigmaPoints<PairState<mtPredictionNoise,mtNoise>,2*(mtNoise::D_+mtPredictionNoise::D_)+1,2*(mtState::D_+mtNoise::D_+mtPredictionNoise::D_)+1,2*(mtState::D_)> stateSigmaPointsNoi_;
  SigmaPoints<mtInnovation,2*(mtState::D_+mtNoise::D_+mtPredictionNoise::D_)+1,2*(mtState::D_+mtNoise::D_+mtPredictionNoise::D_)+1,0> innSigmaPoints_;
  SigmaPoints<LWF::VectorState<mtState::D_>,2*mtState::D_+1,2*mtState::D_+1,0> updateVecSP_;
  SigmaPoints<mtState,2*mtState::D_+1,2*mtState::D_+1,0> posterior_;
  PredictionUpdate(){
    resetUpdate();
  };
  void resetUpdate(){
    updateVec_.setIdentity();
    updnoiP_ = mtNoise::mtCovMat::Identity()*0.0001;
    preupdnoiP_.setZero();
    noiP_.setIdentity();
    stateSigmaPoints_.computeParameter(1e-3,2.0,0.0);
    stateSigmaPointsPre_.computeParameter(1e-3,2.0,0.0);
    stateSigmaPointsNoi_.computeParameter(1e-3,2.0,0.0);
    innSigmaPoints_.computeParameter(1e-3,2.0,0.0);
    updateVecSP_.computeParameter(1e-3,2.0,0.0);
    posterior_.computeParameter(1e-3,2.0,0.0);
  }
  virtual ~PredictionUpdate(){};
  int predictAndUpdateEKF(mtState& state, mtCovMat& cov, const mtMeas& meas, Prediction& prediction, const mtPredictionMeas& predictionMeas, double dt){
    // Predict
    F_ = prediction.jacInput(state,predictionMeas,dt);
    Fn_ = prediction.jacNoise(state,predictionMeas,dt);
    state = prediction.eval(state,predictionMeas,dt);
    state.fix();
    cov = F_*cov*F_.transpose() + Fn_*prediction.prenoiP_*Fn_.transpose();

    // Update
    H_ = this->jacInput(state,meas);
    Hn_ = this->jacNoise(state,meas);
    C_ = Fn_*preupdnoiP_*Hn_.transpose();
    y_ = this->eval(state,meas);
    Py_ = H_*cov*H_.transpose() + Hn_*updnoiP_*Hn_.transpose() + H_*C_ + C_.transpose()*H_.transpose();
    y_.boxMinus(yIdentity_,innVector_);
    Pyinv_.setIdentity();
    Py_.llt().solveInPlace(Pyinv_);

    // Kalman Update
    K_ = (cov*H_.transpose()+C_)*Pyinv_;
    cov = cov - K_*Py_*K_.transpose();
    updateVec_ = -K_*innVector_;
    state.boxPlus(updateVec_,state);
    state.fix();
    return 0;
  }
  int predictAndUpdateUKF(mtState& state, mtCovMat& cov, const mtMeas& meas, Prediction& prediction, const mtPredictionMeas& predictionMeas, double dt){
    // Predict
    noiP_.template block<mtPredictionNoise::D_,mtPredictionNoise::D_>(0,0) = prediction.prenoiP_;
    noiP_.template block<mtPredictionNoise::D_,mtNoise::D_>(0,mtPredictionNoise::D_) = preupdnoiP_;
    noiP_.template block<mtNoise::D_,mtPredictionNoise::D_>(mtPredictionNoise::D_,0) = preupdnoiP_.transpose();
    noiP_.template block<mtNoise::D_,mtNoise::D_>(mtPredictionNoise::D_,mtPredictionNoise::D_) = updnoiP_;
    stateSigmaPointsNoi_.computeFromZeroMeanGaussian(noiP_);
    stateSigmaPoints_.computeFromGaussian(state,cov);

    // Prediction
    for(unsigned int i=0;i<stateSigmaPointsPre_.L_;i++){
      stateSigmaPointsPre_(i) = prediction.eval(stateSigmaPoints_(i),predictionMeas,stateSigmaPointsNoi_(i).first(),dt);
    }

    // Calculate mean and variance
    state = stateSigmaPointsPre_.getMean();
    state.fix();
    cov = stateSigmaPointsPre_.getCovarianceMatrix(state);

    // Update
    for(unsigned int i=0;i<innSigmaPoints_.L_;i++){
      innSigmaPoints_(i) = this->eval(stateSigmaPointsPre_(i),meas,stateSigmaPointsNoi_(i).second());
    }
    y_ = innSigmaPoints_.getMean();
    Py_ = innSigmaPoints_.getCovarianceMatrix(y_);
    Pxy_ = (innSigmaPoints_.getCovarianceMatrix(stateSigmaPointsPre_)).transpose();
    y_.boxMinus(yIdentity_,innVector_);
    Pyinv_.setIdentity();
    Py_.llt().solveInPlace(Pyinv_); // alternative way to calculate the inverse

    // Kalman Update
    K_ = Pxy_*Pyinv_;
    cov = cov - K_*Py_*K_.transpose();
    updateVec_ = -K_*innVector_;

    // Adapt for proper linearization point
    updateVecSP_.computeFromZeroMeanGaussian(cov);
    for(unsigned int i=0;i<2*mtState::D_+1;i++){
      state.boxPlus(updateVec_+updateVecSP_(i).vector_,posterior_(i));
    }
    state = posterior_.getMean();
    cov = posterior_.getCovarianceMatrix(state);
    return 0;
  }
};

}

#endif /* UPDATEMODEL_HPP_ */
