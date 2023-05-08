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

static void rcc_wait_ready_flag(const RCC_Handle_t *__restrict pRCCHandle, uint32_t flag) {
	while (!(pRCCHandle->pRCC->CR & (1 << flag))) {};
}

static void rcc_on(const RCC_Handle_t *__restrict pRCCHandle, uint32_t dev, uint32_t flag) {
    pRCCHandle->pRCC->CR |= (1 << dev);
    rcc_wait_ready_flag(pRCCHandle, flag);
}

static void setAHB1_flashLatency(RCC_Handle_t *__restrict pRCCHandle) {
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

void setAHB1_lockPLL(RCC_Handle_t *__restrict pRCCHandle) {
	// check the current clock source
    if (PLL_CLOCK == getClockSource(pRCCHandle)) {
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
	uint32_t flag_ready = 0;
	uint32_t flag_on = 0;
	uint32_t pll_M = 0;
	
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
	default:
        // configure the flash wait state
	    setAHB1_flashLatency(pRCCHandle);
        return;
	}
	
    rcc_on(flag_on, flag_ready);

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

static void setAHB1_lockHSx(const RCC_Handle_t *__restrict pRCCHandle, uint32_t type) {
    uint32_t temp = type / pRCCHandle->RCC_Config.AHB_ClockFreq;
    uint32_t value = 0;
    
   	if (temp == 1) { 
        value = 0; 
    }
    if (temp >= 64) { 
        temp = temp / 2; 
    }
    if (temp >= 2) {
		value = 8;
		
		uint8_t i = 0;
		for (; temp != 2; i++) {
	    		temp = temp / 2;
		}
		value = value + i;

#if RCC_DEBUG
        printf("Clock stock at : %d running at : %d\n ", type, pRCCHandle->RCC_Config.AHB_ClockFreq);
        printf("AHB1 HPRE value %d\n ", value);
#endif
	}
	pRCCHandle->pRCC->CFGR &= ~(0x0F << 4);
	pRCCHandle->pRCC->CFGR |= (value << 4);
}

void setAHB1lock(RCC_Handle_t *__restrict pRCCHandle) {
	switch (pRCCHandle->RCC_Config.clockSource) {
	case HSI_CLOCK: {
#if RCC_DEBUG
		printf("HSI:\n");
#endif
		setAHB1_lockHSx(pRCCHandle, SYSTEM_HSI);
		return;
	}

	case HSE_CLOCK: {
#if RCC_DEBUG
		printf("HSE:\n");
#endif
		setAHB1_lockHSx(pRCCHandle, SYSTEM_HSE);
		return;
	}

	case PLL_CLOCK: {
		setAHB1_lockPLL(pRCCHandle);
		return;
	}

	default: break;
	}
}

static void setAPB1Clock_PLL(RCC_Handle_t *__restrict pRCCHandle) {
        uint32_t temp = getAHBClock(pRCCHandle);
        uint32_t value = 0;
        uint32_t i = 1;
        if (pRCCHandle->RCC_Config.APB1_ClockFreq > APB1_MAX_FREQ) {
#if RCC_DEBUG
            printf("APB1 Freq : %d not possible setting it to max possible \n", pRCCHandle->RCC_Config.APB1_ClockFreq);
#endif
            pRCCHandle->RCC_Config.APB1_ClockFreq = APB1_MAX_FREQ;
        }

        while (1) {
            if (temp <= pRCCHandle->RCC_Config.APB1_ClockFreq) {
                // check to see if its under limit the APB1 can handle
                if (temp <= APB1_MAX_FREQ) {
                    break; // capture the value of temp and break the loop
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

void setAPB1Clock(RCC_Handle_t *pRCCHandle) {
    uint32_t temp = 1;
    uint32_t value = 0;
    uint32_t i = 0;

    uint8_t clockSource = pRCCHandle->RCC_Config.clockSource;
    if (PLL_CLOCK == clockSource) {
        setAPB1Clock_PLL(pRCCHandle);
        return;
    }

    if (((HSI_CLOCK == clockSource) || (HSE_CLOCK == clockSource)) {
        if (pRCCHandle->RCC_Config.APB1_ClockFreq > SYSTEM_HSI) {
            return;
        }

        if (getAHBClock(pRCCHandle) < pRCCHandle->RCC_Config.APB1_ClockFreq) {
#if RCC_DEBUG
            printf("ERR : APB1 FREQ : %d not possible AHB1 is : %d\n ", pRCCHandle->RCC_Config.APB1_ClockFreq, getAHBClock(pRCCHandle));
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
	switch (i) {
        case 1: { value = 0; }; break;
        case 2: { value = 4; }; break;
        case 4: { value = 5; }; break;
        case 8: { value = 6; }; break;
        case 16: { value = 7; }; break;
        default: break;
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
        rcc_on(pRCCHandle, RCC_CR_HSION, RCC_CR_HSIRDY);
        pRCCHandle->pRCC->CFGR &= ~(3 << 0);

    } else if (pRCCHandle->RCC_Config.clockSource == HSE_CLOCK) {
        pRCCHandle->pRCC->CR |= (1 << RCC_CR_HSEON);
        rcc_on(pRCCHandle, RCC_CR_HSEON, RCC_CR_HSERDY);
        pRCCHandle->pRCC->CFGR |= (1 << 0);

    } else if (pRCCHandle->RCC_Config.clockSource == PLL_CLOCK) {
        pRCCHandle->pRCC->CR |= (1 << RCC_CR_PLLON);
       	rcc_on(pRCCHandle, RCC_CR_PLLON, RCC_CR_PLLRDY);
        pRCCHandle->pRCC->CFGR |= (1 << 1);
    }
}

uint16_t getClockSource(const RCC_Handle_t *__restrict pRCCHandle) {
    uint32_t source = (pRCCHandle->pRCC->CFGR & 0x0C;
    uint16_t output = 0;

    switch (source) {
        case 0x00: output = HSI_CLOCK; break;
        case 0x04: output = HSE_CLOCK; break;
        case 0x08: output = PLL_CLOCK; break;
        default: break;
    }

#if RCC_DEBUG
    char *souce_name;
    switch (output) {
        case HSI_CLOCK: souce_name = "HSI"; break;
        case HSE_CLOCK: souce_name = "HSE"; break;
        case PLL_CLOCK: souce_name = "PLL"; break;
        default: break;
    }

    printf("CLOCK source %s: %d \n", source_name, source >> 0);
#endif

    return output;
}

static uint32_t getAHBClock_PLL(const RCC_Handle_t *__restrict pRCCHandle, uint8_t clockSouce) {
    uint32_t value  = 2;
    uint32_t value2 = 2;

    uint32_t temp = (((clockSouce / 
                        (pRCCHandle->pRCC->PLLCFGR & 0x3F))                       // divide by M
                   *   ((pRCCHandle->pRCC->PLLCFGR & 0x7FC0) >> 6)))              // divide N
                   / ((((pRCCHandle->pRCC->PLLCFGR & (0x30000)) >> 16) * 2) + 2); // divide by P

    uint32_t temp2  = ((pRCCHandle->pRCC->CFGR & 0xF0) >> 4);

    if (temp2 >= 12) {
        value2 = 4; // 8, 16, 32, 64, 128, 256
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

static uint32_t getAHBClock_HSx(const RCC_Handle_t *__restrict pRCCHandle, uint8_t clockSouce) {

}

uint32_t getAHBClock(const RCC_Handle_t *__restrict pRCCHandle) {
    uint32_t temp = 0;
    uint32_t value = 2;

    if (HSI_CLOCK == pRCCHandle->RCC_Config.clockSource) {
        // default for HSI is 16MHZ
        temp = ((pRCCHandle->pRCC->CFGR & 0xF0) >> 4);
        if (temp >= 12) {
            value = 4; // 8, 16, 32, 64, 128, 256
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
    } else if (HSE_CLOCK == pRCCHandle->RCC_Config.clockSource) {
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
    } else if (PLL_CLOCK == pRCCHandle->RCC_Config.clockSource) {
        uint32_t clock = 0;

        if (HSI_CLOCK == pRCCHandle->RCC_Config.PLLSource) {
            clock = SYSTEM_HSI;
        } else if (HSE_CLOCK == pRCCHandle->RCC_Config.PLLSource) {
            clock = SYSTEM_HSE;
        } else {
            return 0;
        }
        return getAHBClock_PLL(pRCCHandle, clock);
    }
}

static uint32_t getAPBxClock(const RCC_Handle_t *__restrict pRCCHandle, uint16_t offset, uint16_t shift) {
    uint16_t temp = ((pRCCHandle->pRCC->CFGR & offset) >> shift);
    uint8_t value = 2;

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

uint32_t getAPB1Clock(const RCC_Handle_t *__restrict pRCCHandle) {
    return getAPBxClock(pRCCHandle, 0x1C00, 10);
}

uint32_t getAPB2Clock(const RCC_Handle_t *__restrict pRCCHandle) {
    return getAPBxClock(pRCCHandle, 0xE000, 13);
}
