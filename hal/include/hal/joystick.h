#ifndef JOYSTICK_H
#define JOYSTICK_H

typedef enum {
    JOYSTICK_NONE,
    JOYSTICK_UP,
    JOYSTICK_DOWN
} Joystick_direction;
Joystick_direction Joystick_getDirection(void);

#endif // JOYSTICK_H