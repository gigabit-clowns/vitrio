Detector events
===============

A detector that counts electrons records events, not images, as an EER
file does. Its events are read as they are, or become a movie by cutting
time into fractions and rendering each.

Reading
-------

.. autofunction:: vitrio.read_events
.. autofunction:: vitrio.detector_event_timeline_reader_provider
.. autofunction:: vitrio.detector_event_image_reader_provider

Events
------

.. autoclass:: vitrio.DetectorEventTimeline
   :members:
.. autoclass:: vitrio.DetectorEventTimelineDescriptor
   :members:
.. autoclass:: vitrio.DetectorEventTimelineReader
   :members:
.. autoclass:: vitrio.DetectorEventTimelineReaderProvider
   :members:
.. autoclass:: vitrio.FileDetectorEventTimelineReaderProvider
.. autoclass:: vitrio.DetectorEventTimelineFileReadFormatSelector
   :members:
   :exclude-members: get_shared

   .. automethod:: get_shared

Movies
------

.. autoclass:: vitrio.DetectorEventFractionation
   :members:
.. autoclass:: vitrio.DetectorEventBinning
   :members:
.. autoclass:: vitrio.DetectorEventRenderer
   :members:
.. autoclass:: vitrio.CountingDetectorEventRenderer
   :members:
.. autoclass:: vitrio.DetectorEventImageReaderProvider
