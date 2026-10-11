DigitalMicrograph test assets

These files are here so that the DM reader is tested against bytes this
project did not write through its own code. They are synthetic:
make_dm_assets.py, beside them, lays out the tags of each file by hand from
the description of the format, through neither vitrio nor any reader of it,
so that a test can state what a file holds without reading it by other means.

  python make_dm_assets.py tests/assets/dm

They were then read by RosettaSciIO 0.15.0 and ncempy 1.16, the readers of
HyperSpy and of the National Center for Electron Microscopy, which found the
image, its shape, its data type and its values in each file either can read.
Neither reads every one of them, as their notes below say. That is the check
that the layout is that of the format, as those readers have it, rather than
whatever this project would agree with itself on.

What they are not is the output of Gatan Microscopy Suite: their images are a
few pixels wide, and their tags are the handful the image is described by,
with none of the acquisition metadata GMS writes beside it. Text is an array
of 16-bit characters, as GMS writes it. A datum named Padding, an array of
bytes before the image list, moves the values of the image to begin at an
offset aligned for their data type or not, as each file asks.


The values

p counts the values of a file from zero, in the order they are stored, the
columns changing fastest, then the rows, then the images of a stack.

  float32   0.25 * p - 4
  uint16    1000 + 3 * p
  int16     100 * (p div 24) - (p mod 24)


image_float32.dm3
1,784 bytes
Version 3, little-endian. An image of 8 columns by 6 rows of float32 values,
which begin at byte 1,216, aligned. The image list holds a thumbnail first,
an RGBA image of 4 by 2 pixels that the Thumbnails group names.

Read by RosettaSciIO and ncempy alike.


image_uint16_unaligned.dm4
2,729 bytes
Version 4, little-endian. An image of 7 columns by 5 rows of uint16 values,
which begin at byte 2,033, an odd offset. A thumbnail comes first, as above.

Read by RosettaSciIO and ncempy alike.


stack_int16.dm4
2,971 bytes
Version 4, little-endian. A stack of 3 images of 6 columns by 4 rows of int16
values, which begin at byte 2,188, aligned. Its Dimensions are 6, 4 and 3, the
columns first. A thumbnail comes first, as above.

Read by RosettaSciIO and ncempy alike.


image_float32_big_endian.dm3
925 bytes
Version 3, big-endian. An image of 5 columns by 4 rows of float32 values,
which begin at byte 533, unaligned. There is no thumbnail.

Read by RosettaSciIO, which finds its tags, its shape and its data type, but
reads the values of an image in the byte order of the host whatever the file
states: what it reads are these values with their bytes reversed. ncempy
refuses big-endian files.
