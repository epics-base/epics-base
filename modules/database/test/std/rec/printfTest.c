/*************************************************************************\
* Copyright (c) 2023 Karl Vestin
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "dbUnitTest.h"
#include "testMain.h"
#include "errlog.h"
#include "dbAccess.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void test_double_percentile(void){
    const char format_string[] = "Format test string %%d";
    const char result_string[] = "Format test string %d";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 1);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_d_format(void){
    const char format_string[] = "Format test string %d";
    const char result_string[] = "Format test string 7";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 7);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_c_format(void){
    const char format_string[] = "Format test string %c";
    const char result_string[] = "Format test string R";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0, 82 is ASCII for R */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 82);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_i_format(void){
    const char format_string[] = "Format test string %i";
    const char result_string[] = "Format test string -27";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, -27);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_o_format(void){
    const char format_string[] = "Format test string %o";
    const char result_string[] = "Format test string 6777";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 06777);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_u_format(void){
    const char format_string[] = "Format test string %u";
    const char result_string[] = "Format test string 8009";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 8009);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_x_format(void){
    const char format_string[] = "Format test string %x";
    const char result_string[] = "Format test string fafa";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_LONG, 0xfafa);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_X_format(void){
    const char format_string[] = "Format test string %X";
    const char result_string[] = "Format test string BA";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 0x00ba);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_e_format(void){
    const char format_string[] = "Format test string %e";
    const char result_string[] = "Format test string -1.400000e+01";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, -14);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_E_format(void){
    const char format_string[] = "Format test string %E";
    const char result_string[] = "Format test string 1.992000E+03";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 1992);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_f_format(void){
    const char format_string[] = "Format test string %f";
    const char result_string[] = "Format test string -0.062000";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_DOUBLE, -0.062);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_F_format(void){
    const char format_string[] = "Format test string %F";
    const char result_string[] = "Format test string 6729982.999000";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_DOUBLE, 6729982.999);
    
    /* verify that string is formatted as expected */
    // visual studio less than 2015 does not support %F
    // mingw/gcc also fails, suspect this may be gcc version
    // related and checking __GNUC__ could resolve but 
    // initial attempts didn't work so excluding mingw entirely for now
    #ifdef _WIN32
    #if (defined(_MSC_VER) && _MSC_VER < 1900) || defined(_MINGW)
    testTodoBegin("Fails on windows with old visual studio versions and mingw");
    #endif
    #endif
    
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);
    
    #ifdef _WIN32
    #if (defined(_MSC_VER) && _MSC_VER < 1900) || defined(_MINGW)
    testTodoEnd();
    #endif
    #endif
    // number of tests = 3
}

static void test_g_format(void){
    const char format_string[] = "Format test string %g";
    const char result_string[] = "Format test string -0.093";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_DOUBLE, -93e-3);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_G_format(void){
    const char format_string[] = "Format test string %G";
    const char result_string[] = "Format test string 7.2884E+08";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_LONG, 728839938);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_s_format(void){

    const char format_string[] = "Format test string %d %s";
    const char result_string[] = "Format test string 7 Molly III";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 7);

    /* set value on inp1 */
    testdbPutFieldOk("test_printf_inp1_rec.VAL", DBF_STRING, "Molly III");

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 4
}

static void test_plus_flag(void){

    const char format_string[] = "Format test string %+d";
    const char result_string[] = "Format test string +7";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 7);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_minus_flag(void){

    const char format_string[] = "Format test string %-10d";
    const char result_string[] = "Format test string 18        ";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 18);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_space_flag(void){

    const char format_string[] = "Format test string % d";
    const char result_string[] = "Format test string  12";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 12);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_hash_flag(void){

    const char format_string[] = "Format test string %#o";
    const char result_string[] = "Format test string 014";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 014);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_min_width_flag(void){

    const char format_string[] = "Format test string %04i";
    const char result_string[] = "Format test string 0003";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 3);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_prec_flag(void){

    const char format_string[] = "Format test string %.4f";
    const char result_string[] = "Format test string 71.2000";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_DOUBLE, 71.2);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_h_flag(void){

    const char format_string[] = "Format test string %hx";
    const char result_string[] = "Format test string baba";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_LONG, 0xffbaba);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 3
}

static void test_hh_flag(void){

    const char format_string[] = "Format test string %hhx";
    const char result_string[] = "Format test string c1";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_LONG, 0xffc0c1);

    /* verify that string is formatted as expected */
    #ifdef __rtems__
    testTodoBegin("Fails on UB-20 gcc-9 on RTEMS");
    #endif
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);
    #ifdef __rtems__
    testTodoEnd();
    #endif

    // number of tests = 3
}

static void test_l_flag(void){

    const char format_string[] = "Format test string %lx";
    const char result_string[] = "Format test string 70a1c0c1";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_LONG, 0x70a1c0c1);
    
    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);
    // number of tests = 3
}

static void test_ll_flag(void){

    const char format_string[] = "%d %s %llx";
    const char result_string[] = "2 Reperbahn ba0110baa0a1c0c1";
    const epicsInt64 value = 0xba0110baa0a1c0c1ull;

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 2);

    /* set value on inp1 */
    testdbPutFieldOk("test_printf_inp1_rec.VAL", DBF_STRING, "Reperbahn");

    /* set value on inp2 */
    testdbPutFieldOk("test_printf_inp2_rec.VAL", DBR_INT64, value);
    testdbGetFieldEqual("test_printf_inp2_rec.VAL", DBR_INT64, value);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 6
}

static void test_sizv(void){

    const char format_string[] = "%d %s %llx";
    const char result_string[] = "99 123456789012345678901234567890 6640baa0a1";
    const epicsInt64 value = 0x06640baa0a1c0c1ull;

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 99);

    /* set value on inp1 */
    testdbPutFieldOk("test_printf_inp1_rec.VAL", DBF_STRING, "123456789012345678901234567890");

    /* set value on inp2 */
    testdbPutFieldOk("test_printf_inp2_rec.VAL", DBR_INT64, value);
    testdbGetFieldEqual("test_printf_inp2_rec.VAL", DBR_INT64, value);

    /* verify that string is formatted as expected */
    testdbGetArrFieldEqual("test_printf_rec.VAL$", DBF_CHAR, 45, 45, result_string);

    // number of tests = 6
}

static void test_all_inputs(void){

    const char format_string[] = "%d %s %i %i %i %i %i %i %i %i";
    const char result_string[] = "0 One 2 3 4 5 6 7 8 9";

    /* set format string */
    testdbPutFieldOk("test_printf_rec.FMT", DBF_STRING, format_string);

    /* set value on inp0 */
    testdbPutFieldOk("test_printf_inp0_rec.VAL", DBF_SHORT, 0);

    /* set value on inp1 */
    testdbPutFieldOk("test_printf_inp1_rec.VAL", DBF_STRING, "One");

    /* set value on inp2 */
    testdbPutFieldOk("test_printf_inp2_rec.VAL", DBF_SHORT, 2);

    /* set value on inp3 */
    testdbPutFieldOk("test_printf_inp3_rec.VAL", DBF_SHORT, 3);

    /* set value on inp4 */
    testdbPutFieldOk("test_printf_inp4_rec.VAL", DBF_SHORT, 4);

    /* set value on inp5 */
    testdbPutFieldOk("test_printf_inp5_rec.VAL", DBF_SHORT, 5);

    /* set value on inp6 */
    testdbPutFieldOk("test_printf_inp6_rec.VAL", DBF_SHORT, 6);

    /* set value on inp7 */
    testdbPutFieldOk("test_printf_inp7_rec.VAL", DBF_SHORT, 7);

    /* set value on inp8 */
    testdbPutFieldOk("test_printf_inp8_rec.VAL", DBF_SHORT, 8);

    /* set value on inp9 */
    testdbPutFieldOk("test_printf_inp9_rec.VAL", DBF_SHORT, 9);

    /* verify that string is formatted as expected */
    testdbGetFieldEqual("test_printf_rec.VAL", DBF_STRING, result_string);

    // number of tests = 12
}

MAIN(printfTest) {
#ifdef _WIN32
#if (defined(_MSC_VER) && _MSC_VER < 1900) || \
    (defined(_MINGW) && defined(_TWO_DIGIT_EXPONENT))
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
#endif

    testPlan(3+3+3+3+3+3+3+3+4+3+3+3+3+3+3+3+3+3+3+3+3+3+3+3+6+6+12);

    testdbPrepare();   
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("printfTest.db", NULL, NULL);
    
    eltc(0);
    testIocInitOk();
    eltc(1);

    test_double_percentile();
    test_c_format();
    test_d_format();
    test_i_format();
    test_o_format();
    test_u_format();
    test_x_format();
    test_X_format();
    test_e_format();
    test_E_format();
    test_f_format();
    test_F_format();
    test_g_format();
    test_G_format();
    test_s_format();
    test_plus_flag();
    test_minus_flag();
    test_space_flag();
    test_hash_flag();
    test_min_width_flag();
    test_prec_flag();
    test_h_flag();
    test_hh_flag();
    test_l_flag();
    test_ll_flag();
    test_sizv();
    test_all_inputs();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
