/* libmain - flex run-time support library "main" function */

extern int yylex();

int main(int argc, char *argv[])
{
    return yylex();
}
