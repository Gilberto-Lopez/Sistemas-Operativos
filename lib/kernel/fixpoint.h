#ifndef THREADS_FIX_POINT_H
#define THREADS_FIX_POINT_H

#include <stdint.h>

#define FRACT_BITS 14 // Número de bits reservados para la parte decimal
#define F_CONST 16384 // Equivale a 2**14

/* Convierte un entero en un número equivalente en representación de punto fijo 17.14 */
#define INT_TO_FIXPOINT(p, q)   (((p)<<FRACT_BITS) / (q))

/* Convierte un número en representación de punto fijo 17.14 a un entero truncando su parte decimal */
#define FIXPOINT_TO_INT(x)      ((x) / F_CONST)

/* Multiplica dos números en representación de punto fijo 17.14 */
#define MULT_FP(x, y)        ((((int64_t) x) * (y)) / F_CONST)

/* Divide dos números en representación de punto fijo 17.14*/
#define DIV_FP(x, y)         ( ( ((int64_t) x) * F_CONST ) / (y) )

#endif
