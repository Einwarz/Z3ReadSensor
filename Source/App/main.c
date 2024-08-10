/*
 * main.c
 *
 *  Created on: Aug 1, 2024
 *      Author: Duy Anh
 */

#include <stdio.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_i2c.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "Source/Mid/button/button.h"
#include "af.h"
#include "hal/plugin/temperature/temperature.h"
#include "em_iadc.h"
#include "i2cspm.h"

#define CLK_SRC_ADC_FREQ 10000000
#define CLK_ADC_FREQ 10000000

#define IADC_INPUT_0_PORT_PIN     iadcPosInputPortCPin5;

#define IADC_INPUT_0_BUS          CDBUSALLOC
#define IADC_INPUT_0_BUSALLOC     GPIO_CDBUSALLOC_CDODD0_ADC0

static volatile IADC_Result_t sample;

static volatile double singleResult;

static uint32_t upperBoundValue = 0xFFF;
static uint32_t lowerBoundValue = 0x330;

void buttonCallbackHandler(uint8_t button, BtnState state);

void initADC(){
	emberAfCorePrintln("Iint ADC");
	IADC_Init_t init = IADC_INIT_DEFAULT;
	IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
	IADC_InitSingle_t initSingle = IADC_INITSINGLE_DEFAULT;
	IADC_SingleInput_t singleInput = IADC_SINGLEINPUT_DEFAULT;
	IADC_reset(IADC0);

	init.warmup = iadcWarmupNormal;
	CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);

	init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

	initAllConfigs.configs[0].reference = iadcCfgReferenceInt1V2;
	initAllConfigs.configs[0].vRef = 1210;
	initAllConfigs.configs[0].osrHighSpeed = iadcCfgOsrHighSpeed2x;
	initAllConfigs.configs[0].analogGain = iadcCfgAnalogGain0P5x;
	initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
	                                                                     CLK_ADC_FREQ,
	                                                                     0,
	                                                                     iadcCfgModeNormal,
	                                                                     init.srcClkPrescale);
	singleInput.posInput   = IADC_INPUT_0_PORT_PIN;
	singleInput.negInput   = iadcNegInputGnd;
	GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;

	IADC_init(IADC0, &init, &initAllConfigs);
	IADC_initSingle(IADC0, &initSingle, &singleInput);
	IADC_clearInt(IADC0, _IADC_IF_MASK);
	IADC_enableInt(IADC0, IADC_IEN_SINGLEDONE);
	NVIC_ClearPendingIRQ(IADC_IRQn);
	NVIC_EnableIRQ(IADC_IRQn);
	emberAfCorePrintln("Init ADC Success");
}


void initCMU(void)
{
  // Enabling clock to the I2C, GPIO, LE
  CMU_ClockEnable(cmuClock_I2C0, true);
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_IADC0, true);
  // Starting LFXO and waiting until it is stable
  CMU_OscillatorEnable(cmuOsc_LFXO, true, true);
}
void emberAfMainInitCallback(void)
{

	I2CSPM_Init_TypeDef i2cInit = I2CSPM_INIT_SENSOR;
	I2CSPM_Init(&i2cInit);
	halTemperatureInit();
	CHIP_Init();
	initCMU();
	initADC();
	buttonInit(buttonCallbackHandler);

}
void SensorMesuaringCmd(){
	emberAfCorePrintln("Start Reading");
	halTemperatureInit();
	emberAfCorePrintln("1 Done");
	halTemperatureStartRead();
	emberAfCorePrintln("2 Done");
}
void LDR_ADCDataRead(){
	IADC_command(IADC0, iadcCmdStartSingle);
}
void IADC_IRQHandler(void)
{
	emberAfCorePrintln("ADC Interupt Start");
	sample = IADC_pullSingleFifoResult(IADC0);
	singleResult = sample.data * 2.42 / 0xFFF;
	uint32_t lightValuePercentage = ((sample.data - lowerBoundValue)* 100)/(upperBoundValue - lowerBoundValue) ;
	IADC_clearInt(IADC0, IADC_IF_SINGLEDONE);
	emberAfCorePrintln("ADC DATA: %d" , lightValuePercentage);
}

void buttonCallbackHandler(uint8_t button, BtnState state){
	if(button == BUTTON0)
	{
			switch (state){
			case release:
				emberAfCorePrintln("pressed TEMP");
				SensorMesuaringCmd();
				break;
			default:
				break;
			}
		}
	if(button == BUTTON1)
		{
				switch (state){
				case release:
					emberAfCorePrintln("pressed ADC");
					LDR_ADCDataRead();
					break;
				default:
					break;
				}
			}
}
void halTemperatureReadingCompleteCallback(int32_t temperature,bool readSuccess){
	if(readSuccess)
	{
		emberAfCorePrintln("Temp = %d",temperature/1000);
	}
	else{
		emberAfCorePrintln("Fat Beanos fail");
	}
}

void halHumidityReadingCompleteCallback(uint16_t humidity, bool readSuccess){
	if(readSuccess)
		{
			emberAfCorePrintln("Humidity = 0x%X",humidity);
		}
		else{
			emberAfCorePrintln("Fat Beanos fail");
		}


}
