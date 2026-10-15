Reading and writing
===================

The format of a file is worked out from the file when it is read, and from
its extension when it is written.

Whole files
-----------

.. autofunction:: vitrio.query_descriptor
.. autofunction:: vitrio.read
.. autofunction:: vitrio.write_single
.. autofunction:: vitrio.write_stack
.. autofunction:: vitrio.write

Batches and patches
-------------------

A batch is read or written on the threads of a loader or a saver, and its
images may come from any number of files.

.. autofunction:: vitrio.read_batch_async
.. autofunction:: vitrio.read_patches_async
.. autofunction:: vitrio.write_batch_async

.. autoclass:: vitrio.Completion
   :members:
