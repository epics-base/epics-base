/* share/src/libCom/post.h  $Id$ */
/*
 *      Author: Bob Dalesio
 *      Date:   9-21-88
 *      @(#)post.h @(#)post.h   1.1     12/6/90
 *
 *      Control System Software for the GTA Project
 *
 *      Copyright 1988, 1989, the Regents of the University of California.
 *
 *      This software was produced under a U.S. Government contract
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *      operated by the University of California for the U.S. Department
 *      of Energy.
 *
 *      Developed by the Controls and Automation Group (AT-8)
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *      Direct inqueries to:
 *      Andy Kozubal, AT-8, Mail Stop H820
 *      Los Alamos National Laboratory
 *      Los Alamos, New Mexico 87545
 *      Phone: (505) 667-6508
 *      E-mail: kozubal@k2.lanl.gov
 *
 * Modifcation Log
 * ---------------
 * .01  01-11-89        lrd     add right and left shift
 * .02  02-01-89        lrd     add trig functions
 */

/*	defines for element table      */
#define 	FETCH_A		0
#define		FETCH_B		1
#define		FETCH_C		2
#define		FETCH_D		3
#define		FETCH_E		4
#define		FETCH_F		5
#define		FETCH_G		6
#define		FETCH_H		7
#define		FETCH_I		8
#define		FETCH_J		9
#define		FETCH_K		10
#define		FETCH_L		11
#define		ACOS		12
#define		ASIN		13
#define		ATAN		14
#define		COS		15
#define		COSH		16
#define		SIN		17
#define		STORE_A		18
#define		STORE_B		19
#define		STORE_C		20
#define		STORE_D		21
#define		STORE_E		22
#define		STORE_F		23
#define		STORE_G		24
#define		STORE_H		25
#define		STORE_I		26
#define		STORE_J		27
#define		STORE_K		28
#define		STORE_L		29
#define		RIGHT_SHIFT	30
#define		LEFT_SHIFT	31
#define		SINH		32
#define		TAN		33
#define		TANH		34
#define		LOG_2		35
#define		COND_ELSE	36
#define		ABS_VAL		37
#define		UNARY_NEG	38
#define		SQU_RT		39
#define		LOG_10		40
#define		LOG_E		41
#define		RANDOM		42
#define		ADD		43
#define		SUB		44
#define		MULT		45
#define		DIV		46
#define		EXPON		47
#define		MODULO		48
#define		BIT_OR		49
#define		BIT_AND		50
#define		BIT_EXCL_OR	51
#define		GR_OR_EQ	52
#define		GR_THAN		53
#define		LESS_OR_EQ	54
#define		LESS_THAN	55
#define		NOT_EQ		56
#define		EQUAL		57
#define		REL_OR		58
#define		REL_AND		59
#define		REL_NOT		60
#define		BIT_NOT		61
#define		PAREN		62
#define		END_STACK	-1
