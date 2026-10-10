EER test assets

These files are here so that the EER reader is tested against bytes this
project did not write. They are synthetic: make_eer_assets.py, beside them,
encodes events that follow a formula into the strips of each frame and writes
the TIFF file around them by hand, through neither libtiff nor vitrio, so that
a test can state what a file holds without reading it by other means.

  python make_eer_assets.py tests/assets/eer

Both were then decoded by imagecodecs 2026.10.10, through tifffile 2026.9.20
asked for every subpixel (superres=2), which found each event the formula
places and no other. That is the check that the bit order, the order of the
fields of a code and the stored subpixels are those of the format, as its
reference reader and imagecodecs have them, rather than whatever this project
would agree with itself on.

What they are not is the output of a detector: their frames are 8 by 6 and
16 by 8 pixels rather than 4096 by 4096, and their acquisition metadata is a
single item.


The events

Frame f, of w by h pixels, holds an event on pixel p, counted row after row,
when (7 * p + 3 * f) mod 5 is 0. Its horizontal subpixel is (p + f) mod 2^hb
and its vertical one (3 * p + f) mod 2^vb, hb and vb being the subpixel bits
of the file. A frame listed as empty holds none.

The position of an event, on the grid the subpixels make, is

  row     (p div w) * 2^vb + vertical
  column  (p mod w) * 2^hb + horizontal

A subpixel is stored with the bit its width counts flipped: with 2 bits, the
stored 2 is the subpixel 0.


movie_rle7.eer
1,385 bytes
Four frames of 8 columns by 6 rows, compressed with 65001: runs of 7 bits and
subpixels of 2 bits each way. Each frame is cut into two strips of 3 rows, a
stream each. No frame is empty. 39 events, 10, 10, 10 and 9 per frame.

The fixed encoding of the format, and the one its files carry the most.


movie_rle6.eer
1,229 bytes
Three frames of 16 columns by 8 rows, compressed with 65002, whose tags 65007,
65008 and 65009 state runs of 6 bits, a horizontal subpixel of 1 bit and a
vertical one of 2 bits. Each frame is cut into two strips of 4 rows. The
second frame is empty. 52 events, 26 in each of the others.

The encoding whose widths vary, read from private tags, and a strip of 64
pixels with nothing on them, more than a run of 6 bits reaches, which takes
codes that place no event.
