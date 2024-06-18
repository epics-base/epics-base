set "A" "A outer"
set "B" "B outer"
iocshLoad "iocshTestLocal2.cmd"
epicsEnvSet "outerB" "$(B)"


