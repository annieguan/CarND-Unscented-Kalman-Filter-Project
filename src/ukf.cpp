#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.8;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.6;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

   //set state dimension
  n_x_ = 5;

  //set augmented dimension
  n_aug_ = 7;

  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;

  //define spreading parameter
  lambda_ = 3 - n_aug_;

  //initial position
  x_ << 0.1, 0.1, 0.1, 0.1, 0.01;

  //initial state covariance matrix
  P_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 0, 1;

  R_laser_ = MatrixXd(2,2);
  R_radar_ = MatrixXd(3,3);

  
  // set weights
  weights_= VectorXd(2*n_aug_+1);
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }
  
  Xsig_pred_= MatrixXd(n_x_, 2 * n_aug_ + 1);
  Xsig_pred_.fill(0.0);
}



UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

  if (!is_initialized_)
  {
    if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_)
    {
      x_(0) = meas_package.raw_measurements_(0);
      x_(1) = meas_package.raw_measurements_(1);
      x_(2) = 0;
      x_(3) = 0;
      x_(4) = 0;
    }
    
    
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_)
    {
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rho_dot = meas_package.raw_measurements_(2);

      x_(0) = rho * cos(phi);
      x_(1) = rho * sin(phi);
      x_(2) = rho_dot;
      x_(3) = phi;
      x_(4) = 0;
     
    }
    
    time_us_ = meas_package.timestamp_;

    R_laser_ << std_laspx_*std_laspx_, 0,
        0, std_laspy_*std_laspy_;

    R_radar_ <<  std_radr_*std_radr_, 0, 0,
                 0, std_radphi_*std_radphi_, 0,
                 0, 0,std_radrd_*std_radrd_;
    is_initialized_ = true;
    return;
  }
  
   // calculate the time difference of the measurement to the current state
  long delta_t_us = meas_package.timestamp_ - time_us_;
  double delta_t= delta_t_us /1e6 ;

 
  time_us_ = meas_package.timestamp_;

  //std::cout << "delta_t = " << delta_t << std::endl;
  //std::cout << "meas_package.timestamp_ = " << meas_package.timestamp_ << std::endl;
  std::cout << "meas_package.sensor_type_ = " << meas_package.sensor_type_ << std::endl;

  //Do incremental updates to prevent mumerical unstability
  while (delta_t > 0.1)
  {
    Prediction(0.05);
    delta_t -=0.05;
  }
  

  // call the prediction function
  Prediction(delta_t);

  // distinguish between radar measurement and laser measurement for update call
  if(meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_) {
    UpdateLidar(meas_package);
  }
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_) {
    UpdateRadar(meas_package);
  }


}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  // build augmented vector
  VectorXd x_aug = VectorXd(7);
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  // build augmented state covariance
  MatrixXd P_aug_ = MatrixXd(7, 7);
  P_aug_.fill(0.0);
  P_aug_.topLeftCorner(5,5) = P_;
  P_aug_(5,5) = std_a_*std_a_;
  P_aug_(6,6) = std_yawdd_*std_yawdd_;
  MatrixXd L = P_aug_.llt().matrixL();

   //create augmented sigma points
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }

  //predict sigma points
  Xsig_pred_= MatrixXd(n_x_, 2 * n_aug_ + 1);
  
  for (int i = 0; i< 2*n_aug_+1; i++)
  {
    //extract values for better readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }

  std::cout << "Xsig_pred_ = " << std::endl << Xsig_pred_ << std::endl;

  //Predict Mean and Covariance

  // predicted state mean
  x_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  // 2n+1 simga points
    x_ = x_+ weights_(i) * Xsig_pred_.col(i);
  }

  // predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  // 2n+1 simga points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
   
    //angle normalization
    
    while (x_diff(3)> M_PI) { x_diff(3)-=2.*M_PI; }
    while (x_diff(3)<-M_PI) { x_diff(3)+=2.*M_PI; }

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;

  }

  std::cout << "Predicted state ukf" << std::endl;
  std::cout << x_ << std::endl;

  std::cout << "Predicted covariance matrix ukf" << std::endl;
  std::cout << P_ << std::endl;

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  MatrixXd Xi_z = Xsig_pred_.topRows(2);

   // predict measurement
  VectorXd z_pred = VectorXd(2);
  z_pred.fill(0.0);

  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Xi_z.col(i);
  }

   // measurement covariance matrix S
   MatrixXd S = MatrixXd(2,2);
   S.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {  // 2n+1 simga points
     S = S + weights_(i) * ( Xi_z.col(i) - z_pred ) * ( Xi_z.col(i) - z_pred ).transpose();
   }
   
   S = S + R_laser_;



   // cross correlation Tc
   MatrixXd Tc = MatrixXd(n_x_,2);
   Tc.fill(0.0);
   for (int i = 0; i < 2 * n_aug_ + 1; i++) {  // 2n+1 simga points
     Tc = Tc + weights_(i) * ( Xsig_pred_.col(i) - x_ ) * ( Xi_z.col(i) - z_pred ).transpose();
   }

   
   // Kalman gain K;
   MatrixXd K = Tc * S.inverse();

   //std::cout << "K: " << K << std::endl;

   // update
   VectorXd z = meas_package.raw_measurements_;
   x_ = x_ + K * (z - z_pred);
   P_ = P_ - K*S*K.transpose();

   std::cout << "Updated state x_: " << x_ << std::endl;
   std::cout << "Updated state covariance P_: " << std::endl;
   std::cout <<  P_ << std::endl;
 

   VectorXd z_diff = z-z_pred;
   NIS_laser_ = z_diff.transpose()*S.inverse()*z_diff;



}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  

  //transform sigma points into measurement space
  MatrixXd Zsig = MatrixXd(3, 2 * n_aug_ + 1);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    // extract values for better readibility
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    
    if (p_x == 0 && p_y == 0)
    {
       return;
      
    }
     
    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig(1,i) = atan2(p_y,p_x);                                 //phi
    Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }


  //mean predicted measurement
  VectorXd z_pred = VectorXd(3);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  //measurement covariance matrix S
  MatrixXd S = MatrixXd(3,3);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
   
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
   
    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  
  S = S + R_radar_;

  
  //Update State

  // cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_,3);
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  // 2n+1 simga points
    Tc = Tc + weights_(i) * ( Xsig_pred_.col(i) - x_ ) * ( Zsig.col(i) - z_pred ).transpose();
  }


  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  

  // update state mean and covariance matrix
  VectorXd z = meas_package.raw_measurements_;
  x_ = x_ + K * (z - z_pred);
  P_ = P_ - K*S*K.transpose();


  std::cout << "Updated state x_: " << x_ << std::endl;
  std::cout << "Updated state covariance P_: " << std::endl;
  std::cout <<  P_ << std::endl;

  VectorXd z_diff = z-z_pred;
  NIS_radar_ = z_diff.transpose()*S.inverse()*z_diff;

}
