vitrio
======

vitrio is a high-throughput image I/O library tailored to the access
patterns and file formats common in Cryo-Electron Microscopy (CryoEM). This
is the reference of its Python package. The C++ library it is made on has
its own, at `../cpp/ <../cpp/index.html>`_, and the
`README <https://github.com/gigabit-clowns/vitrio#readme>`_ and the
`examples <https://github.com/gigabit-clowns/vitrio/tree/main/examples>`_
show how the two are used.

.. code-block:: python

   import vitrio as vio

   micrograph = vio.read("micrograph.mrc")
   vio.write_single(micrograph, "copy.tif")

Anywhere an array is asked for, so is whatever hands out host memory
through DLPack or the buffer protocol: a numpy array, a tensor of PyTorch
or JAX, a ``memoryview``. It is read or written where it is, and nothing is
copied. What a read returns is a :class:`vitrio.Array`, which those use in
turn.

The functions that are ``_async`` return before the work is done, with a
:class:`vitrio.Completion` to wait on. Anything that reads, writes or waits
releases the GIL meanwhile.

.. toctree::
   :maxdepth: 1
   :caption: Reference

   reading_and_writing
   arrays_and_locations
   loaders_and_providers
   scratches
   detector_events
   data
