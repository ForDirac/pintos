pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --qemu  --disk=tmp.dsk -p build/tests/filesys/extended/grow-two-files -a grow-two-files -p build/tests/filesys/extended/tar -a tar --swap-size=4 -- -q  -f run grow-two-files
pintos -v -k -T 60  --qemu --disk=tmp.dsk -g fs.tar -a build/tests/filesys/extended/grow-two-files.tar --swap-size=4 -- -q  run 'tar fs.tar /'
rm -f tmp.dsk
