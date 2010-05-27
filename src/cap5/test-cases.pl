grep record BL02I-MO-IOC-02.db |\
grep -v '^#' |\
perl -e '
%h = (); 
while(<>){ 
   @a = split /,/; 
   $a[0] =~ s/record\(//; 
   $h{$a[0]}=$a[1]; 
} 
print %h;'

