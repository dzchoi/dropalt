#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Data structure to define Shift Register output expander hardware */
/* This structure gets shifted into registers LSB first */
typedef union {
    struct {
        uint16_t HUB_CONNECT: 1;  /*!< bit: 0  SIGNAL VBUS CONNECT TO USB HUB WHEN 1 */
        uint16_t HUB_RESET_N: 1;  /*!< bit: 1  RESET USB HUB WHEN 0, RUN WHEN 1 */
        uint16_t S_UP: 1;         /*!< bit: 2  SELECT UP PATH 0:USBC-1, 1:USBC-2 */
        uint16_t E_UP_N: 1;       /*!< bit: 3  ENABLE UP 1:2 MUX WHEN 0 */
        uint16_t S_DN1: 1;        /*!< bit: 4  SELECT DN1 PATH 0:USBC-1, 1:USBC-2 */
        uint16_t E_DN1_N: 1;      /*!< bit: 5  ENABLE DN1 1:2 MUX WHEN 0 */
        uint16_t E_VBUS_1: 1;     /*!< bit: 6  ENABLE 5V OUT TO USBC-1 WHEN 1 */
        uint16_t E_VBUS_2: 1;     /*!< bit: 7  ENABLE 5V OUT TO USBC-2 WHEN 1 */
        uint16_t SRC_1: 1;        /*!< bit: 8  ADVERTISE A SOURCE TO USBC-1 CC */
        uint16_t SRC_2: 1;        /*!< bit: 9  ADVERTISE A SOURCE TO USBC-2 CC */
        uint16_t IRST: 1;         /*!< bit: 10 RESET THE IS3733 I2C WHEN 1, RUN WHEN 0 */
        uint16_t SDB_N: 1;        /*!< bit: 11 SHUTDOWN THE CHIP WHEN 0, RUN WHEN 1 */
        uint16_t : 1;             /*!< bit: 12 */
        uint16_t : 1;             /*!< bit: 13 */
        uint16_t : 1;             /*!< bit: 14 */
        uint16_t : 1;             /*!< bit: 15 */
    } bit;                        /*!< Structure used for bit access */

    struct {
        uint8_t low: 8;           // bits 7-0
        uint8_t high: 8;          // bits 15-8
    };

    uint16_t reg;                 /*!< Type used for register access */
} sr_exp_t;

#define SR_CTRL_HUB_CONNECT     0x0001u
#define SR_CTRL_HUB_RESET_N     0x0002u
#define SR_CTRL_S_UP            0x0004u
#define SR_CTRL_E_UP_N          0x0008u
#define SR_CTRL_S_DN1           0x0010u
#define SR_CTRL_E_DN1_N         0x0020u
#define SR_CTRL_E_VBUS_1        0x0040u
#define SR_CTRL_E_VBUS_2        0x0080u
#define SR_CTRL_SRC_1           0x0100u
#define SR_CTRL_SRC_2           0x0200u
#define SR_CTRL_IRST            0x0400u
#define SR_CTRL_SDB_N           0x0800u

extern sr_exp_t sr_exp_data;

void sr_exp_init(void);
void sr_exp_writedata(uint16_t set_bits, uint16_t clear_bits);

#ifdef __cplusplus
}
#endif
