/*
 *  Copyright (C) 2014 -2016  Espressif System
 *
 */

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "uart.h"

enum {
    UART_EVENT_RX_CHAR,
    UART_EVENT_MAX
};

typedef struct _os_event_ {
    uint32 event;
    uint32 param;
} os_event_t;

xTaskHandle xUartTaskHandle;
xQueueHandle xQueueUart;

LOCAL STATUS
uart_tx_one_char(uint8 uart, uint8 TxChar)
{
    uint32 fifo_cnt;
    do {
        fifo_cnt = READ_PERI_REG(UART_STATUS(uart)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);

    } while(((fifo_cnt >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT) >= 126);

    WRITE_PERI_REG(UART_FIFO(uart), TxChar);
    return OK;
}

void uart_write_char(uint8 uart, char c) {
    if (c == '\n') {
        uart_tx_one_char(uart, '\r');
        uart_tx_one_char(uart, '\n');
    } else if (c != '\r') {
        uart_tx_one_char(uart, c);
    }
}

void uart0_write_char(char c)
{
    uart_write_char(UART0, c);
}

void uart1_write_char(char c)
{
    uart_write_char(UART1, c);
}

void ICACHE_FLASH_ATTR
UART_SetWordLength(UART_Port uart_no, UART_WordLength len)
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no), UART_BIT_NUM, len, UART_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetStopBits(UART_Port uart_no, UART_StopBits bit_num)
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no), UART_STOP_BIT_NUM, bit_num, UART_STOP_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetLineInverse(UART_Port uart_no, UART_LineLevelInverse inverse_mask)
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_LINE_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0(uart_no), inverse_mask);
}

void ICACHE_FLASH_ATTR
UART_SetParity(UART_Port uart_no, UART_ParityMode Parity_mode)
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_PARITY | UART_PARITY_EN);

    if (Parity_mode != USART_Parity_None) {
        SET_PERI_REG_MASK(UART_CONF0(uart_no), Parity_mode | UART_PARITY_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_SetBaudrate(UART_Port uart_no, uint32 baud_rate)
{
    uart_div_modify(uart_no, UART_CLK_FREQ / baud_rate);
}

//only when USART_HardwareFlowControl_RTS is set , will the rx_thresh value be set.
void ICACHE_FLASH_ATTR
UART_SetFlowCtrl(UART_Port uart_no, UART_HwFlowCtrl flow_ctrl, uint8 rx_thresh)
{
    if (flow_ctrl & USART_HardwareFlowControl_RTS) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
        SET_PERI_REG_BITS(UART_CONF1(uart_no), UART_RX_FLOW_THRHD, rx_thresh, UART_RX_FLOW_THRHD_S);
        SET_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    } else {
        CLEAR_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }

    if (flow_ctrl & USART_HardwareFlowControl_CTS) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);
        SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    } else {
        CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_WaitTxFifoEmpty(UART_Port uart_no) //do not use if tx flow control enabled
{
    while (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S));
}

void ICACHE_FLASH_ATTR
UART_ResetFifo(UART_Port uart_no)
{
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
}

void ICACHE_FLASH_ATTR
UART_ClearIntrStatus(UART_Port uart_no, uint32 clr_mask)
{
    WRITE_PERI_REG(UART_INT_CLR(uart_no), clr_mask);
}

void ICACHE_FLASH_ATTR
UART_SetIntrEna(UART_Port uart_no, uint32 ena_mask)
{
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), ena_mask);
}

void ICACHE_FLASH_ATTR
UART_intr_handler_register(void *fn)
{
    _xt_isr_attach(ETS_UART_INUM, fn);
}

void ICACHE_FLASH_ATTR
UART_SetPrintPort(UART_Port uart_no)
{
    if (uart_no == 1) {
        os_install_putc1(uart1_write_char);
    } else {
        os_install_putc1(uart0_write_char);
    }
}

void ICACHE_FLASH_ATTR
UART_ParamConfig(UART_Port uart_no,  UART_ConfigTypeDef *pUARTConfig)
{
    if (uart_no == UART1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    } else {
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
    }

    UART_SetFlowCtrl(uart_no, pUARTConfig->flow_ctrl, pUARTConfig->UART_RxFlowThresh);
    UART_SetBaudrate(uart_no, pUARTConfig->baud_rate);

    WRITE_PERI_REG(UART_CONF0(uart_no),
            ((pUARTConfig->parity == USART_Parity_None) ? 0x0 : (UART_PARITY_EN | pUARTConfig->parity))
            | (pUARTConfig->stop_bits << UART_STOP_BIT_NUM_S)
            | (pUARTConfig->data_bits << UART_BIT_NUM_S)
            | ((pUARTConfig->flow_ctrl & USART_HardwareFlowControl_CTS) ? UART_TX_FLOW_EN : 0x0)
            | pUARTConfig->UART_InverseMask);

    UART_ResetFifo(uart_no);
}

void ICACHE_FLASH_ATTR
UART_IntrConfig(UART_Port uart_no,  UART_IntrConfTypeDef *pUARTIntrConf)
{

    uint32 reg_val = 0;
    UART_ClearIntrStatus(uart_no, UART_INTR_MASK);
    reg_val = READ_PERI_REG(UART_CONF1(uart_no)) & ~((UART_RX_FLOW_THRHD << UART_RX_FLOW_THRHD_S) | UART_RX_FLOW_EN) ;

    reg_val |= ((pUARTIntrConf->UART_IntrEnMask & UART_RXFIFO_TOUT_INT_ENA) ?
            ((((pUARTIntrConf->UART_RX_TimeOutIntrThresh)&UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S) | UART_RX_TOUT_EN) : 0);

    reg_val |= ((pUARTIntrConf->UART_IntrEnMask & UART_RXFIFO_FULL_INT_ENA) ?
            (((pUARTIntrConf->UART_RX_FifoFullIntrThresh)&UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) : 0);

    reg_val |= ((pUARTIntrConf->UART_IntrEnMask & UART_TXFIFO_EMPTY_INT_ENA) ?
            (((pUARTIntrConf->UART_TX_FifoEmptyIntrThresh)&UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S) : 0);

    WRITE_PERI_REG(UART_CONF1(uart_no), reg_val);
    CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_INTR_MASK);
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), pUARTIntrConf->UART_IntrEnMask);
}

LOCAL void
uart0_rx_intr_handler(void *para)
{
    /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    * uart1 and uart0 respectively
    */
    (void)para;
    uint8 uart_no = UART0;//UartDev.buff_uart_no;
    uint8 fifo_len = 0;
    uint8 buf_idx = 0;

    uint32 uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no)) ;

    while (uart_intr_status != 0x0) {
        if (UART_FRM_ERR_INT_ST == (uart_intr_status & UART_FRM_ERR_INT_ST)) {
            //printf("FRM_ERR\r\n");
            WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
        } else if (UART_RXFIFO_FULL_INT_ST == (uart_intr_status & UART_RXFIFO_FULL_INT_ST)) {
            printf("full\r\n");
            fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
            buf_idx = 0;

            while (buf_idx < fifo_len) {
                uart_tx_one_char(UART0, READ_PERI_REG(UART_FIFO(UART0)) & 0xFF);
                buf_idx++;
            }

            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
        } else if (UART_RXFIFO_TOUT_INT_ST == (uart_intr_status & UART_RXFIFO_TOUT_INT_ST)) {
            printf("tout\r\n");
            fifo_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
            buf_idx = 0;

            while (buf_idx < fifo_len) {
                uart_tx_one_char(UART0, READ_PERI_REG(UART_FIFO(UART0)) & 0xFF);
                buf_idx++;
            }

            WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
        } else if (UART_TXFIFO_EMPTY_INT_ST == (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST)) {
            printf("empty\n\r");
            WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
            CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
        } else {
            //skip
        }

        uart_intr_status = READ_PERI_REG(UART_INT_ST(uart_no)) ;
    }
}

void ICACHE_FLASH_ATTR
uart_init_new(void)
{
    UART_WaitTxFifoEmpty(UART0);
    UART_WaitTxFifoEmpty(UART1);

    UART_ConfigTypeDef uart_config;
    uart_config.baud_rate           = BIT_RATE_74880;
    uart_config.data_bits           = UART_WordLength_8b;
    uart_config.parity              = USART_Parity_None;
    uart_config.stop_bits           = USART_StopBits_1;
    uart_config.flow_ctrl           = USART_HardwareFlowControl_None;
    uart_config.UART_RxFlowThresh   = 120;
    uart_config.UART_InverseMask    = UART_None_Inverse;
    UART_ParamConfig(UART0, &uart_config);

    UART_IntrConfTypeDef uart_intr;
    uart_intr.UART_IntrEnMask = UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA;
    uart_intr.UART_RX_FifoFullIntrThresh = 10;
    uart_intr.UART_RX_TimeOutIntrThresh = 2;
    uart_intr.UART_TX_FifoEmptyIntrThresh = 20;
    UART_IntrConfig(UART0, &uart_intr);

    UART_SetPrintPort(UART0);
    UART_intr_handler_register(uart0_rx_intr_handler);
    ETS_UART_INTR_ENABLE();
}
