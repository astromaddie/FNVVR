#include <algorithm>

#include "vr_game_timer_step.h"

namespace dxvk {

  double vrGameTimerStepMs(double realMs) {
    return std::clamp(realMs, VrGameTimerMinStepMs, VrGameTimerMaxStepMs);
  }


  float vrHavokMaxStep(double stepMs, float settingSeconds) {
    float stepSeconds = float(stepMs / 1000.0);
    return stepSeconds < settingSeconds ? stepSeconds : settingSeconds;
  }

}
