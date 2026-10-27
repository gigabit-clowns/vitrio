Scratches
=========

A scratch is a copy of the images a program goes back to, such as the
particles of a dataset read once per epoch. A loader made with one reads
what the scratch holds from it, and takes in what it does not hold yet.

.. autoclass:: vitrio.ImageScratch

.. autofunction:: vitrio.create_host_image_scratch
.. autofunction:: vitrio.create_file_image_scratch
.. autofunction:: vitrio.create_directory_image_scratch
.. autofunction:: vitrio.prefetch_scratch_async
