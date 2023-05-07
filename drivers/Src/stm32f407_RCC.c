/*
 * stm32f407_RCC.c
 *
 *  Created on: 12-Apr-2022
 *      Author: pro
 */

#include "stm32f407xx_RCC.h"

void RCC_Init(RCC_Handle_t *pRCCHandle) {
    setAHB1lock(pRCCHandle);
    setAPB1Clock(pRCCHandle);
    setAPB2Clock(pRCCHandle);
    changeClockSource(pRCCHandle);
}

void sysTick_Delay(uint32_t delayMS, RCC_Handle_t *pRCCHandle) {
    uint32_t *sysCSR = (uint32_t *)(0xE000E010);
    uint32_t *sysRVR = (uint32_t *)(0xE000E014);
    uint32_t *sysCVR = (uint32_t *)(0xE000E018);
    uint32_t temp = 0;
    uint32_t count = 0;
    *sysCSR |= (1 << 2); // enable processor clock
    temp = getAHBClock(pRCCHandle) / 1000;
    *sysRVR &= ~(0xFFFFFF);
    *sysRVR = temp;
    *sysCSR |= (1 << 0);
    while (count <= delayMS) {
        if ((*sysCSR & (1 << 16))) {
            count++;
            *sysRVR = temp;
        }
    }
}

void setAHB1_flashLatency(RCC_Handle_t *pRCCHanle) {
	uint32_t *Flash_Latency = (uint32_t *)0x40023C00;

        // configure the flash wait state
        int offset = 0;
        int new_clk_freq = pRCCHandle->RCC_Config.AHB_ClockFreq;
        if (new_clk_freq <= 30) {
            offset = 0;
        } else if ((30000000 < new_clk_freq) && (new_clk_freq <= 60000000)) {
            offset = 1;
        } else if ((60000000 < new_clk_freq) && (new_clk_freq <= 90000000)) {
            offset = 2;
        } else if ((90000000 < new_clk_freq) && (new_clk_freq <= 120000000)) {
            offset = 3;
        } else if ((120000000 < new_clk_freq) && (new_clk_freq <= 150000000)) {
            offset = 4;
        } else if ((150000000 < new_clk_freq) && (new_clk_freq <= 168000000)) {
            offset = 5;
        }
        *Flash_Latency &= ~(0x07 << 0);
        *Flash_Latency |= (offset << 0);
}

void setAHB1_clockPLL(RCC_Handle_t *pRCCHanle) {
	// check the current clock source
        if (getClockSource(pRCCHandle) == PLL_CLOCK) {
            // PLL cannot be configured when PLL is ON
            // switch the clock to HSI
            pRCCHandle->RCC_Config.clockSource = HSI_CLOCK;
            changeClockSource(pRCCHandle);
            // turn off PLL
            pRCCHandle->pRCC->CR &= ~(1 << RCC_CR_PLLON);
            pRCCHandle->RCC_Config.clockSource = PLL_CLOCK;
        }

	// Configure the PLL engine
        // max SYSCLK that can be achieved is 168MHZ
	int flag_ready = -1;
	int flag_on = -1;
	int pll_M = -1;
	
	switch (pRCCHandle->RCC_Config.PLLSource) {
	case HSI_CLOCK:
		flag_ready = RCC_CR_HSIRDY;
		flag_on = RCC_CR_HSION;
		pll_M = 0x08;
		break;
	case HSE_CLOCK:
		flag_ready = RCC_CR_HSERDY;
		flag_on = RCC_CR_HSEON;
		pll_M = 0x04;
		break;
	default: break;
	}
	
	pRCCHandle->pRCC->CR |= (1 << flag_on);
	while (!(pRCCHandle->pRCC->CR & (1 << flag_ready))) {}
	// load value 'pll_M' in M
	pRCCHandle->pRCC->PLLCFGR &= ~(0x3F << 0); // clearing the proper bit field bug fix
	pRCCHandle->pRCC->PLLCFGR |= (pll_M << 0);
	// load the frequency value in N
	pRCCHandle->pRCC->PLLCFGR &= ~((0x1FF) << 6);
	pRCCHandle->pRCC->PLLCFGR |= ((pRCCHandle->RCC_Config.AHB_ClockFreq / 1000000) << 6);
	// load the value in P
	pRCCHandle->pRCC->PLLCFGR &= ~(0x03 << 16);
	
        // configure the flash wait state
	setAHB1_flashLatency(pRCCHandle);
}

void setAHB1_clockHSx(RCC_Handle_t *pRCCHanle, uint32_t type) {
	i = 0;
	temp = type / pRCCHandle->RCC_Config.AHB_ClockFreq;
	value = 0;
   	if (temp == 1) { value = 0; }
    	if (temp >= 64) { temp = temp / 2; }
    	if (temp >= 2) {
		value = 8;
		
		uint8_t i = 0;
		for (; temp != 2; i++) {
	    		temp = temp / 2;
		}
		value = value + i;

#if RCC_DEBUG
                printf("Clock stock at : %d running at : %d\n ",
                       type, pRCCHandle->RCC_Config.AHB_ClockFreq);
                printf("AHB1 HPRE value %d\n ", value);
#endif
	}
	pRCCHandle->pRCC->CFGR &= ~(0x0F << 4);
	pRCCHandle->pRCC->CFGR |= (value << 4);
}

void setAHB1lock(RCC_Handle_t *pRCCHandle) {
	switch (pRCCHandle->RCC_Config.clockSource) {
	case HSI_CLOCK: {
#if RCC_DEBUG
		printf("HSI:\n");
#endif
		setAHB1_clockHSx(pRCCHandle, SYSTEM_HSI);
		return;
	}

	case HSE_CLOCK: {
#if RCC_DEBUG
		printf("HSE:\n");
#endif
		setAHB1_clockHSx(pRCCHandle, SYSTEM_HSE);
		return;
	}

	case PLL_CLOCK: {
		setAHB1_clockPLL(pRCCHandle);
		return;
	}

	default: break;
	}
}

void setAPB1Clock(RCC_Handle_t *pRCCHandle) {
    uint32_t temp = 1;
    uint32_t value = 0;
    uint32_t i = 0;
    if (((pRCCHandle->RCC_Config.clockSource == HSI_CLOCK) ||
         (pRCCHandle->RCC_Config.clockSource == HSE_CLOCK)) &&
        (pRCCHandle->RCC_Config.APB1_ClockFreq <= SYSTEM_HSI)) {
        if (getAHBClock(pRCCHandle) < pRCCHandle->RCC_Config.APB1_ClockFreq) {
#if RCC_DEBUG
            printf("ERR : APB1 FREQ : %d not possible AHB1 is : %d\n ",
                   pRCCHandle->RCC_Config.APB1_ClockFreq,
                   getAHBClock(pRCCHandle));
#endif
            return;
        }
        temp = getAHBClock(pRCCHandle) / pRCCHandle->RCC_Config.APB1_ClockFreq;
        if (temp == 1) {
            value = 0;
        }
        if (temp >= 2) {
            value = 4;
            for (; temp != 2; i++) {
                temp = temp / 2;
            }
            value = value + i;

#if RCC_DEBUG
            printf("APB1 PRE1 value %d\n ", value);
#endif
        }
        pRCCHandle->pRCC->CFGR &= ~(0x07 << 10);
        pRCCHandle->pRCC->CFGR |= (value << 10);
        i = 0;
        value = 0;

    } else if (pRCCHandle->RCC_Config.clockSource == PLL_CLOCK) {

        temp = getAHBClock(pRCCHandle);
        i = 1;
        if (pRCCHandle->RCC_Config.APB1_ClockFreq > APB1_MAX_FREQ) {
#if RCC_DEBUG
            printf("APB1 Freq : %d not possible setting it to max possible \n",
                   pRCCHandle->RCC_Config.APB1_ClockFreq);
#endif
            pRCCHandle->RCC_Config.APB1_ClockFreq = APB1_MAX_FREQ;
        }

        while (1) {

            if (temp <= pRCCHandle->RCC_Config.APB1_ClockFreq) {
                // check to see if its under limit the APB1 can handle
                if (temp <= APB1_MAX_FREQ) {
                    // capture the value of temp and break the loop
                    break;
                }
            }
            temp = temp / 2;
            i = i * 2;
        }
#if RCC_DEBUG
        printf("APB1 PRE1 value %d\n ", value);
#endif

        switch (i) {
        case 1: { value = 0; }; break;
        case 2: { value = 4; }; break;
        case 4: { value = 5; }; break;
        case 8: { value = 6; }; break;
        case 16: { value = 7; }; break;
        default: break;
        }
        pRCCHandle->pRCC->CFGR &= ~(0x07 << 10);
        pRCCHandle->pRCC->CFGR |= (value << 10);
    }
}

void setAPB2Clock(RCC_Handle_t *pRCCHandle) {
    uint32_t temp = 1;
    uint32_t value = 0;
    uint8_t i = 0;
    if (((pRCCHandle->RCC_Config.clockSource == HSI_CLOCK) ||
         (pRCCHandle->RCC_Config.clockSource == HSE_CLOCK)) &&
        (pRCCHandle->RCC_Config.APB1_ClockFreq <= SYSTEM_HSI)) {
        if (getAHBClock(pRCCHandle) < pRCCHandle->RCC_Config.APB2_ClockFreq) {
#if RCC_DEBUG
            printf("ERR : APB2 FREQ : %d not possible AHB1 is : %d\n ",
                   pRCCHandle->RCC_Config.APB2_ClockFreq,
                   getAHBClock(pRCCHandle));
#endif
            return;
        }
        temp = getAHBClock(pRCCHandle) / pRCCHandle->RCC_Config.APB2_ClockFreq;
        if (temp == 1) {
            value = 0;
        }
        if (temp >= 2) {
            value = 4;
            for (; temp != 2; i++) {
                temp = temp / 2;
            }
            value = value + i;

#if RCC_DEBUG
            printf("APB2 PRE2 value %d\n ", value);
#endif
        }
        pRCCHandle->pRCC->CFGR &= ~(0x07 << 13);
        pRCCHandle->pRCC->CFGR |= (value << 13);
        i = 0;
        value = 0;
    } else if (pRCCHandle->RCC_Config.clockSource == PLL_CLOCK) {
        temp = getAHBClock(pRCCHandle);
        i = 1;
        if (pRCCHandle->RCC_Config.APB2_ClockFreq > APB2_MAX_FREQ) {
#if RCC_DEBUG
            printf("APB2 Freq : %d not possible setting it to max possible \n",
                   pRCCHandle->RCC_Config.APB2_ClockFreq);
#endif
            pRCCHandle->RCC_Config.APB2_ClockFreq = APB2_MAX_FREQ;
        }

        while (1) {

            if (temp <= pRCCHandle->RCC_Config.APB2_ClockFreq) {
                // check to see if its under limit the APB1 can handle
                if (temp <= APB2_MAX_FREQ) {
                    // capture the value of temp and break the loop
                    break;
                }
            }
            temp = temp / 2;
            i = i * 2;
        }
#if RCC_DEBUG
        printf("APB2 PRE2 value %d\n ", value);
#endif

        if (i == 1) {
            value = 0;
        } else if (i == 2) {
            value = 4;
        } else if (i == 4) {
            value = 5;
        } else if (i == 8) {
            value = 6;
        } else if (i == 16) {
            value = 7;
        }

#if RCC_DEBUG
        printf("APB2 PRE2 value %d\n ", value);
#endif

        pRCCHandle->pRCC->CFGR &= ~(0x07 << 13);
        pRCCHandle->pRCC->CFGR |= (value << 13);
    }
}

void changeClockSource(RCC_Handle_t *pRCCHandle) {

    if (pRCCHandle->RCC_Config.clockSource == HSI_CLOCK) {
        pRCCHandle->pRCC->CR |= (1 << RCC_CR_HSION);
        while (!(pRCCHandle->pRCC->CR & (1 << RCC_CR_HSIRDY)))
            ;
        pRCCHandle->pRCC->CFGR &= ~(3 << 0);

    } else if (pRCCHandle->RCC_Config.clockSource == HSE_CLOCK) {
        pRCCHandle->pRCC->CR |= (1 << RCC_CR_HSEON);
        while (!(pRCCHandle->pRCC->CR & (1 << RCC_CR_HSERDY)))
            ;
        pRCCHandle->pRCC->CFGR |= (1 << 0);

    } else if (pRCCHandle->RCC_Config.clockSource == PLL_CLOCK) {
        pRCCHandle->pRCC->CR |= (1 << RCC_CR_PLLON);
        while (!(pRCCHandle->pRCC->CR & (1 << RCC_CR_PLLRDY)))
            ;
        pRCCHandle->pRCC->CFGR |= (1 << 1);
    }
}

uint16_t getClockSource(RCC_Handle_t *pRCCHandle) {

    if ((pRCCHandle->pRCC->CFGR & 0x0C) == 0x00) {
#if RCC_DEBUG
        printf("CLOCK source HSI: %d \n", (pRCCHandle->pRCC->CFGR & 0x0C) >> 0);
#endif
        return HSI_CLOCK;
    } else if ((pRCCHandle->pRCC->CFGR & 0x0C) == 0x04) {
#if RCC_DEBUG
        printf("CLOCK source HSE: %d \n", (pRCCHandle->pRCC->CFGR & 0x0C) >> 0);
#endif
        return HSE_CLOCK;
    } else if ((pRCCHandle->pRCC->CFGR & 0x0C) == 0x08) {
#if RCC_DEBUG
        printf("CLOCK source PLL: %d \n", (pRCCHandle->pRCC->CFGR & 0x0C) >> 0);
#endif
        return PLL_CLOCK;
    }
    return 0;
}
uint32_t getAHBClock(RCC_Handle_t *pRCCHandle) {

    uint32_t temp = 0;
    uint32_t value = 2;

    if (pRCCHandle->RCC_Config.clockSource == HSI_CLOCK) {
        // default for HSI is 16MHZ
        temp = ((pRCCHandle->pRCC->CFGR & 0xF0) >> 4);
        if (temp >= 12) {
            value = 4; // 8 ,16 ,32,64, 128 , 256
        }
        if (temp > 8) {
            temp = temp - 8;
            for (uint8_t i = 0; i < temp; i++) {
                // for every loop we double the value of 2
                value = value * 2;
            }
        } else if (temp == 8) {
            value = 2;
        } else {
            value = 1;
        }
        return (SYSTEM_HSI / value);
    } else if (pRCCHandle->RCC_Config.clockSource == HSE_CLOCK) {
        // default for HSI is 8MHZ
        temp = ((pRCCHandle->pRCC->CFGR & 0xF0) >> 4);
        temp = temp - 8;
        if (temp > 8) {

            for (uint8_t i = 0; i < temp; i++) {
                // for every loop we double the value of 2
                value = value * 2;
            }
        } else if (temp == 8) {
            value = 2;
        } else {
            value = 1;
        }
        return (SYSTEM_HSE / value);
    } else if (pRCCHandle->RCC_Config.clockSource == PLL_CLOCK) {
        temp = 0;
        value = 0;
        uint32_t temp2 = ((pRCCHandle->pRCC->CFGR & 0xF0) >> 4);
        uint32_t value2 = 2;

        // OPTIMISATION REQUIRED CONSOLIDATE THE IF STATMENT TO ONE IF STATEMENT

        if (pRCCHandle->RCC_Config.PLLSource == HSI_CLOCK) {
            temp = (((SYSTEM_HSI /
                      (pRCCHandle->pRCC->PLLCFGR & 0x3F)) * // divide by M
                     ((pRCCHandle->pRCC->PLLCFGR & 0x7FC0) >> 6))) / // divide N
                   ((((pRCCHandle->pRCC->PLLCFGR & (0x30000)) >> 16) * 2) +
                    2); // divide by P

            if (temp2 >= 12) {
                value2 = 4; // 8 ,16 ,32,64, 128 , 256
            }
            if (temp2 > 8) {
                temp2 = temp2 - 8;
                for (uint8_t i = 0; i < temp2; i++) {
                    // for every loop we double the value of 2
                    value2 = value2 * 2;
                }
            } else if (temp2 == 8) {
                value2 = 2;
            } else {
                value2 = 1;
            }

            value = (temp / value2); // divide by AHB prescaler (HPRE)
            return value;

        } else if (pRCCHandle->RCC_Config.PLLSource == HSE_CLOCK) {
            temp = 0;
            value = 0;

            temp = (((SYSTEM_HSE /
                      (pRCCHandle->pRCC->PLLCFGR & 0x3F)) * // divide by M
                     ((pRCCHandle->pRCC->PLLCFGR & 0x7FC0) >> 6))) / // divide N
                   ((((pRCCHandle->pRCC->PLLCFGR & (0x30000)) >> 16) * 2) +
                    2); // divide by P

            if (temp2 >= 12) {
                value2 = 4; // 8 ,16 ,32,64, 128 , 256
            }
            if (temp2 > 8) {
                temp2 = temp2 - 8;
                for (uint8_t i = 0; i < temp2; i++) {
                    // for every loop we double the value of 2
                    value2 = value2 * 2;
                }
            } else if (temp2 == 8) {
                value2 = 2;
            } else {
                value2 = 1;
            }

            value = (temp / value2); // divide by AHB prescaler (HPRE)
            return value;
        }
    }
    return 0;
}

uint32_t getAPB1Clock(RCC_Handle_t *pRCCHandle) {
    uint16_t temp = 0;
    uint8_t value = 2;

    temp = ((pRCCHandle->pRCC->CFGR & 0x1C00) >> 10);

    if (temp > 4) {
        temp = temp - 4;
        for (uint8_t i = 0; i < temp; i++) {
            // for every loop we double the value of 2
            value = value * 2;
        }
    } else if (temp == 4) {
        value = 2;
    } else {
        value = 1;
    }

    return (getAHBClock(pRCCHandle) / value);
}

uint32_t getAPB2Clock(RCC_Handle_t *pRCCHandle) {

    uint16_t temp = 0;
    uint8_t value = 2;

    temp = ((pRCCHandle->pRCC->CFGR & 0xE000) >> 13);

    if (temp > 4) {
        temp = temp - 4;
        for (uint8_t i = 0; i < temp; i++) {
            // for every loop we double the value of 2
            value = value * 2;
        }
    } else if (temp == 4) {
        value = 2;
    } else {
        value = 1;
    }

    return (getAHBClock(pRCCHandle) / value);
}
