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
#include "ups_fw.h"

/*---------------------------------------------------------------------------*/
#define FW_VERSION  "V0-2"

/*---------------------------------------------------------------------------*/
/* Debug Enable Flag */
/*---------------------------------------------------------------------------*/
//#define __DEBUG_LOG__

#if defined (__DEBUG_LOG__)

/* for Battery discharge graph (log) */
#define PERIOD_LOG_MILLIS   1000
__xdata unsigned long MillisLog = 0;
__xdata unsigned long LogCount = 0;

void display_battery_status (void)
{
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
        if (BatteryStatus == eBATTERY_DISCHARGING)
            LogCount++;
    }
}

#endif

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* Target Watchdog GPIO Interrupt */
/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
void LED_SetState (unsigned char state)
{
    switch (state) {
        case    0:  // All off
            LED_LV4_STATUS = 0; LED_LV3_STATUS = 0;
            LED_LV2_STATUS = 0; LED_LV1_STATUS = 0;
            break;
        /* toggle state */
        case    1:  LED_LV1_STATUS = !LED_LV1_STATUS;   break;
        case    2:  LED_LV2_STATUS = !LED_LV2_STATUS;   break;
        case    3:  LED_LV3_STATUS = !LED_LV3_STATUS;   break;
        case    4:  LED_LV4_STATUS = !LED_LV4_STATUS;   break;
        default:
            LED_LV4_STATUS = 1; LED_LV3_STATUS = 1;
            LED_LV2_STATUS = 1; LED_LV1_STATUS = 1;
            break;
    }
    digitalWrite(PORT_LED_LV4, LED_LV4_STATUS);
    digitalWrite(PORT_LED_LV3, LED_LV3_STATUS);
    digitalWrite(PORT_LED_LV2, LED_LV2_STATUS);
    digitalWrite(PORT_LED_LV1, LED_LV1_STATUS);
}

/*---------------------------------------------------------------------------*/
void LED_Test (void)
{
    /* UPS System watchdog disable */
    GLOBAL_CFG_UNLOCK();
    WDT_DISABLE();
    WDT_CLR();

    LED_SetState (0);   delay(300); LED_SetState (9);   delay(300);
    LED_SetState (0);   delay(300); LED_SetState (9);   delay(300);
    // All set
    LED_SetState (0);   delay(300);
    LED_SetState (1);   delay(500); LED_SetState (2);   delay(500);
    LED_SetState (3);   delay(500); LED_SetState (4);   delay(500);
    LED_SetState (4);   delay(500); LED_SetState (3);   delay(500);
    LED_SetState (2);   delay(500); LED_SetState (1);   delay(500);
    // All clear
    LED_SetState (0);   delay(300);

    /* target watchdog reset */
    MillisRequestWatchdogTime = millis();

    /* UPS System watchdog enable */
    GLOBAL_CFG_UNLOCK();
    WDT_ENABLE();
    WDT_CLR();
}

/*---------------------------------------------------------------------------*/
unsigned long cal_battery_level (unsigned int val)
{
    return  (3500 + (val * 50));
}

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

    for (unsigned char cnt = 0; cnt < ADC_SAMPLE_CNT; cnt++) {
        volt = battery_adc_volt_cal ();
        if (volt_max < volt)    volt_max = volt;
        if (volt_min > volt)    volt_min = volt;
        volt_sum += volt;
    }
    volt_sum -= (volt_max + volt_min);

    return  volt_sum / (float)(ADC_SAMPLE_CNT - 2);
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
    for (unsigned char i = 0; i < BAT_AVR_SAMPLE_CNT; i++) {
        BatteryVoltSamples[i] = battery_volt();
    }
}

/*---------------------------------------------------------------------------*/
float battery_avr_volt (enum eBATTERY_STATUS battery_status)
{
    /* 처음 booting시 초기화 */
    static char BatteryRemove = false;
    static int  SamplePos = 0;

    float avr_volt = 0.;

    /* battery remove 상태에서는 0v 값을 전달한다.*/
    if (battery_status == eBATTERY_REMOVED) {
        BatteryRemove = true;
        return 0.0;
    }

    /* battery remove 상태에서 회복된 경우 avr값을 다시 산출하기 위하여 arrary를 초기화한다.*/
    if (BatteryRemove) {
        BatteryRemove = false;  SamplePos = 0;
        battery_avr_volt_init ();
    }
    BatteryVoltSamples[SamplePos] = battery_volt();

    SamplePos = (SamplePos + 1) % BAT_AVR_SAMPLE_CNT;

    for (unsigned char i = 0; i < BAT_AVR_SAMPLE_CNT; i++)
        avr_volt += BatteryVoltSamples[i];

    return  (avr_volt / BAT_AVR_SAMPLE_CNT);
}

/*---------------------------------------------------------------------------*/
enum eBATTERY_STATUS battery_status (void)
{
    static bool BatteryRemove = false;

    LED_CHRG_STATUS = digitalRead(PORT_LED_CHRG);
    LED_FULL_STATUS = digitalRead(PORT_LED_FULL);

    if (!LED_FULL_STATUS) {
        /* Battery remove상태에서는 FULL은 0이고 CHRG는 Pulse 값을 가짐 */
        /* CHRG값이 0으로 떨어지는지 확인하여 Battery remove상태인지 확인함 (300ms). */
        unsigned long check_mills = millis();
        while (millis() < (check_mills + 300)) {
            if (!digitalRead(PORT_LED_CHRG)) {
                LED_CHRG_STATUS = 0;
                BatteryRemove = true;
                return eBATTERY_REMOVED;
            }
        }
#if defined (__DEBUG__)
        if (!LED_CHRG_STATUS)
            USBSerial_println("Battery Removed...");
#endif
    }

    if (BatteryRemove) {
        BatteryRemove = false;
        LED_CHRG_STATUS = 0;    LED_FULL_STATUS = 0;
        return eBATTERY_REMOVED;
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
    LED_LV4_STATUS = DispBatVolt > BAT_DISPLAY_LV4 ? 1 : 0;
    LED_LV3_STATUS = DispBatVolt > BAT_DISPLAY_LV3 ? 1 : 0;
    LED_LV2_STATUS = DispBatVolt > BAT_DISPLAY_LV2 ? 1 : 0;
    LED_LV1_STATUS = DispBatVolt > BAT_DISPLAY_LV1 ? 1 : 0;

    digitalWrite(PORT_LED_LV4, LED_LV4_STATUS);
    digitalWrite(PORT_LED_LV3, LED_LV3_STATUS);
    digitalWrite(PORT_LED_LV2, LED_LV2_STATUS);
    digitalWrite(PORT_LED_LV1, LED_LV1_STATUS);

    /* 배터리 방전상태에서는 현재 배터리 잔량위치의 LED 점멸함 */
    if (bat_status == eBATTERY_DISCHARGING) {
        if      (DispBatVolt > BAT_DISPLAY_LV4)
            digitalWrite(PORT_LED_LV4, BlinkStatus);
        else if (DispBatVolt > BAT_DISPLAY_LV3)
            digitalWrite(PORT_LED_LV3, BlinkStatus);
        else if (DispBatVolt > BAT_DISPLAY_LV2)
            digitalWrite(PORT_LED_LV2, BlinkStatus);
        else if (DispBatVolt > BAT_DISPLAY_LV1)
            digitalWrite(PORT_LED_LV1, BlinkStatus);
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
        /* GPIO Trigger watchdog reset */
        if (TargetWatchdogClear) {
            TargetWatchdogClear = false;
            /* updata watchdog time */
            MillisRequestWatchdogTime = millis();
        }
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
                rPOWER_STATE.bits.bPowerOffEvent = true;
                rPOWER_STATE.bits.ePrevBatteryStatus = BatteryStatus;
                RESET_KEEP = rPOWER_STATE.byte;
            break;
        case    'F':
                USBSerial_print(FW_VERSION);
            break;
        case    'O':
                if (BatteryBootVolt)
                    USBSerial_print(BatteryBootVolt);
                else
                    USBSerial_print("0000");
            break;
        case    'R':    case    'B':
            {
                int gpio = ('R' == cmd) ? PORT_CTL_RESET : PORT_CTL_POWER;
                USBSerial_print(digitalRead (gpio)? "0001" : "0000");
            }
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
                    if (rPOWER_STATE.bits.bPowerOffEvent) {
                        PeriodRequestWatchdogTime = 0;
                        MillisRequestWatchdogTime = 0;
                    } else {
                        PeriodRequestWatchdogTime = cal_period;
                        MillisRequestWatchdogTime = cur_millis;
                    }
                    break;
                case    'P':
                    /* system watch dog & repeat flag all clear */
                    rPOWER_STATE.bits.bPowerOffEvent = true;
                    RESET_KEEP = rPOWER_STATE.byte;
                    repeat_data_clear();
                    break;
                case    'O':
                    /*---------------------------------------------------------------------------*/
                    /* Set battery level for system power on */
                    /* Power on when battery charge condition detected.(default) */
                    /* 0     : Detect charging status.(default) */
                    /* 1 ~ 9 : BATTERY LEVEL */
                    /* #define CAL_BAT_LEVEL(x)    (3500 + (x * 50)) */
                    /*---------------------------------------------------------------------------*/
                    if ((data <= '9') && (data >= '0'))
                        BatteryBootVolt = cal_battery_level(data - '0');
                    break;
                case    'F':
                    /*
                        Firmware Version request
                    */
                    break;
                /* ups watchdog reset test */
                case    'X':
                        USBSerial_println("@W-RST#");
                        USBSerial_flush();
                        while (1);
                    return;
                case    'T':
                    /*
                        LED Test Firmware
                    */
                    LED_Test();
                    USBSerial_println("@T-END#");
                    return;

                /* RESET, PWRBTN GPIO Test */
                case    'R':    case    'B':
                    {
                        int gpio = ('R' == cmd) ? PORT_CTL_RESET : PORT_CTL_POWER;
                        pinMode (gpio, OUTPUT);
                        switch (data) {
                            case    '0':    digitalWrite (gpio, 0); break;
                            case    '1':    digitalWrite (gpio, 1); break;
                            default:
                                /* return setting value */
                                break;
                        }
                    }
                    break;
                default:
                    return;
            }
            request_data_send (cmd);
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

    pinMode (PORT_CTL_RESET, INPUT);

#if defined (__DEBUG__)
    USBSerial_println ("Target system reset...");
#endif
}

/*---------------------------------------------------------------------------*/
void target_system_power (bool onoff)
{
    if (onoff) {
        /* target system force power off */
        pinMode (PORT_CTL_POWER, INPUT);    pinMode (PORT_CTL_RESET, INPUT);

        rPOWER_STATE.bits.bPowerOffEvent = false;

#if defined (__DEBUG__)
        USBSerial_println ("Target system power on...");
#endif
    } else {
        /* target system force power off */
        pinMode (PORT_CTL_POWER, OUTPUT);   pinMode (PORT_CTL_RESET, OUTPUT);
        digitalWrite (PORT_CTL_POWER, 1);   digitalWrite (PORT_CTL_RESET, 1);

        rPOWER_STATE.bits.bPowerOffEvent = true;

#if defined (__DEBUG__)
        USBSerial_println ("Target system force power off...");
#endif
    }
    RESET_KEEP = rPOWER_STATE.byte;
    USBSerial_flush ();
}

/*---------------------------------------------------------------------------*/
void port_init(void)
{
    pinMode(PORT_BAT_ADC,  INPUT);
    pinMode(PORT_LED_CHRG, INPUT_PULLUP);
    pinMode(PORT_LED_FULL, INPUT_PULLUP);
    LED_CHRG_STATUS = 0;    LED_FULL_STATUS = 0;

    pinMode(PORT_CTL_POWER, INPUT);
    pinMode(PORT_CTL_RESET, INPUT);

    /* Target GPIO Watchdog PIN */
    pinMode(PORT_WATCHDOG, INPUT);

    /* Battery level led all clear */
    LED_LV4_STATUS = 0;     LED_LV3_STATUS = 0;
    LED_LV2_STATUS = 0;     LED_LV1_STATUS = 0;
    pinMode(PORT_LED_LV4, OUTPUT);  digitalWrite(PORT_LED_LV4, LED_LV4_STATUS);
    pinMode(PORT_LED_LV3, OUTPUT);  digitalWrite(PORT_LED_LV3, LED_LV3_STATUS);
    pinMode(PORT_LED_LV2, OUTPUT);  digitalWrite(PORT_LED_LV2, LED_LV2_STATUS);
    pinMode(PORT_LED_LV1, OUTPUT);  digitalWrite(PORT_LED_LV1, LED_LV1_STATUS);
}

/*---------------------------------------------------------------------------*/
void setup()
{
    /* UPS GPIO Port Init */
    port_init();

    /* USB Serial init */
    USBSerial_flush();

    rPOWER_STATE.byte = RESET_KEEP;

    /* Reset flag를 검사하여 F/W 업데이트의 경우 시스템 reset을 하지 않도록 함. */
    if (rPOWER_STATE.byte == 0) {
        /* 처음 power on시에는 charging 상태에서도 켜지게 함. */
        BatteryBootVolt = 0;
        /* 일정 Battery Level 확인 전 까지 Target system Power off */
        target_system_power (false);
        /* Power On Event활성화 */
        rPOWER_STATE.bits.bPowerOnEvent = true;
    } else {
        /* F/W업데이트시 기존 POWER 상태 복원 */
        if (!rPOWER_STATE.bits.bPowerOffEvent)
            target_system_power (true);
        /* Power On Event clear */
        rPOWER_STATE.bits.bPowerOnEvent = false;
    }

    RESET_KEEP = rPOWER_STATE.byte;

    /* for target watchdog (P3.2 INT0), Only falling trigger */
    attachInterrupt(0, int0_callback, FALLING);

    /* Repeat flag 초기화 */
    repeat_data_clear();

    /* Battery sampling arrary 초기화 */
    battery_avr_volt_init ();

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
        if (rPOWER_STATE.bits.bPowerOffEvent) {
            /*
                Target의 Battery Status가 Discharging에서 Charging으로 바뀐경우
                USB Cable을 재 접속한 것이므로 Power On Event를 활성화 시키고,
                바로 꺼짐을 방지하기 위하여 BatteryBootVolt 전압보다 큰 경우에 Power On을 시킨다.

                처음 Power On booting시 PowerOnEvent는 true상태이며 Main loop에서 BatteryBootVolt확인 후
                Power On을 시킨다.

                Power on reset의 경우 초기 BatteryBootVolt = 0으로 charging모드의 경우 바로 켜지도록 하며
                차후 command를 통하여 두번째 power on부터는 BatteryBootVolt를 적용하도록 한다.

                RESET_KEEP Register는 Power On reset이 아닌 경우 계속하여 기존의 값을 유지하고 있으므로
                기존 상태의 Battery Status를 기록하여 Battery상태의 변화를 감지하여 Power On Event를 생성하거나
                F/W update후 reset상태에서도 기존의 UPS상태를 유지하기 위하여 사용한다.
            */
            if ((BatteryStatus != eBATTERY_REMOVED) &&
                (rPOWER_STATE.bits.ePrevBatteryStatus != BatteryStatus)) {

                rPOWER_STATE.bits.ePrevBatteryStatus = BatteryStatus;
                rPOWER_STATE.bits.bPowerOffEvent = true;

                /* Discharge -> Charging 2초안에 이루어지는 경우 detect를 하지 못하는 경우 발생함. */
                /* 기본적으로 2초 주기로 검사하기 때문에 발생하는 문제임. */
                switch (BatteryStatus) {
                    case    eBATTERY_CHARGING:
                    case    eBATTERY_FULL:
                        rPOWER_STATE.bits.bPowerOnEvent = true;
                    case    eBATTERY_DISCHARGING:
                        if (rPOWER_STATE.bits.bPowerOnEvent) {
                            if (BatteryAvrVolt > BatteryBootVolt) {
                                rPOWER_STATE.bits.bPowerOnEvent = false;
                                target_system_power (true);
                                /* Battery avr value init */
                                battery_avr_volt_init ();
                            }
                        }
                    break;
                }
                RESET_KEEP = rPOWER_STATE.byte;
            }
            /* LED on display 시간을 100ms만 표시 */
            delay (POWEROFF_LED_DELAY);
            digitalWrite(PORT_LED_LV4, 0);  digitalWrite(PORT_LED_LV3, 0);
            digitalWrite(PORT_LED_LV2, 0);  digitalWrite(PORT_LED_LV1, 0);
            /* main loop를 2000ms 후에 다시 진입하도록 Offset값을 추가 */
            MillisLoop = millis() + PEROID_OFF_MILLIS;
        }

        /* Battery Level이 3400mV보다 낮은 경우 강제로 Power off */
        if (BatteryAvrVolt < BAT_LEVEL_OFF) {
            target_system_power (false);
#if defined(__DEBUG__)
            USBSerial_println("Battery Level < 3400mV. Force Power Off...");
#endif
        }
    }

    /* Serial message check */
    protocol_check();

#if defined (__DEBUG_LOG__)
    display_battery_status ();
#endif

    USBSerial_flush();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


