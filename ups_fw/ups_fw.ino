/*---------------------------------------------------------------------------*/
/**
 * @file ups_fw.ino
 * @author charles-park (charles.park@hardkernel.com)
 * @brief M1S_UPS (Smart UPS Project)
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2022
**/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#include "include/ch5xx.h"

/*---------------------------------------------------------------------------*/
#define FW_VERSION  "V0-1"

/* for 1st DEV board */
//#define PCB_REV_20230712

/*---------------------------------------------------------------------------*/
/* UPS System watchdog control */
/*---------------------------------------------------------------------------*/
#define GLOBAL_CFG_UNLOCK()     { SAFE_MOD = 0x55; SAFE_MOD = 0xAA;}
#define WDT_ENABLE()            ( GLOBAL_CFG |=  bWDOG_EN )
#define WDT_DISABLE()           ( GLOBAL_CFG &= ~bWDOG_EN )
#define WDT_CLR()               ( WDOG_COUNT  = 0 )

/* RESET_KEEP Register를 사용하여 Power state관리. */
#define RESET_KEEP_POWERON  0x80
#define RESET_KEEP_POWEROFF 0x40

/*---------------------------------------------------------------------------*/
/* TC4056A Charger status check pin */
/*---------------------------------------------------------------------------*/
/* CH552 GPIO P1.1 */
#define PORT_BAT_ADC    11

/* CH552 GPIO P1.4 */
#define PORT_LED_FULL   14

#if defined(PCB_REV_20230712)
    /* CH552 GPIO P3.2 */
    #define PORT_LED_CHRG   32
#else
    /* CH552 GPIO P1.5 */
    #define PORT_LED_CHRG   15
#endif

/*---------------------------------------------------------------------------*/
/* Target watchdog control pin */
/*---------------------------------------------------------------------------*/
#if defined(PCB_REV_20230712)
    /* CH552 GPIO P1.5 */
    #define PORT_WATCHDOG   15
#else
    /* CH552 GPIO P3.2 */
    #define PORT_WATCHDOG   32
#endif

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Target Power / Reset control pin */
/*---------------------------------------------------------------------------*/
/* CH552 GPIO P3.6 */
#define PORT_CTL_POWER  34
/* CH552 GPIO P3.3 */
#define PORT_CTL_RESET  33

/* Battery level display pin */
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
#if defined(PCB_REV_20230712)
    /* SMAZ5V0 Zener Diode (5000mV) */
    #define MICOM_VDD_mV    5065
#else
    /* TLV70245DBVR LDO Output(4.5V) = 4500 mV */
    #define MICOM_VDD_mV    4500
#endif

/* ADC 8 bits (256). mV resolution value */
#define MICOM_ADC_RES   255
/* R9(1.2K) */
#define BAT_ADC_R1      1200
/* R10(20K) */
#define BAT_ADC_R2      20000

/*---------------------------------------------------------------------------*/
/* Display Battery Level */
/*---------------------------------------------------------------------------*/
#define BAT_LEVEL_LV4   3900
#define BAT_LEVEL_LV3   3750
#define BAT_LEVEL_LV2   3650
#define BAT_LEVEL_LV1   3550

/* Battery level이 3400mV보다 작은 경우 강제로 system power off */
#define BAT_LEVEL_OFF   3400

/* booting을 위한 최소 battery level 설정 */
#define BAT_LEVEL_BOOT  BAT_LEVEL_LV2

/*---------------------------------------------------------------------------*/
/* ADC Raw 샘플 갯수. 샘플 갯수중 최대값, 최소값을 제외한 평균값은 ADC값으로 사용한다. */
/*---------------------------------------------------------------------------*/
#define ADC_SAMPLING_CNT    10

/*---------------------------------------------------------------------------*/
/* 1분 평균값 산출 (1초당 2번 * 60개 샘플) */
/*---------------------------------------------------------------------------*/
#define BATTERY_AVR_SAMPLE_COUNT    120

__xdata float BatteryVoltSamples[BATTERY_AVR_SAMPLE_COUNT];

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
} eBATTERY_STATUS;

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
float   battery_adc_volt_cal    (void);
float   battery_adc_volt        (void);
float   battery_volt            (void);
void    battery_avr_volt_init   (void);
float   battery_avr_volt        (enum eBATTERY_STATUS battery_status);

enum eBATTERY_STATUS battery_status (void);
void    battery_level_display   (enum eBATTERY_STATUS bat_status, unsigned long bat_volt);
void    request_data_send       (char cmd);
void    protocol_check          (void);
void    repeat_data_clear       (void);
void    repeat_data_check       (void);

void    target_system_reset     (void);
void    target_system_power     (bool onoff);
void    port_init               (void);
void    setup                   ();
void    loop                    ();

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Debug Enable Flag */
/*---------------------------------------------------------------------------*/
//#define __DEBUG_LOG__
#if defined (__DEBUG_LOG__)
    /* for Battery discharge graph (log) */
    #define PERIOD_LOG_MILLIS   1000
    __xdata unsigned long MillisLog = 0;
    __xdata unsigned long LogCount = 0;
    /*
        #if defined (_DEBUG_)
            USBSerial_print()
            USBSerial_println()
        #endif
    */
#else
    /* Normal debug message */
    //#define __DEBUG__
#endif

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Target Watchdog GPIO Interrupt */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#if !defined(PCB_REV_20230712)
/*---------------------------------------------------------------------------*/

volatile bool TargetWatchdogClear = false;

/*---------------------------------------------------------------------------*/
#pragma save
#pragma nooverlay
/*---------------------------------------------------------------------------*/
void int0_callback()
{
    TargetWatchdogClear = true;
}
/*---------------------------------------------------------------------------*/
#pragma restore

/*---------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
float battery_adc_volt_cal (void)
{
    float adc_raw = analogRead(PORT_BAT_ADC);

    return (float)MICOM_VDD_mV / (float)MICOM_ADC_RES * adc_raw;
}

/*---------------------------------------------------------------------------*/
float battery_adc_volt (void)
{
    float volt_max, volt_min, volt_sum, volt;

    volt_sum = 0;
    volt_max = volt_min = battery_adc_volt_cal ();

    for (unsigned char cnt = 0; cnt < ADC_SAMPLING_CNT; cnt++) {
        volt = battery_adc_volt_cal ();
        if (volt_max < volt)    volt_max = volt;
        if (volt_min > volt)    volt_min = volt;
        volt_sum += volt;
    }
    volt_sum -= (volt_max + volt_min);

    return  volt_sum / (float)(ADC_SAMPLING_CNT - 2);
}

/*---------------------------------------------------------------------------*/
float battery_volt (void)
{
    float resistor = (float)(BAT_ADC_R1 + BAT_ADC_R2) / (float)BAT_ADC_R2;
    float volt = battery_adc_volt () * resistor;

    return  volt;
}

/*---------------------------------------------------------------------------*/
void battery_avr_volt_init (void)
{
    /* sample arrary fill (power on boot, bat status err) */
    for (unsigned char i = 0; i < BATTERY_AVR_SAMPLE_COUNT; i++) {
        BatteryVoltSamples[i] = battery_volt();
    }
}

/*---------------------------------------------------------------------------*/
float battery_avr_volt (enum eBATTERY_STATUS battery_status)
{
    /* 처음 booting시 초기화 */
    static bool ErrorFlag = true;
    static int  SamplePos = 0;

    float avr_volt = 0.;

    /* battery remove 상태에서는 0v 값을 전달한다.*/
    if (battery_status == eBATTERY_REMOVED) {
        ErrorFlag = true;
        return 0.0;
    }

    /* battery remove 상태에서 회복된 경우 avr값을 다시 산출하기 위하여 arrary를 초기화한다.*/
    if (ErrorFlag) {
        battery_avr_volt_init ();
        SamplePos = 0;
        ErrorFlag = false;
    }
    BatteryVoltSamples[SamplePos] = battery_volt();

    SamplePos = (SamplePos + 1) % BATTERY_AVR_SAMPLE_COUNT;

    for (unsigned char i = 0; i < BATTERY_AVR_SAMPLE_COUNT; i++)
        avr_volt += BatteryVoltSamples[i];

    return  (avr_volt / BATTERY_AVR_SAMPLE_COUNT);
}

/*---------------------------------------------------------------------------*/
enum eBATTERY_STATUS battery_status (void)
{
    LED_CHRG_STATUS = digitalRead(PORT_LED_CHRG);
    LED_FULL_STATUS = digitalRead(PORT_LED_FULL);

    if (!LED_FULL_STATUS) {
        /* Battery remove상태에서는 FULL은 0이고 CHRG는 Pulse 값을 가짐 */
        /* CHRG값이 0으로 떨어지는지 확인하여 Battery remove상태인지 확인함 (300ms). */
        unsigned long check_mills = millis();
        while (millis() < (check_mills + 300)) {
            if (!digitalRead(PORT_LED_CHRG)) {
                LED_CHRG_STATUS = 0;
                break;
            }
        }
#if defined (__DEBUG__)
        if (!LED_CHRG_STATUS)
            USBSerial_println("Battery Removed...");
#endif
    }

    if      ( LED_CHRG_STATUS &&  LED_FULL_STATUS)
        return eBATTERY_DISCHARGING;
    else if (!LED_CHRG_STATUS &&  LED_FULL_STATUS)
        return eBATTERY_CHARGING;
    else if  (LED_CHRG_STATUS && !LED_FULL_STATUS)
        return eBATTERY_FULL;
    else // (!LED_CHRG_STATUS && !LED_FULL_STATUS)
        return eBATTERY_REMOVED;
}

/*---------------------------------------------------------------------------*/
void battery_level_display (enum eBATTERY_STATUS bat_status, unsigned long bat_volt)
{
    static bool BlinkStatus = 0;
    static unsigned long DispBatVolt = 0;

    /* 배터리 상태를 비교할 초기값 설정, 충전시는 항상 led up, 방전시는 항산 led dn */
    if (!DispBatVolt)
        DispBatVolt = bat_volt;

    /* led blink control */
    BlinkStatus = !BlinkStatus;

    switch (bat_status) {
        case eBATTERY_DISCHARGING:
            /* 전압이 낮아지는 경우에만 업데이트 */
            if (DispBatVolt > bat_volt)
                DispBatVolt = bat_volt;
            break;

        case eBATTERY_FULL:
        case eBATTERY_CHARGING:
            /* 전압이 높아지는 경우에만 업데이트 */
            if (DispBatVolt < bat_volt)
                DispBatVolt = bat_volt;
            break;

        default:
        case eBATTERY_REMOVED:
            DispBatVolt = 0;
            /* Battery error 상태에서는 LED4, LED1을 점멸하여 상태이상을 표시함 */
            digitalWrite(PORT_LED_LV4, BlinkStatus);
            digitalWrite(PORT_LED_LV1, BlinkStatus);
            digitalWrite(PORT_LED_LV3, 0);
            digitalWrite(PORT_LED_LV2, 0);
            LED_LV4_STATUS = LED_LV1_STATUS = 1;
            LED_LV3_STATUS = LED_LV2_STATUS = 0;
            return;
    }

    /* 일반적인 상태에서는 현재 battery level에 맞게 LED를 점등함.(점멸x) */
    LED_LV4_STATUS = DispBatVolt > BAT_LEVEL_LV4 ? 1 : 0;
    LED_LV3_STATUS = DispBatVolt > BAT_LEVEL_LV3 ? 1 : 0;
    LED_LV2_STATUS = DispBatVolt > BAT_LEVEL_LV2 ? 1 : 0;
    LED_LV1_STATUS = DispBatVolt > BAT_LEVEL_LV1 ? 1 : 0;

    digitalWrite(PORT_LED_LV4, LED_LV4_STATUS);
    digitalWrite(PORT_LED_LV3, LED_LV3_STATUS);
    digitalWrite(PORT_LED_LV2, LED_LV2_STATUS);
    digitalWrite(PORT_LED_LV1, LED_LV1_STATUS);

    /* 배터리 방전상태에서는 현재 배터리 잔량위치의 LED 점멸함 */
    if (bat_status == eBATTERY_DISCHARGING) {
        if      (DispBatVolt > BAT_LEVEL_LV4)
            digitalWrite(PORT_LED_LV4, BlinkStatus);
        else if (DispBatVolt > BAT_LEVEL_LV3)
            digitalWrite(PORT_LED_LV3, BlinkStatus);
        else if (DispBatVolt > BAT_LEVEL_LV2)
            digitalWrite(PORT_LED_LV2, BlinkStatus);
        else if (DispBatVolt > BAT_LEVEL_LV1)
            digitalWrite(PORT_LED_LV1, BlinkStatus);
    }
}

/*---------------------------------------------------------------------------*/
void request_data_send(char cmd)
{
    USBSerial_print((char)'@');
    USBSerial_print(cmd);
    switch(cmd) {
        case    'L':
            USBSerial_print((int)LED_LV4_STATUS);   USBSerial_print((int)LED_LV3_STATUS);
            USBSerial_print((int)LED_LV2_STATUS);   USBSerial_print((int)LED_LV1_STATUS);
            break;
        case    'V':
            /* Battery자리수가 4자리가 아닌 경우 Battery Error */
            if ((BatteryStatus != eBATTERY_REMOVED) &&
                ((BatteryAvrVolt < 10000) && (BatteryAvrVolt > 999))) {
                USBSerial_print(BatteryAvrVolt);
            }
            else
                USBSerial_print("0000");
            break;
        case    'C':
            USBSerial_print((char)'F');
            USBSerial_print((int) LED_FULL_STATUS);
            USBSerial_print((char)'C');
            USBSerial_print((int) LED_CHRG_STATUS);
            break;
        case    'W':
            if (PeriodRequestWatchdogTime)
                USBSerial_print(PeriodRequestWatchdogTime);
            else
                USBSerial_print("0000");
            break;
        case    'P':
                USBSerial_print("-OFF");
                RESET_KEEP = BatteryStatus | RESET_KEEP_POWEROFF;
            break;
        case    'F':
                USBSerial_print(FW_VERSION);
            break;
    }
    USBSerial_print((char)'#');
    USBSerial_print("\r\n");
}

/*---------------------------------------------------------------------------*/
void protocol_check(void)
{
    if (USBSerial_available()) {
        /* protocol q */
        Protocol[0] = Protocol[1];  Protocol[1] = Protocol[2];
        Protocol[2] = Protocol[3];  Protocol[3] = USBSerial_read();

        /* Header & Tail check */
        if ((Protocol[0] == '@') && (Protocol[3] == '#')) {
            char cmd  = Protocol[1], data = Protocol[2];
            /* cal period ms */
            unsigned long cal_period =
                ((data <= '9') && (data >= '0')) ? (data - '0') * 1000 : 0;
            unsigned long cur_millis = millis();

            switch (cmd) {
                case    'L':
                    PeriodRequestBatteryLevel = cal_period;
                    MillisRequestBatteryLevel = cur_millis;
                    break;
                case    'V':
                    PeriodRequestBatteryVoltage = cal_period;
                    MillisRequestBatteryVoltage = cur_millis;
                    break;
                case    'C':
                    PeriodRequestChagerStatus = cal_period;
                    MillisRequestChagerStatus = cur_millis;
                    break;
                case    'W':
                    /* Target Power가 ON인 상태에서만 Watchdog control */
                    if (RESET_KEEP & RESET_KEEP_POWEROFF) {
                        PeriodRequestWatchdogTime = 0;
                        MillisRequestWatchdogTime = 0;
                    } else {
                        PeriodRequestWatchdogTime = cal_period;
                        MillisRequestWatchdogTime = cur_millis;
                    }
                    break;
                case    'P':
                    /* system watch dog & repeat flag all clear */
                    RESET_KEEP |= RESET_KEEP_POWEROFF;
                    repeat_data_clear();
                    break;
                case    'F':
                    /*
                        Firmware Version request
                    */
                    break;

#if defined (__DEBUG__)
                /* GPIO Set Test */
                case    'S':    case    's':
                    if ((data == 'R') || (data == 'r')) {
                        USBSerial_println("PORT_CTL_RESET Output(High)");
                        pinMode (PORT_CTL_RESET, OUTPUT);   digitalWrite (PORT_CTL_RESET, 1);
                    }
                    if ((data == 'P') || (data == 'p')) {
                        USBSerial_println("PORT_CTL_POWER Output(High)");
                        pinMode (PORT_CTL_POWER, OUTPUT);   digitalWrite (PORT_CTL_POWER, 1);
                    }
                    return;
                /* GPIO Reset Test */
                case    'R':    case    'r':
                    if ((data == 'R') || (data == 'r')) {
                        USBSerial_println("PORT_CTL_RESET Input(Low)");
                        pinMode (PORT_CTL_RESET, INPUT);
                    }
                    if ((data == 'P') || (data == 'p')) {
                        USBSerial_println("PORT_CTL_POWER Input(Low)");
                        pinMode (PORT_CTL_POWER, INPUT);
                    }
                    return;
                /* ups watchdog test */
                case    'X':
                        USBSerial_println("ups watchdog test");
                        USBSerial_flush();
                        delay(3000);
                    break;
#endif
                default:
                    return;
            }
            request_data_send (cmd);
        }
    }
}

/*---------------------------------------------------------------------------*/
void repeat_data_clear (void)
{
    PeriodRequestBatteryLevel   = 0;    MillisRequestBatteryLevel   = 0;
    PeriodRequestBatteryVoltage = 0;    MillisRequestBatteryVoltage = 0;
    PeriodRequestChagerStatus   = 0;    MillisRequestChagerStatus   = 0;
    PeriodRequestWatchdogTime   = 0;    MillisRequestWatchdogTime   = 0;
}

/*---------------------------------------------------------------------------*/
void repeat_data_check (void)
{
    unsigned long period;

    if (PeriodRequestBatteryLevel) {
        period = MillisRequestBatteryLevel + PeriodRequestBatteryLevel;
        if (period < millis()) {
            MillisRequestBatteryLevel = millis();
            request_data_send ('L');
        }
    }

    if (PeriodRequestBatteryVoltage) {
        period = MillisRequestBatteryVoltage + PeriodRequestBatteryVoltage;
        if (period < millis()) {
            MillisRequestBatteryVoltage = millis();
            request_data_send ('V');
        }
    }

    if (PeriodRequestChagerStatus) {
        period = MillisRequestChagerStatus + PeriodRequestChagerStatus;
        if (period < millis()) {
            MillisRequestChagerStatus = millis();
            request_data_send ('C');
        }
    }

    if (PeriodRequestWatchdogTime) {
        period = MillisRequestWatchdogTime + PeriodRequestWatchdogTime;
        if (period < millis()) {
            PeriodRequestWatchdogTime = 0;
#if defined (__DEBUG__)
            USBSerial_println ("Target watchdog Overflow event...");
#endif
            target_system_reset ();
        }
    }
}

/*---------------------------------------------------------------------------*/
void target_system_reset (void)
{
    pinMode (PORT_CTL_RESET, OUTPUT);
    digitalWrite (PORT_CTL_RESET, 0);   delay (TARGET_RESET_DELAY);
    digitalWrite (PORT_CTL_RESET, 1);   delay (TARGET_RESET_DELAY);
    digitalWrite (PORT_CTL_RESET, 0);

#if !defined(PCB_REV_20230712)
    pinMode (PORT_CTL_RESET, INPUT);
#endif

#if defined (__DEBUG__)
    USBSerial_println ("Target system reset...");
#endif
}

/*---------------------------------------------------------------------------*/
void target_system_power (bool onoff)
{
    if (onoff) {
#if !defined(PCB_REV_20230712)
        /* target system force power off */
        pinMode (PORT_CTL_POWER, INPUT);
        pinMode (PORT_CTL_RESET, INPUT);
#else
        digitalWrite (PORT_CTL_POWER, 0);
        digitalWrite (PORT_CTL_RESET, 0);
#endif
        RESET_KEEP &= ~(RESET_KEEP_POWEROFF);

#if defined (__DEBUG__)
        USBSerial_println ("Target system power on...");
#endif
    } else {
        /* target system force power off */
        pinMode (PORT_CTL_POWER, OUTPUT);   digitalWrite (PORT_CTL_POWER, 1);
        pinMode (PORT_CTL_RESET, OUTPUT);   digitalWrite (PORT_CTL_RESET, 1);

#if defined (__DEBUG__)
        USBSerial_println ("Target system force power off...");
#endif
        RESET_KEEP |=  (RESET_KEEP_POWEROFF);
    }
    USBSerial_flush ();
}

/*---------------------------------------------------------------------------*/
void port_init(void)
{
    pinMode(PORT_BAT_ADC,  INPUT);
    pinMode(PORT_LED_CHRG, INPUT_PULLUP);
    pinMode(PORT_LED_FULL, INPUT_PULLUP);
    LED_CHRG_STATUS = 0;    LED_FULL_STATUS = 0;

#if !defined(PCB_REV_20230712)
    pinMode (PORT_CTL_POWER, INPUT);
    pinMode (PORT_CTL_RESET, INPUT);
#else
    pinMode(PORT_CTL_RESET, OUTPUT);    digitalWrite(PORT_CTL_RESET, 0);
    pinMode(PORT_CTL_POWER, OUTPUT);    digitalWrite(PORT_CTL_POWER, 0);
#endif

    /* Target GPIO Watchdog PIN */
    pinMode(PORT_WATCHDOG, INPUT);

    /* Battery level led all clear */
    LED_LV4_STATUS = 0;     LED_LV3_STATUS = 0;
    LED_LV2_STATUS = 0;     LED_LV1_STATUS = 0;
    pinMode(PORT_LED_LV4, OUTPUT);      digitalWrite(PORT_LED_LV4, LED_LV4_STATUS);
    pinMode(PORT_LED_LV3, OUTPUT);      digitalWrite(PORT_LED_LV3, LED_LV3_STATUS);
    pinMode(PORT_LED_LV2, OUTPUT);      digitalWrite(PORT_LED_LV2, LED_LV2_STATUS);
    pinMode(PORT_LED_LV1, OUTPUT);      digitalWrite(PORT_LED_LV1, LED_LV1_STATUS);
}

/*---------------------------------------------------------------------------*/
void setup()
{
    /* UPS GPIO Port Init */
    port_init();

    /* USB Serial init */
    USBSerial_flush();

    /* Reset flag를 검사하여 F/W 업데이트의 경우 시스템 reset을 하지 않도록 함. */
    if (RESET_KEEP == 0) {
        /* 일정 Battery Level 확인 전 까지 Target system Power off */
        target_system_power (false);
        /* Power On Event활성화 */
        RESET_KEEP |=  (RESET_KEEP_POWERON);
    } else {
        /* F/W업데이트시 기존 POWER 상태 복원 */
        if ((RESET_KEEP & RESET_KEEP_POWEROFF) == 0)
            target_system_power (true);
        /* Power On Event clear */
        RESET_KEEP &= ~(RESET_KEEP_POWERON);
    }

    /* for target watchdog (P3.2 INT0), Only falling trigger */
#if !defined(PCB_REV_20230712)
    attachInterrupt(0, int0_callback, FALLING);
    detachInterrupt(0);
#endif

    /* Repeat flag 초기화 */
    repeat_data_clear();

    /* Battery sampling arrary 초기화 */
    battery_avr_volt (eBATTERY_REMOVED);
    battery_avr_volt (battery_status ());

    /* UPS System watchdog enable */
    GLOBAL_CFG_UNLOCK();
    WDT_ENABLE();
    WDT_CLR();
}

/*---------------------------------------------------------------------------*/
void loop()
{
    /* UPS System watchdog reset */
    WDT_CLR();

    /* Target이 OFF상태의 경우에는 Loop를 2.5sec 마다 한번씩 실행하므로 ADC Update가 느릴 수 있음. */
    if(MillisLoop + PERIOD_LOOP_MILLIS < millis()) {
        MillisLoop      = millis ();
        BatteryAdcVolt  = battery_adc_volt ();
        BatteryVolt     = battery_volt ();
        BatteryStatus   = battery_status ();
        BatteryAvrVolt  = battery_avr_volt (BatteryStatus);

        battery_level_display (BatteryStatus, BatteryAvrVolt);
        repeat_data_check();
        /* Target Power Off status */
        if (RESET_KEEP & RESET_KEEP_POWEROFF) {
            /*
                Target의 Battery Status가 Discharging에서 Charging으로 바뀐경우
                USB Cable을 재 접속한 것이므로 Power On Event를 활성화 시키고,
                바로 꺼짐을 방지하기 위하여 BAT_LEVEL_BOOT 전압보다 큰 경우에 Power On을 시킨다.

                처음 Power On booting시 PowerOnEvent는 true상태이며 Main loop에서 Battery Level확인 후
                Power On을 시킨다.

                RESET_KEEP Register는 Power On reset이 아닌 경우 계속하여 기존의 값을 유지하고 있으므로
                기존 상태의 Battery Status를 기록하여 Battery상태의 변화를 감지하여 Power On Event를 생성하거나
                F/W update후 reset상태에서도 기존의 UPS상태를 유지하기 위하여 사용한다.
            */
            unsigned char OldBatteryStatus = RESET_KEEP & ~(RESET_KEEP_POWEROFF | RESET_KEEP_POWERON);

            if ((BatteryStatus != eBATTERY_REMOVED) && (OldBatteryStatus != BatteryStatus)) {

                RESET_KEEP = (RESET_KEEP | BatteryStatus | RESET_KEEP_POWEROFF);

                /* Discharge -> Charging 2초안에 이루어지는 경우 detect를 하지 못하는 경우 발생함. */
                /* 기본적으로 2초 주기로 검사하기 때문에 발생하는 문제임. */
                switch (BatteryStatus) {
                    case    eBATTERY_CHARGING:
                    case    eBATTERY_FULL:
                        RESET_KEEP |= RESET_KEEP_POWERON;
                    case    eBATTERY_DISCHARGING:
                        if (RESET_KEEP & RESET_KEEP_POWERON) {
                            if (BatteryAvrVolt > BAT_LEVEL_BOOT) {
                                RESET_KEEP &= ~(RESET_KEEP_POWERON);
                                repeat_data_clear();
                                target_system_power (true);
                            }
                        }
                    break;
                }
            }
            /* LED on display 시간을 100ms만 표시 */
            delay (POWEROFF_LED_DELAY);
            digitalWrite(PORT_LED_LV4, 0);  digitalWrite(PORT_LED_LV3, 0);
            digitalWrite(PORT_LED_LV2, 0);  digitalWrite(PORT_LED_LV1, 0);
            /* main loop를 2000ms 후에 다시 진입하도록 Offset값을 추가 */
            MillisLoop = millis() + PEROID_OFF_MILLIS;
        } else {
            /* Battery Level이 3400mV보다 낮은 경우 강제로 Power off */
            if ((BAT_LEVEL_OFF > BatteryAvrVolt) && (BatteryStatus != eBATTERY_REMOVED)) {
#if defined(__DEBUG__)
                USBSerial_println("Battery Level < 3400mV. Force Power Off...");
#endif
                target_system_power (false);
            }
        }
    }

    /* GPIO Trigger watchdog reset */
#if !defined(PCB_REV_20230712)
    if (TargetWatchdogClear) {
        TargetWatchdogClear = false;
        /* updata watchdog time */
        MillisRequestWatchdogTime = millis();
    }
#endif
    /* Serial message check */
    protocol_check();

#if defined (__DEBUG_LOG__)
    /* for battery discharge graph */
    if (MillisLog + PERIOD_LOG_MILLIS < millis()) {
        MillisLog = millis ();
        /* log count, status, BattaryAvrVolt, BatteryVolt, BatteryAdcVolt */
        USBSerial_print(LogCount);              USBSerial_print(",");
        USBSerial_print((int)LED_CHRG_STATUS);  USBSerial_print(",");
        USBSerial_print((int)LED_FULL_STATUS);  USBSerial_print(",");
        USBSerial_print(BatteryAvrVolt);        USBSerial_print(",");
        USBSerial_print(BatteryVolt);           USBSerial_print(",");
        USBSerial_print(BatteryAdcVolt);        USBSerial_print("\r\n");
        LogCount++;
    }
#endif

    USBSerial_flush();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


