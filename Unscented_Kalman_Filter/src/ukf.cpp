#include "ukf.h"
#include "Eigen/Dense"

using Eigen::MatrixXd;
using Eigen::VectorXd;

// I had problems using namespace std. Do not use it.
/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  // x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // I had a problem with core dump, so I'm trying here a solution from Knowledge:
  P_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 0.5, 0,
        0, 0, 0, 0, 0.5;

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 0.5;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.8;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

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

  is_initialized_ = false;


  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
  n_x_ = 5;
  n_aug_ = n_x_ + 2;
  x_ = VectorXd(n_x_);
  // P_ = MatrixXd(n_x_, n_x_);
  
  
  lambda_ = 3 - n_x_;
  
 

  // Zsig_ = MatrixXd::Zero(n_z_, 2 * n_aug_ + 1);
  Xsig_pred_ = MatrixXd::Zero(n_x_, 2 * n_aug_ + 1);

  weights_ = VectorXd::Zero(2 * n_aug_ + 1);
  // weights_.fill(0.5 / (lambda_ + n_aug_));
  // weights_(0) = lambda_/(lambda_+n_aug_);

  R_radar_ = MatrixXd(3, 3);
  R_radar_ << std_radr_ * std_radr_, 0, 0,
                    0, std_radphi_ * std_radphi_, 0,
                    0, 0, std_radrd_ * std_radrd_;

  R_lidar_ = MatrixXd(2, 2);
  R_lidar_ << std_laspx_ * std_laspx_, 0,
            0, std_laspy_ * std_laspy_;



  NIS_radar_ = 0;
  NIS_laser_ = 0;

  prev_timestamp = 0;

}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */

  if (meas_package.sensor_type_ == MeasurementPackage::RADAR && !use_radar_) {
        return;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER && !use_laser_) {
        return;
    }

  if (!is_initialized_) {
    if(meas_package.sensor_type_ == MeasurementPackage::RADAR){
      // Convert RADAR: Polar -> Cartesian.
      auto rho = static_cast<float>(meas_package.raw_measurements_(0));
      auto phi = static_cast<float>(meas_package.raw_measurements_(1));
      // auto rho_dot = static_cast<float>(meas_package.raw_measurements_(2));
      
      // Equations to keep x simple
      // double x = rho * cos(phi);
      // double y = rho * sin(phi);
      // double vx = rho_dot * cos(phi);
      // double vy = rho_dot * sin(phi);
      // double v = sqrt(vx * vx + vy * vy);

      x_ << rho * cos(phi), rho * sin(phi), 0, 0, 0;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      x_ << meas_package.raw_measurements_ [0], meas_package.raw_measurements_[1], 0, 0, 0;
    }
    // Timestamp
    prev_timestamp = meas_package.timestamp_;
    is_initialized_ = true;
    return;
  }

  auto dt = static_cast<double>((meas_package.timestamp_- prev_timestamp) * 1e-6); // or divided by 1000000.0
  prev_timestamp = meas_package.timestamp_;

  while (dt > 0.1) {
    constexpr double delta_t = 0.05;
    Prediction(delta_t);
    dt -= delta_t;
  }

  Prediction(dt);

  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    //set measurement dimension, radar can measure r, phi, and r_dot
    n_z_ = 3;
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
    //set measurement dimension, laser can measure px, py
    n_z_ = 2;
  }

  Zsig_ = MatrixXd::Zero(n_z_, 2 * n_aug_ + 1);
  z_pred_ = VectorXd::Zero(n_z_);
  S_ = MatrixXd::Zero(n_z_, n_z_);



  if(meas_package.sensor_type_==MeasurementPackage::RADAR && use_radar_)
  {
    UpdateRadar(meas_package);
  }
  if(meas_package.sensor_type_==MeasurementPackage::LASER && use_laser_)
  {
    UpdateLidar(meas_package);
  }

  StateUpdate(meas_package.raw_measurements_);
}

void UKF::Prediction(double dt) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */
  

  // Generating Augmented Sigma Points
  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_,n_aug_);
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  
  // Should I define the arguments of x_aug, P_aug?
  x_aug.head(n_x_) = x_;
  // x_aug(5)=0;
  // x_aug(6)=0;

  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(n_x_, n_x_) = std_a_ * std_a_;
  P_aug(n_x_ + 1, n_x_ + 1) = std_yawdd_ * std_yawdd_;

  MatrixXd L = P_aug.llt().matrixL();
  Xsig_aug.col(0) = x_aug;

  for(int i = 0; i < n_aug_; i++)
  {
    Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) *L.col(i);
    Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) *L.col(i);
  }

  // Sigma Point Prediction

  for(int i = 0; i < 2 * n_aug_ + 1; i++)
  {
    double p_x = Xsig_aug(0, i);
    double p_y = Xsig_aug(1, i);
    double v = Xsig_aug(2, i);
    double yaw = Xsig_aug(3, i);
    double yawd = Xsig_aug(4, i);
    double nu_a = Xsig_aug(5, i);
    double nu_yawd = Xsig_aug(6, i);

    double px_p, py_p;
    double v_p = v;

    if (fabs(yawd) > 0.001) {
      px_p = p_x + v / yawd * (sin(yaw + yawd * dt) - sin(yaw));
      py_p = p_y + v / yawd * (-cos(yaw + yawd * dt) + cos(yaw));
    }
    else {
      px_p = p_x + v * dt * cos(yaw);
      py_p = p_y + v * dt * sin(yaw);
    }

    double yaw_p = yaw + yawd * dt;
    double yawd_p = yawd;
    
    px_p = px_p + 0.5 * nu_a * dt * dt * cos(yaw);
    py_p = py_p + 0.5 * nu_a * dt * dt * sin(yaw);
    v_p = v_p + nu_a * dt;

    yaw_p = yaw_p + 0.5 * nu_yawd * dt * dt;
    yawd_p = yawd_p + nu_yawd * dt;

    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }

  // Prediction of Mean and Covariance
  // Should I define weights now or during process?

  weights_(0) = lambda_ / (lambda_ + n_aug_);

  x_.fill(0.0);
  P_.fill(0.0);

  // x
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }

  // P
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    while (x_diff(3) > M_PI) {
        x_diff(3) = x_diff(3) - 2. * M_PI;
    }

    while (x_diff(3)<-M_PI) {
        x_diff(3) = x_diff(3) + 2. * M_PI;
    }

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }
}


void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */
  n_z_ = 2;
  MatrixXd Zsig_ = MatrixXd(n_z_, 2 * n_aug_ + 1);
  VectorXd z_pred_= VectorXd::Zero(n_z_);
  MatrixXd S_ = MatrixXd(n_z_, n_z_);

  // Zsig_
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    Zsig_(0,i) = Xsig_pred_(0,i);
    Zsig_(1,i) = Xsig_pred_(1,i);
  }

  // z_pred_
  z_pred_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred_ = z_pred_ + weights_(i) * Zsig_.col(i);
  }
  
  // S_
  S_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig_.col(i) - z_pred_;

    // Angle adjustment
    while (z_diff(1) > M_PI) {
      z_diff(1) = z_diff(1) - 2. * M_PI;
    }

    while (z_diff(1) <- M_PI) {
      z_diff(1) = z_diff(1) + 2. * M_PI;
    }

    S_ = S_ + weights_(i) * z_diff * z_diff.transpose();

  }

  S_ = S_ + R_lidar_;

  // State update

  // MatrixXd Tc = MatrixXd::Zero(n_x_, n_z_);
  // // Tc.fill(0); // necessary ?!
  // for (int i = 0; i < 2 * n_aug_ + 1; i++) {
  //   VectorXd z_diff = Zsig_.col(i) - z_pred_;
  //   VectorXd x_diff = Xsig_pred_.col(i) - x_;

  //   // Angle adjustment

  //   // z_diff
  //   while (z_diff(1) > M_PI) {
  //     z_diff(1) = z_diff(1) - 2. * M_PI;
  //   }
  //   while (z_diff(1) < -M_PI) {
  //     z_diff(1) = z_diff(1) + 2. * M_PI;
  //   }

  //   // x_diff
  //   while (x_diff(3) > M_PI) {
  //     x_diff(3) = x_diff(3) - 2. * M_PI;
  //   }
  //   while (x_diff(3) < -M_PI) {
  //     x_diff(3) = x_diff(3) + 2. * M_PI;
  //   }

  //   Tc = Tc + weights_(i) * x_diff * z_diff.transpose();

  // }
  // VectorXd z_ = meas_package.raw_measurements_;
  // MatrixXd K = Tc * S_.inverse();
  // VectorXd z_diff = z_ - z_pred_;

  // // See about necessity of the next step (Angle Adjustment)
  // while (z_diff(1) > M_PI) {
  //   z_diff(1) = z_diff(1) - 2. * M_PI;
  // }

  // while (z_diff(1) < -M_PI) {
  //   z_diff(1) = z_diff(1) + 2. * M_PI;
  // }

  // P_ = P_ - K*S_*K.transpose();
  // x_ = x_ + K * z_diff;

  NIS_laser_ = (meas_package.raw_measurements_-z_pred_).transpose()*S_.inverse()*(meas_package.raw_measurements_-z_pred_);
  std::cout << "NIS LiDAR = " << NIS_laser_ << std::endl;


}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

  n_z_ = 3;
  MatrixXd Zsig_ = MatrixXd(n_z_, 2 * n_aug_ + 1);
  VectorXd z_pred_ = VectorXd(n_z_);
  MatrixXd S_ = MatrixXd(n_z_,n_z_);
  
  // Zsig_;
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v   = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v_x = cos(yaw) * v;
    double v_y = sin(yaw) * v;

    Zsig_(0,i) = sqrt(p_x * p_x  +  p_y * p_y);
    Zsig_(1,i) = atan2(p_y, p_x);
    // Zsig_(2,i)=(p_x*v_x+p_y*v_y)/Zsig_(0, i);



    if (Zsig_(0, i) < 0.001) {
      Zsig_(2, i) = (p_x * v_x + p_y * v_y) / 0.001;
    } else {
      Zsig_(2, i) = (p_x * v_x + p_y * v_y) / Zsig_(0, i);
    }
  }

  z_pred_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    z_pred_ = z_pred_ + weights_(i) * Zsig_.col(i);
  }

  S_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd z_diff = Zsig_.col(i) - z_pred_;

    // angle adjustment
    while (z_diff(1) > M_PI) {
      z_diff(1) = z_diff(1) - 2. * M_PI;
    }

    while (z_diff(1) <- M_PI) {
      z_diff(1) = z_diff(1) + 2. * M_PI;
    }

    S_ = S_ + weights_(i) * z_diff * z_diff.transpose();
  }

  S_ = S_ + R_radar_;

  // Starting a test (always core dump)
  // MatrixXd Tc = MatrixXd::Zero(n_x_, n_z_);
  // // Tc.fill(0.0);
  

  // for (int i = 0; i < 2 * n_aug_ + 1; i++) {
  //   VectorXd x_diff = Xsig_pred_.col(i) - x_;
  //   VectorXd z_diff = Zsig_.col(i) - z_pred_;


  //   while (z_diff(1) > M_PI) {
  //     z_diff(1) = z_diff(1) - 2. * M_PI;
  //   }
  //   while (z_diff(1) <- M_PI) {
  //     z_diff(1) = z_diff(1) + 2. * M_PI;
  //   }

  //   // x_diff
  //   while (x_diff(3) > M_PI) {
  //     x_diff(3) = x_diff(3) - 2. * M_PI;
  //   }
  //   while (x_diff(3) <- M_PI) {
  //     x_diff(3) = x_diff(3) + 2. * M_PI;
  //   }

  //   Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  // }

  // VectorXd z_ = meas_package.raw_measurements_;
  // MatrixXd K = Tc * S_.inverse();
  // VectorXd z_diff = z_ - z_pred_;
  
  // while (z_diff(1) > M_PI) {
  //   z_diff(1) = z_diff(1) - 2. * M_PI;
  // }
  // while (z_diff(1) <- M_PI) {
  //   z_diff(1) = z_diff(1) + 2. * M_PI;
  // }

  // P_ = P_ - K*S_*K.transpose();
  // x_ = x_ + K * z_diff;

  NIS_radar_ = (meas_package.raw_measurements_-z_pred_).transpose()*S_.inverse()*(meas_package.raw_measurements_-z_pred_);
  std::cout << "NIS Radar = " << NIS_radar_ << std::endl;

}

void UKF::StateUpdate(const VectorXd& z) {

  MatrixXd Tc = MatrixXd::Zero(n_x_, n_z_);

  for (int i = 0; i < 2 * n_aug_ + 1; i++) {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    VectorXd z_diff = Zsig_.col(i) - z_pred_;


    while (z_diff(1) > M_PI) {
      z_diff(1) = z_diff(1) - 2. * M_PI;
    }
    while (z_diff(1) <- M_PI) {
      z_diff(1) = z_diff(1) + 2. * M_PI;
    }

    // x_diff
    while (x_diff(3) > M_PI) {
      x_diff(3) = x_diff(3) - 2. * M_PI;
    }
    while (x_diff(3) <- M_PI) {
      x_diff(3) = x_diff(3) + 2. * M_PI;
    }

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  MatrixXd K = Tc * S_.inverse();
  VectorXd z_diff = z - z_pred_;

  while (z_diff(1) > M_PI) {
    z_diff(1) = z_diff(1) - 2. * M_PI;
  }
  while (z_diff(1) <- M_PI) {
    z_diff(1) = z_diff(1) + 2. * M_PI;   
  }


  P_ = P_ - K*S_*K.transpose();
  x_ = x_ + K * z_diff;

}