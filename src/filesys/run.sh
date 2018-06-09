rm -f tmp.dsk
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --qemu  --disk=tmp.dsk -p build/tests/filesys/extended/syn-rw -a syn-rw -p build/tests/filesys/extended/tar -a tar -p build/tests/filesys/extended/child-syn-rw -a child-syn-rw --swap-size=4 -- -q  -f run syn-rw
#pintos -v -k -T 60  --qemu --disk=tmp.dsk -g fs.tar -a tests/filesys/extended/syn-rw.tar --swap-size=4 -- -q  run 'tar fs.tar /'
rm -f tmp.dsk
