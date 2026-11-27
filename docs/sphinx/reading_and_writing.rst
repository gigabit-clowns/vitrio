Reading and writing
===================

The format of a file is worked out from the file when it is read, and from
its extension when it is written.

Whole files
-----------

These functions handle one file per call and wait for it. They suit a single
file. To go through many images, use the batches below, as the
:doc:`performance guide <performance>` explains.

.. autofunction:: vitrio.query_descriptor
.. autofunction:: vitrio.read
.. autofunction:: vitrio.write_single
.. autofunction:: vitrio.write_stack
.. autofunction:: vitrio.write

Metadata
--------

What a file states beyond its shape and data type, such as its pixel size,
is read apart from its values, and given when a file is written.

.. autofunction:: vitrio.read_metadata

Batches and patches
-------------------

A batch is read or written on the threads of a loader or a saver, and its
images may come from any number of files. These functions return at once, so
a program starts the next batch before it works on the current one. This is
the fastest way to read and write many images. :func:`vitrio.loader` and
:func:`vitrio.saver` make the loader and the saver.

.. autofunction:: vitrio.read_batch_async
.. autofunction:: vitrio.read_patches_async
.. autofunction:: vitrio.write_batch_async
.. autofunction:: vitrio.write_patches_async

.. autoclass:: vitrio.Completion
   :members:
