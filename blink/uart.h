#ifndef BLINK_UART_H_
#define BLINK_UART_H_

#define UART_DLAB   (1 << 7) /* serial line conf mode bit */
#define UART_DLL    0        /* divisor latch register */
#define UART_DLM    1        /* divisor latch register */
#define UART_IIR    2        /* interrupt identification register */
#define UART_LCR    3        /* line control register */
#define UART_MCR    4        /* modem control register */
#define UART_LSR    5        /* line status register */
#define UART_TTYDA  (1 << 0) /* data available (rx ready) */
#define UART_TTYOE  (1 << 1) /* overrun error */
#define UART_TTYPE  (1 << 2) /* parity error */
#define UART_TTYFE  (1 << 3) /* framing error */
#define UART_TTYBSR (1 << 4) /* break signal received */
#define UART_TTYTXR (1 << 5) /* serial thr empty (tx ready) */
#define UART_TTYIDL (1 << 6) /* serial thr empty and line idle */
#define UART_TTYEDF (1 << 7) /* erroneous data in fifo */

#endif /* BLINK_UART_H_ */
