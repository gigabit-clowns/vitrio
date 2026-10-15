Loaders, savers and providers
=============================

A loader reads batches on an executor, and a saver writes them. Each is
given a provider, which turns a key into an open reader or writer. The
functions that assemble them with the defaults most programs want come
first, and the classes they put together follow.

Assembling
----------

.. autofunction:: vitrio.loader
.. autofunction:: vitrio.saver
.. autofunction:: vitrio.reader_provider
.. autofunction:: vitrio.writer_provider

Loaders and savers
------------------

.. autoclass:: vitrio.ImageLoader
.. autoclass:: vitrio.ExecutorImageLoader
.. autoclass:: vitrio.ImageSaver
.. autoclass:: vitrio.ExecutorImageSaver

Providers
---------

.. autoclass:: vitrio.ImageReaderProvider
.. autoclass:: vitrio.FileImageReaderProvider
.. autoclass:: vitrio.CachingImageReaderProvider
   :members:
.. autoclass:: vitrio.ScratchImageReaderProvider
.. autoclass:: vitrio.ImageWriterProvider
.. autoclass:: vitrio.FileImageWriterProvider
   :members:

Formats
-------

.. autoclass:: vitrio.ImageFileReadFormatSelector
   :members:
   :exclude-members: get_shared

   .. automethod:: get_shared

.. autoclass:: vitrio.ImageFileWriteFormatSelector
   :members:
   :exclude-members: get_shared

   .. automethod:: get_shared


Executors
---------

.. autoclass:: vitrio.Executor
.. autoclass:: vitrio.SynchronousExecutor
.. autoclass:: vitrio.ThreadPoolExecutor
   :members:
