/*
 * Measurement.hpp
 *
 *  Created on: Aug 6, 2016
 *      Author: Bloeschm
 */

#ifndef GIF_MEASUREMENT_HPP_
#define GIF_MEASUREMENT_HPP_
#include <map>
#include <set>

#include "generalized_information_filter/common.hpp"
#include "generalized_information_filter/State.hpp"

namespace GIF{

class BinaryResidualBase;

class MeasurementBase: public State{
 public:
  MeasurementBase(const std::shared_ptr<const StateDefinition>& def): State(def){
  }
  virtual ~MeasurementBase(){};
};

class MeasurementTimeline{
 public:
  MeasurementTimeline(const Duration& maxWaitTime = fromSec(0.1), const Duration& minWaitTime = Duration::zero());
  virtual ~MeasurementTimeline();
  void addMeas(const std::shared_ptr<const MeasurementBase>& meas, const TimePoint& t);
  void removeProcessedFirst();
  void removeProcessedMeas(const TimePoint& t);
  void clear();
  bool getLastTime(TimePoint& lastTime) const;
  TimePoint getMaximalUpdateTime(const TimePoint& currentTime) const;
  void addAllInRange(std::set<TimePoint>& times, const TimePoint& start, const TimePoint& end) const;
  void addLastInRange(std::set<TimePoint>& times, const TimePoint& start, const TimePoint& end) const;
  void splitMeasurements(const TimePoint& t0, const TimePoint& t1, const TimePoint& t2, std::shared_ptr<const BinaryResidualBase>& res);
  void mergeMeasurements(const TimePoint& t0, const TimePoint& t1, const TimePoint& t2, std::shared_ptr<const BinaryResidualBase>& res);
 protected:
  std::map<TimePoint,std::shared_ptr<const MeasurementBase>> measMap_;
  Duration maxWaitTime_;
  Duration minWaitTime_;
  TimePoint lastProcessedTime_;
  bool hasProcessedTime_;
};

}

#endif /* GIF_MEASUREMENT_HPP_ */