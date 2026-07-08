#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

extern FATFS SDFatFS;
extern char SDPath[4];

int tf_card_bringup_run(void) {
  Stm32TfCardContext ctx;
  TfCardPort tf;
  TfCardSmokeResult result;

  stm32h750_tf_card_bind(&tf, &ctx, &SDFatFS, SDPath);
  if (tf_card_run_smoke_test(&tf, &result) != TF_CARD_OK) {
    if (!result.present) {
      return 1;
    }
    if (!result.mounted) {
      return 2;
    }
    if (!result.dirs_created) {
      return 3;
    }
    if (!result.file_written) {
      return 4;
    }
    return 5;
  }
  return 0;
}

#endif
