gcc rle-to-ppm.c -o rle-to-ppm
cat $1 | ./rle-to-ppm > out.ppm
convert out.ppm out.png
convert out.png out.ppm
cat out.ppm | ./write $2 $3
