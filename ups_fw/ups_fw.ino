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
#define FW_VERSION  "V0-6"

/*---------------------------------------------------------------------------*/
/* Debug Enable Flag */
/*---------------------------------------------------------------------------*/
//#define __DEBUG_LOG__

#if defined (__DEBUG_LOG__)

/* for Battery discharge graph (log) */
#define PERIOD_LOG_MILLIS   1000

__xdata unsigned long MillisLog      = 0;
__xdata unsigned long LogCount       = 0;
__xdata unsigned long BatteryVolt    = 0;
__xdata unsigned long BatteryAdcVolt = 0;

void display_battery_status (void)
{
    /* for battery discharge graph */
    if (MillisLog + PERIOD_LOG_MILLIS < millis()) {
        MillisLog = millis ();
        BatteryAdcVolt  = battery_adc_volt ();
        BatteryVolt     = battery_volt ();
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
void int0_callback ()
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
float battery_avr_volt (enum eBATTERY_STATUS bat_status)
{
    static char BatteryRemove = false;
    static int  SamplePos = 0;

    float avr_volt = 0.;

    if (bat_status == eBATTERY_REMOVED) {
        BatteryRemove = true;
        return 0.0;
    }

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
        unsigned long check_mills = millis();
        while (millis() < (check_mills + 300)) {
            if (!digitalRead(PORT_LED_CHRG)) {
                LED_CHRG_STATUS = 0;    BatteryRemove = true;
                return eBATTERY_REMOVED;
            }
        }
    }

    if (BatteryRemove) {
        BatteryRemove = false;  LED_CHRG_STATUS = 0;    LED_FULL_STATUS = 0;
        return eBATTERY_REMOVED;
    }

    if      ( LED_CHRG_STATUS &&  LED_FULL_STATUS)  return eBATTERY_DISCHARGING;
    else if (!LED_CHRG_STATUS &&  LED_FULL_STATUS)  return eBATTERY_CHARGING;
    else if  (LED_CHRG_STATUS && !LED_FULL_STATUS)  return eBATTERY_FULL;
    else // (!LED_CHRG_STATUS && !LED_FULL_STATUS)
        return eBATTERY_REMOVED;
}

/*---------------------------------------------------------------------------*/
void battery_level_display (enum eBATTERY_STATUS bat_status, unsigned long bat_volt)
{
    static bool BlinkStatus = 0;
    /* led blink control */
    BlinkStatus = !BlinkStatus;

    if (bat_status != eBATTERY_REMOVED) {
        LED_LV4_STATUS = bat_volt > BATTERY_DISPLAY_LV4 ? 1 : 0;
        LED_LV3_STATUS = bat_volt > BATTERY_DISPLAY_LV3 ? 1 : 0;
        LED_LV2_STATUS = bat_volt > BATTERY_DISPLAY_LV2 ? 1 : 0;
        LED_LV1_STATUS = bat_volt > BATTERY_DISPLAY_LV1 ? 1 : 0;

        digitalWrite(PORT_LED_LV4, LED_LV4_STATUS);
        digitalWrite(PORT_LED_LV3, LED_LV3_STATUS);
        digitalWrite(PORT_LED_LV2, LED_LV2_STATUS);
        digitalWrite(PORT_LED_LV1, LED_LV1_STATUS);

        if (bat_status == eBATTERY_DISCHARGING) {
            if      (bat_volt > BATTERY_DISPLAY_LV4)    digitalWrite(PORT_LED_LV4, BlinkStatus);
            else if (bat_volt > BATTERY_DISPLAY_LV3)    digitalWrite(PORT_LED_LV3, BlinkStatus);
            else if (bat_volt > BATTERY_DISPLAY_LV2)    digitalWrite(PORT_LED_LV2, BlinkStatus);
            else if (bat_volt > BATTERY_DISPLAY_LV1)    digitalWrite(PORT_LED_LV1, BlinkStatus);
        }
    } else {
        digitalWrite(PORT_LED_LV4, BlinkStatus);    digitalWrite(PORT_LED_LV1, BlinkStatus);
        digitalWrite(PORT_LED_LV3, 0);              digitalWrite(PORT_LED_LV2, 0);
        LED_LV4_STATUS = LED_LV1_STATUS = 1;
        LED_LV3_STATUS = LED_LV2_STATUS = 0;
    }
    if (rRESET_KEEP.bits.ePowerState == ePOWER_OFF) {
        delay (POWEROFF_LED_DELAY);
        digitalWrite(PORT_LED_LV4, 0);  digitalWrite(PORT_LED_LV3, 0);
        digitalWrite(PORT_LED_LV2, 0);  digitalWrite(PORT_LED_LV1, 0);
    }
}

/*---------------------------------------------------------------------------*/
void battery_level_update (enum eBATTERY_STATUS bat_status, unsigned long bat_volt)
{
    static unsigned long DispBatVolt = 0;

    if (!DispBatVolt)
        DispBatVolt = bat_volt;

    switch (bat_status) {
        case eBATTERY_DISCHARGING:
            if (DispBatVolt > bat_volt)
                DispBatVolt = bat_volt;
            break;
        case eBATTERY_FULL:
        case eBATTERY_CHARGING:
            if (DispBatVolt < bat_volt)
                DispBatVolt = bat_volt;
            break;
        default:
        case eBATTERY_REMOVED:
            DispBatVolt = 0;
            break;
    }
    battery_level_display (bat_status, DispBatVolt);
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
            target_system_reset ();
        }
    }
}

/*---------------------------------------------------------------------------*/
void request_data_send (char cmd)
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
void protocol_check (void)
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
                    if (rRESET_KEEP.bits.ePowerState == ePOWER_ON) {
                        PeriodRequestWatchdogTime = cal_period;
                        MillisRequestWatchdogTime = cur_millis;
                    } else {
                        PeriodRequestWatchdogTime = 0;
                        MillisRequestWatchdogTime = 0;
                    }
                    break;
                case    'P':
                    /* system watch dog & repeat flag all clear */
                    repeat_data_clear();
                    rRESET_KEEP.bits.ePowerState = ePOWER_OFF;
                    rRESET_KEEP.bits.bRestartCondition = false;
                    rRESET_KEEP.bits.bGpioStatus = true;
                    RESET_KEEP = rRESET_KEEP.byte;
                    break;
                case    'O':
                    /*---------------------------------------------------------------------------*/
                    /* Set battery level for system power on */
                    /* Power on when battery charge condition detected.(default) */
                    /* 0     : Detect charging status.(default) */
                    /* 1 ~ 9 : BATTERY LEVEL */
                    /* #define CAL_BAT_LEVEL(x)    (3500 + (x * 50)) */
                    /*---------------------------------------------------------------------------*/
                    if ((data <= '9') && (data > '0'))
                        BatteryBootVolt = cal_battery_level(data - '0');
                    else
                        BatteryBootVolt = 0;
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
}

/*---------------------------------------------------------------------------*/
void target_system_power (bool onoff)
{
    if (onoff) {
        if (rRESET_KEEP.bits.bGpioStatus) {
            /* Power button signal */
            pinMode (PORT_CTL_POWER, OUTPUT);
            digitalWrite (PORT_CTL_POWER, 0);   delay (TARGET_RESET_DELAY);
            digitalWrite (PORT_CTL_POWER, 1);   delay (TARGET_RESET_DELAY);
            digitalWrite (PORT_CTL_POWER, 0);
            rRESET_KEEP.bits.bGpioStatus = false;
        }
        pinMode (PORT_CTL_POWER, INPUT);    pinMode (PORT_CTL_RESET, INPUT);
    } else {
        /* target system force power off */
        pinMode (PORT_CTL_POWER, OUTPUT);   pinMode (PORT_CTL_RESET, OUTPUT);
        digitalWrite (PORT_CTL_RESET, 1);   delay (TARGET_RESET_DELAY);
        digitalWrite (PORT_CTL_POWER, 1);   delay (TARGET_RESET_DELAY);
    }
    RESET_KEEP = rRESET_KEEP.byte;
    USBSerial_flush ();
}

/*---------------------------------------------------------------------------*/
void power_state_check (enum eBATTERY_STATUS bat_status, unsigned long bat_volt)
{
    bool boot_condition = false;

    if (((bat_volt > BATTERY_LEVEL_OFF)  && (rRESET_KEEP.bits.ePowerState != ePOWER_INIT)) ||
        ((bat_volt > BATTERY_BOOT_LEVEL) && (rRESET_KEEP.bits.ePowerState == ePOWER_INIT))) {
        switch (rRESET_KEEP.bits.ePowerState) {
            case    ePOWER_INIT:
                boot_condition = true;
            case    ePOWER_OFF:
                if (bat_status == eBATTERY_DISCHARGING)
                    rRESET_KEEP.bits.bRestartCondition = true;

                if (rRESET_KEEP.bits.bRestartCondition) {
                    switch (bat_status) {
                        case eBATTERY_CHARGING: case eBATTERY_FULL:
                            boot_condition = true;
                            break;
                    }
                }
                if (boot_condition) {
                    if (bat_volt > BatteryBootVolt) {
                        rRESET_KEEP.bits.ePowerState = ePOWER_ON;
                        rRESET_KEEP.bits.bRestartCondition = false;
                        target_system_power (true);
                        battery_avr_volt_init ();
                        USBSerial_println("@P--ON#");
                    }
                }
                else    rRESET_KEEP.bits.ePowerState = ePOWER_OFF;
                break;
            default :
                break;
        }
    } else {
        if (bat_status != eBATTERY_REMOVED) {
            rRESET_KEEP.bits.ePowerState = ePOWER_INIT;
            rRESET_KEEP.bits.bGpioStatus = true;
            target_system_power (false);
        } else {
            if (rRESET_KEEP.bits.ePowerState == ePOWER_INIT) {
                rRESET_KEEP.bits.ePowerState = ePOWER_ON;
                target_system_power (true);
            }
        }
    }
    RESET_KEEP = rRESET_KEEP.byte;
}

/*---------------------------------------------------------------------------*/
void port_init (void)
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
void setup ()
{
    /* UPS GPIO Port Init */
    port_init();

    /* USB Serial init */
    USBSerial_flush();

    rRESET_KEEP.byte = RESET_KEEP;
    /* PowerOn reset */
    if (rRESET_KEEP.byte == 0) {
        rRESET_KEEP.bits.ePowerState = ePOWER_INIT;
        BatteryBootVolt = 0;
        target_system_power (false);
    }
    RESET_KEEP = rRESET_KEEP.byte;

    /* for target watchdog (P3.2 INT0), Only falling trigger */
    attachInterrupt(0, int0_callback, FALLING);

    repeat_data_clear();

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

    if(MillisLoop + PERIOD_LOOP_MILLIS < millis()) {

        BatteryStatus   = battery_status ();
        BatteryAvrVolt  = battery_avr_volt (BatteryStatus);

        power_state_check    (BatteryStatus, BatteryAvrVolt);
        battery_level_update (BatteryStatus, BatteryAvrVolt);
        repeat_data_check    ();

        /* main loop cycle update */
        MillisLoop = millis ();
        if (rRESET_KEEP.bits.ePowerState == ePOWER_OFF)
            MillisLoop += PEROID_OFF_MILLIS;
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


