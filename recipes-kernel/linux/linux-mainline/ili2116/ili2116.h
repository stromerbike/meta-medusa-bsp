#ifndef _ILI2116_H
#define _ILI2116_H

/* struct ili2116_platform_data { */
struct ili2116_platform_data {
	unsigned long irq_flags;
	unsigned int poll_period;
	bool (*get_pendown_state)(void);
};

#endif
