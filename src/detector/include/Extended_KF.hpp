#include <Eigen/Dense>
#include <functional>

class Extended_KF{
public:
    Extended_KF() = default;

    using VecVecFunc = std::function<Eigen::VectorXd(const Eigen::VectorXd &)>;
    using VecMatFunc = std::function<Eigen::MatrixXd(const Eigen::VectorXd &)>;
    using VoidMatFunc = std::function<Eigen::MatrixXd()>;

    explicit Extended_KF(const VecVecFunc & f, const VecVecFunc & h, const VecMatFunc & j_f, const VecMatFunc & j_h,
        const VoidMatFunc & u_q, const VecMatFunc & u_r, const Eigen::MatrixXd & P0);
    
    // Set the initial state
    void setState(const Eigen::VectorXd & x0);

    // Compute a predicted state
    Eigen::MatrixXd predict();

    // Update the estimated state based on measurement
    Eigen::MatrixXd update(const Eigen::VectorXd & z);

private:
    // Extrapolate nonlinear vector function
    VecVecFunc f;
    // Observation nonlinear vector function
    VecVecFunc h;
    // Jacobian of f
    VecMatFunc jacobian_f;
    Eigen::MatrixXd F;
    // Jacobian of h
    VecMatFunc jacobian_h;
    Eigen::MatrixXd H;
    // Process noise covariance matrix
    VoidMatFunc update_Q;
    Eigen::MatrixXd Q;
    // Measurement noise covariance matrix
    VecMatFunc update_R;
    Eigen::MatrixXd R;

    // Priori error estimate covariance matrix
    Eigen::MatrixXd P_pri;
    // Posteriori error estimate covariance matrix
    Eigen::MatrixXd P_post;

    // Kalman gain
    Eigen::MatrixXd K;

    // System dimensions
    int n;

    // N-size identity
    Eigen::MatrixXd I;

    // Priori state
    Eigen::VectorXd x_pri;
    // Posteriori state
    Eigen::VectorXd x_post;
};