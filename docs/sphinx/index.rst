vitrio
======

vitrio is an image I/O library for Cryo-Electron Microscopy (cryo-EM and
cryo-ET). This is the documentation of its Python package. The
`README <https://github.com/gigabit-clowns/vitrio#readme>`_ introduces the
library, the
`examples <https://github.com/gigabit-clowns/vitrio/tree/main/examples>`_
are programs you can run, and the C++ library has a reference of its own at
`../cpp/ <../cpp/index.html>`_.

.. code-block:: python

   import vitrio as vio

   micrograph = vio.read("micrograph.mrc")
   vio.write_single(micrograph, "copy.tif")

:func:`vitrio.read` is the simplest call, and the slowest way to go through
many images. Before you write a loop over files, read the
:doc:`performance guide <performance>`. It tells you when to use
:mod:`vitrio.data`, when to use the batch functions such as
:func:`vitrio.read_batch_async`, and how to set them up for your storage.

Wherever a function takes an array, it takes anything that shares host
memory through DLPack or the buffer protocol: a NumPy array, a PyTorch or
JAX tensor, a ``memoryview``. The memory is read or written in place and
nothing is copied. A read returns a :class:`vitrio.Array`, which those
libraries use in turn.

The functions that end in ``_async`` return before the work is done, with a
:class:`vitrio.Completion` to wait on. Anything that reads, writes or waits
releases the GIL meanwhile.

.. toctree::
   :maxdepth: 1
   :caption: Guides

   performance
   formats
   building

.. toctree::
   :maxdepth: 1
   :caption: Reference

   reading_and_writing
   arrays_and_locations
   loaders_and_providers
   scratches
   detector_events
   data
