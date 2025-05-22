/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file postfix.h
 * \author Bob Dalesio, Andrew Johnson
 *
 * \brief The API for the EPICS Calculation Engine
 *
 * Defines macros and the routines provided by the calculation engine for
 * subsystems that need to evaluate mathematical expressions.
 */

#ifndef INCpostfixh
#define INCpostfixh

#include "libComAPI.h"

/** \brief Number of input arguments to a calc expression (A-U)
  *
  * Since UNRELEASED the number of inputs has been increased from 12 to 21.
  */
#define CALCPERFORM_NARGS 21
/** \brief Size of the internal partial result stack */
#define CALCPERFORM_STACK 80

/**
 * \name Postfix and Infix Buffer Sizes
 * @{
 */
/**
 * \brief Calculate required size of postfix buffer from infix
 *
 * This macro calculates the maximum size of postfix buffer needed for an
 * infix expression buffer of a given size. The argument \c n must count
 * the trailing nil byte in the input expression string. The actual size
 * needed is never larger than this value, although it is actually a
 * few bytes smaller for some sizes.
 *
 * The maximum expansion from infix to postfix is for the sub-expression
 * <tt>.1?.1:</tt> which is 6 characters long and results in 21 bytes of
 * postfix:
\verbatim
        .1 => LITERAL_DOUBLE + 8 byte value
        ?  => COND_IF
        .1 => LITERAL_DOUBLE + 8 byte value
        :  => COND_ELSE
        ...
           => COND_END
\endverbatim
 * For other short expressions the factor 21/6 always gives a big enough
 * postfix buffer (proven by hand, look at \c 1+ and <tt>.1+</tt> as well).
 */
#define INFIX_TO_POSTFIX_SIZE(n) ((n)*21/6)

/**
 * \brief Size of a "standard" infix string.
 *
 * This is not a hard limit, just the default size for the database
 */
#define MAX_INFIX_SIZE 160
/**
 * \brief Size of a "standard" postfix buffer.
 *
 * This is not a hard limit, just the default size for the database
 */
#define MAX_POSTFIX_SIZE INFIX_TO_POSTFIX_SIZE(MAX_INFIX_SIZE)

/** @} */

/** \name Calc Engine Error Codes
* \note Changes in these errors must also be made in calcErrorStr().
 * @{
 */

/** \brief No error */
#define CALC_ERR_NONE            0
/** \brief Too many results returned */
#define CALC_ERR_TOOMANY         1
/** \brief  Bad numeric literal */
#define CALC_ERR_BAD_LITERAL     2
/** \brief Bad assignment target */
#define CALC_ERR_BAD_ASSIGNMENT  3
/** \brief Comma without parentheses */
#define CALC_ERR_BAD_SEPERATOR   4
/** \brief Close parenthesis without open */
#define CALC_ERR_PAREN_NOT_OPEN  5
/** \brief Open parenthesis at end of expression */
#define CALC_ERR_PAREN_OPEN      6
/** \brief Unbalanced conditional ?: operators */
#define CALC_ERR_CONDITIONAL     7
/** \brief Incomplete expression, operand missing */
#define CALC_ERR_INCOMPLETE      8
/** \brief Runtime stack would underflow */
#define CALC_ERR_UNDERFLOW       9
/** \brief Runtime stack would overflow */
#define CALC_ERR_OVERFLOW       10
/** \brief Syntax error */
#define CALC_ERR_SYNTAX         11
/** \brief NULL or empty input argument */
#define CALC_ERR_NULL_ARG       12
/** \brief Internal error, bad element type */
#define CALC_ERR_INTERNAL       13

/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Compile an infix expression into postfix byte-code
 *
 * Converts an expression from an infix string to postfix byte-code.
 *
 * \param pinfix Pointer to the infix string
 * \param ppostfix Pointer to the postfix buffer
 * \param perror Place to return an error code
 * \return Non-zero value in event of error
 *
 * It is the caller's responsibility to ensure that \p ppostfix points to
 * sufficient storage to hold the postfix expression.
 * The macro INFIX_TO_POSTFIX_SIZE(n) can be used to calculate an
 * appropriate postfix buffer size from the length of the infix buffer.
 * The macro's parameter \p n must count the terminating nil byte too.
 *
 * -# The **infix expressions** that can be used are very similar to the
 *  C expression syntax, but with some additions and subtle differences in
 *  operator meaning and precedence.
 *  The expression string may contain a series of expressions separated by
 *  a semi-colon character <tt>;</tt> any one of which may actually provide
 *  the calculation result.
 *  However all of the other expressions included must assign their result
 *  to a variable.
 *  All alphabetic elements described below are case independent, so upper
 *  and lower case letters may be used and mixed in the variable and
 *  function names as desired.
 *  Spaces may be used anywhere within an expression except between the
 *  characters that make up a single expression element.

 * -# The simplest expression element is a **numeric literal,** any
 *  (positive) number expressed using the standard floating point syntax
 *  that can be stored as a double precision value.
 *  This now includes the values Infinity and NaN (not a number).
 *  Note that negative numbers will be encoded as a positive literal, to
 *  which the unary negate operator is applied.
 *
 *  Examples:
 *    - \c 1
 *    - \c 2.718281828459
 *    - \c Inf
 *
 * -# There are three **trigonometric constants** available to any expression
 *  which return a value:
 *    - \c pi returns the value of the mathematical constant pi.
 *    - \c D2R evaluates to pi/180 which, when used as a multiplier,
 *      converts an angle from degrees to radians.
 *    - \c R2D evaluates to 180/pi which as a multiplier converts an
 *      angle from radians to degrees.
 *
 * -# **Variables** are used to provide inputs to an expression, and are
 *  named using the single letters \c A through \c U inclusive or the
 *  keyword \c VAL which refers to the previous result of this
 *  calculation.
 *  The software that makes use of the expression evaluation code should
 *  document how the individual variables are given values.
 *  For the calc and calcout record types the input links \c INPA through
 *  \c INPU can be used to obtain values from other record fields, and \c
 *  VAL refers to the the VAL field (which can be overwritten from
 *  outside the record via Channel Access or a database link).
 *
 * -# The **Variable Assignment Operator** was added in 3.14.9 and
 *  provides the ability to assign the result of a sub-expression to any
 *  of the single letter variables, which can then be used in later
 *  sub-expressions.
 *  The variable assignment operator is the character pair <tt>:=</tt> and
 *  must immediately follow the name of the variable to receive the
 *  expression value.
 *  Since the infix string must return exactly one value, every
 *  expression string must have exactly one sub-expression that is not an
 *  assignment, which can appear anywhere in the string.
 *  Sub-expressions within the string are separated by a semi-colon
 *  character <tt>;</tt> .
 *
 *  Examples:
 *    - <tt>B; B:=A</tt>
 *    - <tt>i:=i+1; a*sin(i*D2R)</tt>
 *
 * -# The standard binary **Arithmetic Operators** are provided: <tt>+ -
 *  *</tt> and \c / with their usual relative precedence and
 *  left-to-right associativity.
 *  A minus sign \c - may also be used as a unary negate operator where
 *  it has a higher precedence and associates from right to left.
 *  There is no unary plus operator, so numeric literals cannot begin
 *  with a plus sign \c + .
 *
 *  Examples:
 *       - <tt>a*b + c</tt>
 *       - <tt>a/-4 - b</tt>
 *
 *  Three other binary operators are also provided:
 *  \c % is the integer modulo operator, while the synonymous operators
 *  \c ** and \c ^ raise their left operand to the power of the right
 *  operand.
 *  \c % has the same precedence and associativity as \c * and \c /,
 *  while the power operators associate left-to-right and have a
 *  precedence in between \c * and unary minus \c - .
 *
 *  Examples:
 *      - <tt>e:=a%10</tt>
 *      - <tt>d:=a/10%10</tt>
 *      - <tt>c:=a/100%10</tt>
 *      - <tt>b:=a/1000%10</tt>
 *      - <tt>b*4096+c*256+d*16+e</tt>
 *      - <tt>sqrt(a**2 + b**2)</tt>
 *
 * -# Various **Algebraic Functions** are available which take parameters
 *  inside parentheses.
 *  The parameter separator is a comma <tt>,</tt> .
 *
 *    - Absolute value: \c abs(a)
 *    - Exponential ea: \c exp(a)
 *    - Logarithm, base 10: \c log(a)
 *    - Natural logarithm (base e): \c ln(a) or \c loge(a)
 *    - n parameter maximum value: <tt>max(a, b, ...)</tt>
 *    - n parameter minimum value: <tt>min(a, b, ...)</tt>
 *    - Square root: \c sqr(a) or \c sqrt(a)
 *    - Floating point modulo: <tt>fmod(num, den)</tt>
 *      <br>The \c fmod() function was added in 7.0.8
 *
 * -# Standard circular **Trigonometric Functions** exist with angles
 *  expressed in radians:
 *
 *    - Sine: \c sin(a)
 *    - Cosine: \c cos(a)
 *    - Tangent: \c tan(a)
 *    - Arcsine: \c asin(a)
 *    - Arccosine: \c acos(a)
 *    - Arctangent: \c atan(a)
 *    - 2 parameter arctangent: <tt>atan2(a, b)</tt>
 *      <br>Note that the \c atan2 arguments are the reverse of the ANSI C
 *      function, so while C would return \c arctan(a/b) the calc
 *      expression engine returns \c arctan(b/a)
 *
 * -# The basic **Hyperbolic Trigonometry** functions are provided, but
 *  no inverse functions (which aren't provided by the ANSI C math
 *  library either).
 *
 *     - Hyperbolic sine: \c sinh(a)
 *     - Hyperbolic cosine: \c cosh(a)
 *     - Hyperbolic tangent: \c tanh(a)
 *
 * -# These **Numeric Functions** perform operations related to the
 *  floating point numeric representation and truncation or rounding.
 *
 *     - Round up to next integer: \c ceil(a)
 *     - Round down to next integer: \c floor(a)
 *     - Round to nearest integer: \c nint(a)
 *     - Test for infinite result: \c isinf(a)
 *     - Test for any non-numeric values: <tt>isnan(a, ...)</tt>
 *     - Test for all finite, numeric values: <tt>finite(a, ...)</tt>
 *     - Random number between 0 and 1: \c rndm
 *
 * -# The **Boolean Operators** evaluate their arguments as true or
 *  false, where \c 0.0 is false and any other value is true.
 *
 *     - Boolean and: <tt>a && b</tt>
 *     - Boolean or: <tt>a || b</tt>
 *     - Boolean not: \c !a
 *
 * -# Most **Bitwise Operators** convert their arguments to 32-bit signed
 *  integer (by truncation), perform the appropriate bitwise operation,
 *  then convert back to a floating point value.
 *  The arithmetic right shift operator \c >> thus retains the sign bit of
 *  the left-hand argument.
 *  The logical right shift operator \c >>> is performed on an unsigned
 *  integer though, so it injects zeros while shifting.
 *  The right-hand shift argument is masked so only the lower 5 bits are
 *  used.
 *  Unlike in C, \c ^ is not a bitwise exclusive-or operator.
 *
 *     - Bitwise and: <tt>a & b</tt> or <tt>a and b</tt>
 *     - Bitwise or: <tt>a | b</tt> or <tt>a or b</tt>
 *     - Bitwise exclusive or: <tt>a xor b</tt>
 *     - Bitwise not (ones complement): <tt>~a</tt> or <tt>not a</tt>
 *     - Arithmetic left shift: <tt>a << b</tt>
 *     - Arithmetic right shift: <tt>a >> b</tt>
 *     - Logical right shift: <tt>a >>> b</tt>
 *
 * -# The **Relational Operators** perform numeric comparisons between
 *  two double-precision values:
 *
 *     - Less than: <tt>a < b</tt>
 *     - Less than or equal to: <tt>a <= b</tt>
 *     - Equal to: <tt>a = b</tt> or <tt>a == b</tt>
 *     - Greater than or equal to: <tt>a >= b</tt>
 *     - Greater than: <tt>a > b</tt>
 *     - Not equal to: <tt>a != b</tt> or <tt>a # b</tt>
 *
 * -# Expressions can use the C **Conditional Operator**, which has a
 *  lower precedence than all of the other operators except for the
 *  assignment operator.
 *
 *     - \a condition <tt>?</tt> \a true-expression <tt>:</tt>
 *       \a false-expression
 *     - Example:
 *        <tt>a < 360 ? a+1 : 0</tt>
 *
 * -# Sub-expressions can be placed within **Parentheses** <tt>()</tt>
 *  to override operator presence rules.
 *  Parentheses can be nested to any depth, but the intermediate value
 *  stack used by the expression evaluation engine is limited to 80
 *  results (which takes an expression at least 321 characters long to
 *  reach).
 */
LIBCOM_API long
    postfix(const char *pinfix, char *ppostfix, short *perror);

/** \brief Run the calculation engine
 *
 * Evaluates the postfix expression against a set ot input values.
 *
 * \param parg Pointer to an array of double values for the arguments
 * \c A-U that can appear in the expression.
 * Note that the argument values may be modified if the expression uses
 * the assignment operator.
 * \param presult Where to put the calculated result, which may be a NaN
 * or Infinity.
 * \param ppostfix The postfix expression created by postfix().
 * \return Status value 0 for OK, or non-zero if an error is discovered
 * during the evaluation process.
 */
LIBCOM_API long
    calcPerform(double *parg, double *presult, const char *ppostfix);

/** \brief Find the inputs and outputs of an expression
 *
 * Software using the calc subsystem may need to know what expression
 * arguments are used and/or modified by a particular expression. It can
 * discover this from the postfix string by calling calcArgUsage(), which
 * takes two pointers \c pinputs and \c pstores to a pair of unsigned long
 * bitmaps which return that information to the caller. Passing a NULL value
 * for either of these pointers is legal if only the other is needed.
 *
 * The least significant bit (bit 0) of the bitmap at \c *pinputs will be set
 * if the expression depends on the argument A, and so on through bit 20 for
 * the argument U. An argument that is not used until after a value has been
 * assigned to it will not be set in the pinputs bitmap, thus the bits can
 * be used to determine whether a value needs to be supplied for their
 * associated argument or not for the purposes of evaluating the expression.
 *
 * Bit 0 of the bitmap at \c *pstores will be set if the expression assigns
 * a value to the argument A, bit 1 for argument B etc.
 * \param ppostfix A postfix expression created by postfix().
 * \param pinputs Bitmap pointer.
 * \param pstores Bitmap pointer.
 * \return The return value will be non-zero if the ppostfix expression was
 * illegal, otherwise 0.
 */
LIBCOM_API long
    calcArgUsage(const char *ppostfix, unsigned long *pinputs, unsigned long *pstores);

/** \brief Convert an error code to a string.
 *
 * Gives out a printable version of an individual error code.
 * The error codes are macros defined here with names starting \c CALC_ERR_
 * \param error Error code
 * \return A string representation of the error code
 */
LIBCOM_API const char *
    calcErrorStr(short error);

/** \brief Disassemble a postfix expression
 *
 * Convert the byte-code stream to text and print to stdout.
 * \param pinst postfix instructions
 */
LIBCOM_API void
    calcExprDump(const char *pinst);

#ifdef __cplusplus
}
#endif

#endif /* INCpostfixh */
