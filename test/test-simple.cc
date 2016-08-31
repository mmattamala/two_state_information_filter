#include "gtest/gtest.h"

#include "../include/generalized_information_filter/element-vector.h"
#include "generalized_information_filter/binary-residual.h"
#include "generalized_information_filter/common.h"
#include "generalized_information_filter/filter.h"
#include "generalized_information_filter/prediction.h"
#include "generalized_information_filter/residuals/imu-prediction.h"
#include "generalized_information_filter/residuals/pose-update.h"
#include "generalized_information_filter/transformation.h"
#include "generalized_information_filter/unary-update.h"

using namespace GIF;

class TransformationExample : public Transformation<ElementPack<Vec3>,
    ElementPack<double, std::array<Vec3, 4>>> {
 public:
  TransformationExample()
      : mtTransformation({"pos"}, {"tim", "sta"}) {
  }

  virtual ~TransformationExample() {}

  void evalTransform(Vec3& posOut, const double& timeIn,
                     const std::array<Vec3, 4>& posIn) const {
    posOut = (timeIn + 1.0) * (posIn[2] + Vec3(1, 2, 3));
  }
  void jacTransform(MatX& J, const double& timeIn,
                    const std::array<Vec3, 4>& posIn) const {
    J.setZero();
    setJacBlock<0, 0>(J, Vec3(1, 2, 3));
    Eigen::Matrix<double, 3, 12> J2;
    J2.setZero();
    J2.block<3, 3>(0, 6) = Mat3::Identity();
    setJacBlock<0, 1>(J, J2);
  }
};

class EmptyMeas : public ElementVector {
 public:
  EmptyMeas(): ElementVector(std::shared_ptr<ElementVectorDefinition>(new ElementVectorDefinition())){};
};

class BinaryRedidualVelocity : public BinaryResidual<ElementPack<Vec3>,
    ElementPack<Vec3, Vec3>, ElementPack<Vec3>, ElementPack<Vec3>, EmptyMeas> {
 public:
  BinaryRedidualVelocity()
      : mtBinaryRedidual( { "pos" }, { "pos", "vel" }, { "pos" }, { "pos" },
                         false, false, false) {
    dt_ = 0.1;
  }

  virtual ~BinaryRedidualVelocity() {}

  void eval(Vec3& posRes, const Vec3& posPre, const Vec3& velPre,
                        const Vec3& posCur, const Vec3& posNoi) const {
    posRes = posPre + dt_ * velPre - posCur + posNoi;
  }

  void jacPre(MatX& J, const Vec3& posPre, const Vec3& velPre,
                  const Vec3& posCur, const Vec3& posNoi) const {
    J.setZero();
    setJacBlockPre<0, 0>(J, Mat3::Identity());
    setJacBlockPre<0, 1>(J, dt_ * Mat3::Identity());
  }

  void jacCur(MatX& J, const Vec3& posPre, const Vec3& velPre,
                  const Vec3& posCur, const Vec3& posNoi) const {
    J.setZero();
    setJacBlockCur<0, 0>(J, -Mat3::Identity());
  }

  void jacNoi(MatX& J, const Vec3& posPre, const Vec3& velPre,
                  const Vec3& posCur, const Vec3& posNoi) const {
    J.setZero();
    setJacBlockNoi<0, 0>(J, Mat3::Identity());
  }

 protected:
  double dt_;
};

class AccelerometerMeas : public ElementVector {
 public:
  AccelerometerMeas(const Vec3& acc = Vec3(0, 0, 0))
      : ElementVector(std::shared_ptr<ElementVectorDefinition>(new ElementPack<Vec3>( { "acc" }))),
        acc_(ElementVector::GetValue<Vec3>("acc")) {
    acc_ = acc;
  }
  Vec3& acc_;
};

class BinaryRedidualAccelerometer : public BinaryResidual<ElementPack<Vec3>,
    ElementPack<Vec3>, ElementPack<Vec3>, ElementPack<Vec3>, AccelerometerMeas> {
 public:
  BinaryRedidualAccelerometer()
      : mtBinaryRedidual( { "vel" }, { "vel" }, { "vel" }, { "vel" }, false,
                         true, true) {
    dt_ = 0.1;
  }

  virtual ~BinaryRedidualAccelerometer() {
  }

  void eval(Vec3& velRes, const Vec3& velPre, const Vec3& velCur,
                        const Vec3& velNoi) const {
    velRes = velPre + dt_ * meas_->acc_ - velCur + velNoi;
  }
  void jacPre(MatX& J, const Vec3& velPre, const Vec3& velCur,
                  const Vec3& velNoi) const {
    J.setZero();
    setJacBlockPre<0, 0>(J, Mat3::Identity());
  }
  void jacCur(MatX& J, const Vec3& velPre, const Vec3& velCur,
                  const Vec3& velNoi) const {
    J.setZero();
    setJacBlockCur<0, 0>(J, -Mat3::Identity());
  }
  void jacNoi(MatX& J, const Vec3& velPre, const Vec3& velCur,
                  const Vec3& velNoi) const {
    J.setZero();
    setJacBlockNoi<0, 0>(J, Mat3::Identity());
  }

 protected:
  double dt_;
};

class PredictionAccelerometer : public Prediction<ElementPack<Vec3>,
    ElementPack<Vec3>, AccelerometerMeas> {
 public:
  PredictionAccelerometer()
      : mtPrediction( { "vel" }, { "vel" }) {
    dt_ = 0.1;
  }

  virtual ~PredictionAccelerometer() {}

  void predict(Vec3& velCur, const Vec3& velPre,
                          const Vec3& velNoi) const {
    velCur = velPre + dt_ * meas_->acc_ + velNoi;
  }
  void predictJacPre(MatX& J, const Vec3& velPre,
                            const Vec3& velNoi) const {
    J.setZero();
    setJacBlockPre<0, 0>(J, Mat3::Identity());
  }
  void predictJacNoi(MatX& J, const Vec3& velPre,
                            const Vec3& velNoi) const {
    J.setZero();
    setJacBlockNoi<0, 0>(J, Mat3::Identity());
  }

 protected:
  double dt_;
};

// The fixture for testing class ScalarState
class NewStateTest : public virtual ::testing::Test {
 protected:
  NewStateTest()
      : covMat_(1, 1) {}
  virtual ~NewStateTest() {}
  MatX covMat_;
};

// Test constructors
TEST_F(NewStateTest, constructor) {
  TransformationExample t;
  std::shared_ptr<ElementVector> s1a(new ElementVector(t.inputDefinition()));
  std::shared_ptr<ElementVector> s1b(new ElementVector(t.inputDefinition()));
  s1a->SetIdentity();
  s1a->Print();

  // Boxplus and BoxMinus
  Eigen::VectorXd v(s1a->GetDimension());
  v.setZero();
  for (int i = 0; i < s1a->GetDimension(); i++) {
    v(i) = i;
  }
  s1a->BoxPlus(v, s1b);
  s1b->Print();
  s1a->BoxMinus(s1b, v);
  std::cout << v.transpose() << std::endl;

  // Jacobian
  MatX J;
  t.jacFD(J, s1a);
  std::cout << J << std::endl;

  // Transformation
  std::shared_ptr<ElementVector> s2(new ElementVector(t.outputDefinition()));
  MatX P1(s1a->GetDimension(), s1a->GetDimension());
  MatX P2(s2->GetDimension(), s2->GetDimension());
  t.transformState(s2, s1a);
  t.transformCovMat(P2, s1a, P1);
  t.testJac(s1a);

  // Velocity Residual
  std::shared_ptr<BinaryRedidualVelocity> velRes(new BinaryRedidualVelocity());
  std::shared_ptr<ElementVector> pre(new ElementVector(velRes->preDefinition()));
  pre->SetIdentity();
  std::shared_ptr<ElementVector> cur(new ElementVector(velRes->curDefinition()));
  cur->SetIdentity();
  std::shared_ptr<ElementVector> noi(new ElementVector(velRes->noiDefinition()));
  noi->SetIdentity();
  velRes->testJacs(pre, cur, noi);

  // Accelerometer Residual
  std::shared_ptr<BinaryRedidualAccelerometer> accRes(
      new BinaryRedidualAccelerometer());

  // Filter
  Filter f;
  f.addRes(velRes);
  f.addRes(accRes);
  std::shared_ptr<ElementVector> preState(new ElementVector(f.stateDefinition()));
  preState->SetIdentity();
  preState->GetValue < Vec3 > ("pos") = Vec3(1, 2, 3);
  preState->Print();
  std::shared_ptr<ElementVector> curState(new ElementVector(f.stateDefinition()));
  curState->SetIdentity();
  curState->Print();
  f.evalRes(preState, curState);


  // Test measurements
  std::shared_ptr<EmptyMeas> eptMeas(new EmptyMeas);
  TimePoint start = Clock::now();
  f.init(start+fromSec(0.00));
  f.addMeas(0, eptMeas,start+fromSec(-0.1));
  f.addMeas(0, eptMeas,start+fromSec(0.0));
  f.addMeas(0, eptMeas,start+fromSec(0.2));
  f.addMeas(0, eptMeas,start+fromSec(0.3));
  f.addMeas(0, eptMeas,start+fromSec(0.4));
  f.addMeas(1, std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(-0.1,0.0,0.0))),start+fromSec(-0.1));
  f.addMeas(1, std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.0,0.0,0.0))),start+fromSec(0.0));
  f.addMeas(1, std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.1,0.0,0.0))),start+fromSec(0.1));
  f.addMeas(1, std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.4,0.0,0.0))),start+fromSec(0.3));
  f.addMeas(1, std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.3,0.0,0.0))),start+fromSec(0.5));

  f.update();

  f.update();

  // Prediction Accelerometer
  std::shared_ptr<PredictionAccelerometer> accPre(
      new PredictionAccelerometer());
  std::shared_ptr<ElementVector> preAcc(new ElementVector(accPre->preDefinition()));
  std::shared_ptr<ElementVector> curAcc(new ElementVector(accPre->preDefinition()));
  std::shared_ptr<ElementVector> noiAcc(new ElementVector(accPre->noiDefinition()));
  preAcc->SetIdentity();
  curAcc->SetIdentity();
  noiAcc->SetIdentity();
  accPre->testJacs(preAcc, curAcc, noiAcc);

  // Test measurements
  Filter f2;
  f2.addRes(velRes);
  f2.addRes(accPre);
  f2.init(start+fromSec(0.00));
  f2.addMeas(0,eptMeas,start+fromSec(-0.1));
  f2.addMeas(0,eptMeas,start+fromSec(0.0));
  f2.addMeas(0,eptMeas,start+fromSec(0.2));
  f2.addMeas(0,eptMeas,start+fromSec(0.3));
  f2.addMeas(0,eptMeas,start+fromSec(0.4));
  f2.addMeas(1,std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(-0.1,0.0,0.0))),start+fromSec(-0.1));
  f2.addMeas(1,std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.0,0.0,0.0))),start+fromSec(0.0));
  f2.addMeas(1,std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.1,0.0,0.0))),start+fromSec(0.1));
  f2.addMeas(1,std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.4,0.0,0.0))),start+fromSec(0.3));
  f2.addMeas(1,std::shared_ptr<AccelerometerMeas>(
      new AccelerometerMeas(Vec3(0.3,0.0,0.0))),start+fromSec(0.5));
  f2.update();
  f2.update();

  // Test IMU + Pose filter
  int s = 0;
  std::shared_ptr<IMUPrediction> imuPre(new IMUPrediction());
  imuPre->getR() = 1e-8 * imuPre->getR();
  imuPre->testJacs(s);
  std::shared_ptr<PoseUpdate> poseUpd(new PoseUpdate());
  poseUpd->getR() = 1e-8 * poseUpd->getR();
  poseUpd->testJacs(s);

  Filter imuPoseFilter;
  int imuPreInd = imuPoseFilter.addRes(imuPre);
  int poseUpdInd = imuPoseFilter.addRes(poseUpd);
  imuPoseFilter.init(start);
  imuPoseFilter.addMeas(imuPreInd, std::shared_ptr<IMUMeas>(
      new IMUMeas(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, 9.81))), start);
  for (int i = 1; i <= 10; i++) {
    imuPoseFilter.addMeas(imuPreInd, std::shared_ptr<IMUMeas>(
        new IMUMeas(Vec3(0.3, 0.0, 0.1), Vec3(0.0, 0.2, 9.81))),
        start + fromSec(i));
    imuPoseFilter.addMeas(poseUpdInd, std::shared_ptr<PoseMeas>(
        new PoseMeas(Vec3(0.0, 0.0, 0.0), Quat(1.0, 0.0, 0.0, 0.0))),
        start + fromSec(i));
  }
  TimePoint startFilter = Clock::now();
  imuPoseFilter.update();
  std::cout << toSec(Clock::now()-startFilter)*1000 << " ms" << std::endl;

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}