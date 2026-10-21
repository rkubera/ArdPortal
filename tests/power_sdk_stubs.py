"""SDK collaborators for the actual power platform tests."""
arduino=r'''
#pragma once
#include <cstdint>
#include <string>
using String=std::string;
inline uint32_t millis(){return 0;}
inline unsigned idleMs=0,deepCalls=0;
inline void delay(uint32_t ms){idleMs=ms;}
inline unsigned getCpuFrequencyMhz(){return 160;}
struct rst_info {unsigned reason=0;};
inline rst_info resetInfo;
inline bool nullResetInfo=false;
enum {REASON_DEFAULT_RST,REASON_WDT_RST,REASON_EXCEPTION_RST,REASON_SOFT_WDT_RST,
      REASON_SOFT_RESTART,REASON_DEEP_SLEEP_AWAKE,REASON_EXT_SYS_RST};
struct Esp {static rst_info* getResetInfoPtr(){return nullResetInfo?nullptr:&resetInfo;}static uint64_t deepSleepMax(){return 100000000;}void deepSleep(uint64_t){++deepCalls;}};
inline Esp ESP;
'''
wifi=r'''
#pragma once
#define WIFI_OFF 0
#define WIFI_PS_NONE 0
#define WIFI_PS_MIN_MODEM 1
#define WIFI_NONE_SLEEP 0
#define WIFI_MODEM_SLEEP 1
#define WIFI_LIGHT_SLEEP 2
using wifi_ps_type_t=int;using WiFiSleepType_t=int;
struct Wifi {
 int sleep=0,radio=1,interval=3;bool fail=false;
 int getMode(){return radio;}bool mode(int value){if(fail)return false;radio=value;return true;}
 int getSleep(){return sleep;}int getSleepMode(){return sleep;}
 bool setSleep(int value){if(fail)return false;sleep=value;return true;}
 int getListenInterval(){return interval;}
 bool setSleepMode(int value,int listen=0){if(fail)return false;interval=listen;return setSleep(value);}
};
inline Wifi WiFi;
'''
sleep=r'''
#pragma once
#define ESP_OK 0
#define ESP_SLEEP_WAKEUP_TIMER 1
#define ESP_SLEEP_WAKEUP_UNDEFINED 0
inline int sdkWakeCause=0;
inline int esp_sleep_get_wakeup_cause(){return sdkWakeCause;}
inline int esp_sleep_disable_wakeup_source(int){return ESP_OK;}
inline bool timerFail=false;
inline int esp_sleep_enable_timer_wakeup(uint64_t){return timerFail?-1:ESP_OK;}
inline void esp_deep_sleep_start(){++deepCalls;}
'''
pm=r'''
#pragma once
struct esp_pm_config_t {int max_freq_mhz=80,min_freq_mhz=40;bool light_sleep_enable=false;};
inline esp_pm_config_t pmConfig;
inline bool pmFail=false;
inline int esp_pm_get_configuration(esp_pm_config_t* out){*out=pmConfig;return ESP_OK;}
inline int esp_pm_configure(const esp_pm_config_t* value){if(pmFail)return -1;pmConfig=*value;return ESP_OK;}
'''

arduino += r'''
#define IRAM_ATTR
#define ARDUINO_ISR_FLAG 0
#define GPIO_PORT_0 0
#define HIGH 1
#define LOW 0
#define RISING 1
#define FALLING 2
#define CHANGE 3
#define ONLOW 4
#define ONHIGH 5
using gpio_num_t=int;using gpio_int_type_t=int;using esp_err_t=int;
inline int pinLevels[64]={};
inline void (*pinHandlers[64])(void*)={};
inline void* pinArguments[64]={};
inline bool pinEnabled[64]={},pinWake[64]={};
inline uint32_t pinRegisters[64]={};
inline bool gpioFail=false;
inline int digitalRead(uint8_t pin){return pinLevels[pin];}
inline void attachInterruptArg(uint8_t pin,void(*handler)(void*),void* arg,int mode){pinHandlers[pin]=handler;pinArguments[pin]=arg;pinEnabled[pin]=true;pinRegisters[pin]=uint32_t(mode)<<7;}
inline void detachInterrupt(uint8_t pin){pinHandlers[pin]=nullptr;pinEnabled[pin]=false;pinRegisters[pin]=0;}
inline uint32_t xt_rsil(int){return 0;}
inline void xt_wsr_ps(uint32_t){}
#define GPC(pin) pinRegisters[pin]
#define GPCI 7
inline void triggerPin(uint8_t pin){if(pinHandlers[pin]&&pinEnabled[pin]&&(pinRegisters[pin]>>7))pinHandlers[pin](pinArguments[pin]);}
'''
sleep += r'''
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_SLEEP_REJECT 3
inline unsigned lightCalls=0;
inline int lightResult=ESP_OK;
inline int esp_light_sleep_start(){++lightCalls;return lightResult;}
#define ESP_SLEEP_WAKEUP_GPIO 2
#define ESP_SLEEP_WAKEUP_EXT0 3
#define ESP_SLEEP_WAKEUP_EXT1 4
#define ESP_EXT1_WAKEUP_ANY_HIGH 1
#define ESP_EXT1_WAKEUP_ALL_LOW 0
#define ESP_EXT1_WAKEUP_ANY_LOW 0
#define ESP_GPIO_WAKEUP_GPIO_HIGH 1
#define ESP_GPIO_WAKEUP_GPIO_LOW 0
inline bool gpioWakeArmed=false;
inline bool esp_sleep_is_valid_wakeup_gpio(gpio_num_t pin){return pin>=0&&pin<=5;}
inline int esp_sleep_enable_gpio_wakeup(){gpioWakeArmed=!gpioFail;return gpioFail?-1:ESP_OK;}
inline int esp_sleep_enable_ext0_wakeup(gpio_num_t,int){return gpioFail?-1:ESP_OK;}
inline int esp_sleep_enable_ext1_wakeup(uint64_t,int){return gpioFail?-1:ESP_OK;}
inline int esp_deep_sleep_enable_gpio_wakeup(uint64_t,int){return gpioFail?-1:ESP_OK;}
'''
gpio=r'''
#pragma once
#include <Arduino.h>
#define GPIO_IS_VALID_GPIO(pin) ((pin)<40)
inline int gpio_install_isr_service(int){return gpioFail?-1:ESP_OK;}
inline int gpio_set_intr_type(gpio_num_t pin,gpio_int_type_t mode){pinRegisters[pin]=uint32_t(mode)<<7;return gpioFail?-1:ESP_OK;}
inline int gpio_isr_handler_add(gpio_num_t pin,void(*handler)(void*),void* arg){if(gpioFail)return -1;pinHandlers[pin]=handler;pinArguments[pin]=arg;pinEnabled[pin]=true;return ESP_OK;}
inline int gpio_isr_handler_remove(gpio_num_t pin){detachInterrupt(pin);return ESP_OK;}
inline int gpio_wakeup_enable(gpio_num_t pin,gpio_int_type_t){pinWake[pin]=true;return gpioFail?-1:ESP_OK;}
inline int gpio_wakeup_disable(gpio_num_t pin){pinWake[pin]=false;return ESP_OK;}
inline int gpio_intr_enable(gpio_num_t pin){pinEnabled[pin]=true;return ESP_OK;}
'''
freertos=r'''
#pragma once
#include <Arduino.h>
#define configSUPPORT_STATIC_ALLOCATION 1
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define portENTER_CRITICAL_ISR(mux) ((void)(mux))
#define portEXIT_CRITICAL_ISR(mux) ((void)(mux))
#define pdFALSE 0
#define portYIELD_FROM_ISR() ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
#define taskYIELD() ((void)0)
using BaseType_t=int;using TickType_t=uint32_t;
struct StaticSemaphore_t{bool token=false;};
using SemaphoreHandle_t=StaticSemaphore_t*;
inline void (*idleHook)()=nullptr;
inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t* storage){return storage;}
inline int xSemaphoreTake(SemaphoreHandle_t sem,TickType_t ticks){
 if(sem->token){sem->token=false;return 1;}
 idleMs=ticks;if(idleHook)idleHook();sem->token=false;return 0;
}
inline void vSemaphoreDelete(SemaphoreHandle_t){}
inline int xSemaphoreGiveFromISR(SemaphoreHandle_t sem,BaseType_t*){sem->token=true;return 1;}
'''
coredecls=r'''
#pragma once
#include <Arduino.h>
inline void esp_schedule(){}
template<class Predicate>void esp_delay(uint32_t ms,Predicate blocked){if(blocked())delay(ms);}
'''
hal=r'''
#pragma once
#include <Arduino.h>
#define GPIO_LL_GET_HW(port) nullptr
inline void gpio_ll_intr_disable(void*,uint32_t pin){pinEnabled[pin]=false;}
'''

def write_extra_stubs(directory):
    files={'driver/gpio.h':gpio,'hal/gpio_ll.h':hal,'freertos/FreeRTOS.h':freertos,
           'freertos/semphr.h':'#pragma once\n#include "FreeRTOS.h"\n','coredecls.h':coredecls,
           'user_interface.h':'#pragma once\n#include <Arduino.h>\n',
           'esp_system.h':r'''
#pragma once
enum {ESP_RST_UNKNOWN,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,
      ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,ESP_RST_BROWNOUT};
inline int sdkResetReason=ESP_RST_POWERON;
inline int esp_reset_reason(){return sdkResetReason;}
'''}
    for name,data in files.items():
        path=directory/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(data)
