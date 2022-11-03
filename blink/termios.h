#ifndef BLINK_TERMIOS_H_
#define BLINK_TERMIOS_H_
#include <termios.h>

#ifndef TCGETS
#define TCGETS TIOCGETA
#endif
#ifndef TCSETS
#define TCSETS TIOCSETA
#endif
#ifndef TCSETSW
#define TCSETSW TIOCSETAW
#endif
#ifndef TCSETSF
#define TCSETSF TIOCSETAF
#endif

#endif /* BLINK_TERMIOS_H_ */
