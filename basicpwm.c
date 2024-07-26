#include "hardware/pwm.h"

gpio_set_function(15, GPIO_FUNC_PWM);  
slice=pwm_gpio_to_slice_num (15);  
channel=pwm_gpio_to_channel (15);  

