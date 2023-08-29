/*---------------------------------------------------------------------------*/
/**
 * @file ups_fw.h
 * @author charles-park (charles.park@hardkernel.com)
 * @brief M1S_UPS (Smart UPS Project)
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2022
**/
/*---------------------------------------------------------------------------*/
#ifndef __UPS_FW_H__
#define __UPS_FW_H__

/*---------------------------------------------------------------------------*/
#include "include/ch5xx.h"

/*---------------------------------------------------------------------------*/
/* UPS System watchdog control */
/*---------------------------------------------------------------------------*/
#define GLOBAL_CFG_UNLOCK()     { SAFE_MOD = 0x55; SAFE_MOD = 0xAA;}
#define WDT_ENABLE()            ( GLOBAL_CFG |=  bWDOG_EN )
#define WDT_DISABLE()           ( GLOBAL_CFG &= ~bWDOG_EN )
#define WDT_CLR()               ( WDOG_COUNT  = 0 )

/* RESET_KEEP Register를 사용하여 Power state관리. */
typedef union {
    struct {
        unsigned char   ePrevBatteryStatus  : 6;
        unsigned char   bPowerOffEvent      : 1;
        unsigned char   bPowerOnEvent       : 1;
    }   bits;
    unsigned char       byte;
}   PowerState;

volatile PowerState     rPOWER_STATE;

/*---------------------------------------------------------------------------*/
/* TC4056A Charger status check port */
/*---------------------------------------------------------------------------*/
/* CH552 GPIO P1.1 */
#define PORT_BAT_ADC    11

/* CH552 GPIO P1.4 */
#define PORT_LED_FULL   14

/* CH552 GPIO P1.5 */
#define PORT_LED_CHRG   15

/*---------------------------------------------------------------------------*/
/* Target watchdog control port */
/*---------------------------------------------------------------------------*/
/* CH552 GPIO P3.2 */
#define PORT_WATCHDOG   32

/*---------------------------------------------------------------------------*/
/* Target Power / Reset control port */
/*---------------------------------------------------------------------------*/
/* CH552 GPIO P3.6 */
#define PORT_CTL_POWER  34
/* CH552 GPIO P3.3 */
#define PORT_CTL_RESET  33

/*---------------------------------------------------------------------------*/
/* Battery level display port */
/*---------------------------------------------------------------------------*/
/* CH552 GPIO P3.0 */
#define PORT_LED_LV4    30
/* CH552 GPIO P3.1 */
#define PORT_LED_LV3    31
/* CH552 GPIO P1.7 */
#define PORT_LED_LV2    17
/* CH552 GPIO P1.6 */
#define PORT_LED_LV1    16

/*---------------------------------------------------------------------------*/
/* M1S_UPS H/W Value set-tup */
/*---------------------------------------------------------------------------*/
/* TLV70245DBVR LDO Output(4.5V) = 4500 mV */
#define MICOM_VDD_mV    4500

/* ADC 8 bits (256). mV resolution value */
//#define MICOM_ADC_RES   256
#define MICOM_ADC_RES   255
/* R9(1.2K) */
#define BAT_ADC_R1      1200
/* R10(20K) */
#define BAT_ADC_R2      20000

/*---------------------------------------------------------------------------*/
/* Battery Level (1 ~ 9) */
/* cal = 3500 + (level * 50) mV*/
/*---------------------------------------------------------------------------*/
#define BAT_LEVEL_3950mV    3950
#define BAT_LEVEL_3900mV    3900
#define BAT_LEVEL_3850mV    3850
#define BAT_LEVEL_3800mV    3800
#define BAT_LEVEL_3750mV    3750
#define BAT_LEVEL_3700mV    3700
#define BAT_LEVEL_3650mV    3650
#define BAT_LEVEL_3600mV    3600
#define BAT_LEVEL_3550mV    3550
/* Battery level이 3400mV보다 작은 경우 강제로 system power off */
#define BAT_LEVEL_OFF       3400

/*---------------------------------------------------------------------------*/
/* Display Battery Level */
/*---------------------------------------------------------------------------*/
#define BAT_DISPLAY_LV4     BAT_LEVEL_3900mV
#define BAT_DISPLAY_LV3     BAT_LEVEL_3750mV
#define BAT_DISPLAY_LV2     BAT_LEVEL_3650mV
#define BAT_DISPLAY_LV1     BAT_LEVEL_3550mV

/*---------------------------------------------------------------------------*/
/* booting을 위한 최소 battery level 설정 */
/* Set battery level for system power on */
/* Power on when battery charge condition detected.(default) */
/* 0     : Detect charging status.(default) */
/* 1 ~ 9 : BATTERY LEVEL */
/*---------------------------------------------------------------------------*/
__xdata unsigned long BatteryBootVolt = 0;

/*---------------------------------------------------------------------------*/
/* ADC Raw 샘플 갯수. 샘플 갯수중 최대값, 최소값을 제외한 평균값은 ADC값으로 사용한다. */
/*---------------------------------------------------------------------------*/
#define ADC_SAMPLE_CNT      10

/*---------------------------------------------------------------------------*/
/* 1분 평균값 산출 (1초당 2번 * 60개 샘플) */
/*---------------------------------------------------------------------------*/
//#define BAT_AVR_SAMPLE_CNT  120
#define BAT_AVR_SAMPLE_CNT  20

__xdata float BatteryVoltSamples[BAT_AVR_SAMPLE_CNT];

/*---------------------------------------------------------------------------*/
/*
    Charger Status
                 | LED_CHARG_STATUS | LED_FULL_STATUS |
    Discharging  |         1        |        1        |
    Charging     |         0        |        1        |
    Full Charged |         1        |        0        |
    Error        |         0        |        0        | Battery Removed.
*/
/*---------------------------------------------------------------------------*/
__xdata bool LED_CHRG_STATUS = false;
__xdata bool LED_FULL_STATUS = false;

/*---------------------------------------------------------------------------*/
/* Battery level indicator LED status */
/*---------------------------------------------------------------------------*/
__xdata bool LED_LV1_STATUS = false;
__xdata bool LED_LV2_STATUS = false;
__xdata bool LED_LV3_STATUS = false;
__xdata bool LED_LV4_STATUS = false;

/*---------------------------------------------------------------------------*/
/* Serial Protocol */
/*---------------------------------------------------------------------------*/
#define PROTOCOL_SIZE   4

__xdata char Protocol[PROTOCOL_SIZE];

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Battery Status */
/*---------------------------------------------------------------------------*/
enum {
    eBATTERY_DISCHARGING = 1,
    eBATTERY_CHARGING,
    eBATTERY_FULL,
    eBATTERY_REMOVED,
    eBATTERY_END,
}   eBATTERY_STATUS;

__xdata enum eBATTERY_STATUS BatteryStatus = eBATTERY_REMOVED;

/*---------------------------------------------------------------------------*/
/* Main Loop Control */
/*---------------------------------------------------------------------------*/
#define TARGET_RESET_DELAY      100

/* Battery Read 및 Display 주기 */
#define PERIOD_LOOP_MILLIS      500

/* Power off상태에서 LED display 주기 */
#define PEROID_OFF_MILLIS       2000

/* Power off시 배터리 잔량 LED표시 시간 */
#define POWEROFF_LED_DELAY      100

__xdata unsigned long MillisLoop        = 0;
__xdata unsigned long BatteryVolt       = 0;
__xdata unsigned long BatteryAvrVolt    = 0;
__xdata unsigned long BatteryAdcVolt    = 0;

/*---------------------------------------------------------------------------*/
/* Auto repeat data control */
/*---------------------------------------------------------------------------*/
__xdata unsigned long PeriodRequestBatteryLevel   = 0;
__xdata unsigned long MillisRequestBatteryLevel   = 0;

__xdata unsigned long PeriodRequestBatteryVoltage = 0;
__xdata unsigned long MillisRequestBatteryVoltage = 0;

__xdata unsigned long PeriodRequestChagerStatus   = 0;
__xdata unsigned long MillisRequestChagerStatus   = 0;

/*---------------------------------------------------------------------------*/
/* Target reset(watchdog) control */
/*---------------------------------------------------------------------------*/
__xdata unsigned long PeriodRequestWatchdogTime = 0;
__xdata unsigned long MillisRequestWatchdogTime = 0;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Function prototype define */
/*---------------------------------------------------------------------------*/
void    LED_SetState            (unsigned char state);
void    LED_Test                (void);

unsigned long cal_battery_level (unsigned int val);

float   battery_adc_volt_cal    (void);
float   battery_adc_volt        (void);
float   battery_volt            (void);
void    battery_avr_volt_init   (void);
float   battery_avr_volt        (enum eBATTERY_STATUS battery_status);

enum eBATTERY_STATUS battery_status (void);
void    battery_level_display   (enum eBATTERY_STATUS bat_status, unsigned long bat_volt);
void    repeat_data_clear       (void);
void    repeat_data_check       (void);
void    request_data_send       (char cmd);
void    protocol_check          (void);

void    target_system_reset     (void);
void    target_system_power     (bool onoff);
void    port_init               (void);
void    setup                   ();
void    loop                    ();

/*---------------------------------------------------------------------------*/
#endif  // __UPS_FW_H__

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

