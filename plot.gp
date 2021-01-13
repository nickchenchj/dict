reset
set style fill solid
set xlabel 'experiment'
set ylabel 'time(sec)'
set terminal png font " Times_New_Roman,12 "
set output 'out.png'

plot [:][:] \
"./cpy_bloom.txt" with points title "CPY with Bloom filter", \
"./ref_bloom.txt" with points title "REF with Bloom filter", \
"./cpy_no_bloom.txt" with points title "CPY without Bloom filter", \
"./ref_no_bloom.txt" with points title "REF without Bloom filter"