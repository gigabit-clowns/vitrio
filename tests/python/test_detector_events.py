# SPDX-License-Identifier: LGPL-2.1-or-later

import gc
import pathlib

import numpy as np
import pytest

import vitrio as vio

ASSETS = pathlib.Path(__file__).resolve().parents[1] / 'assets' / 'eer'

# The files of tests/assets/eer, as their README describes them: frames,
# columns, rows, horizontal and vertical subpixel bits, and empty frames.
RLE7 = ('movie_rle7.eer', 4, 8, 6, 2, 2, ())
RLE6 = ('movie_rle6.eer', 3, 16, 8, 1, 2, (1,))

@pytest.fixture(scope='module')
def eer():
	try:
		vio.detector_event_timeline_reader_provider().acquire(ASSETS / RLE7[0])
	except Exception:
		pytest.skip('this build reads no EER files')

def test_a_fractionation_cuts_time_as_evenly_as_it_can():
	fractionation = vio.DetectorEventFractionation(4)
	begins = [fractionation.get_fraction_begin(i, 14) for i in range(5)]
	assert begins == [0, 3, 7, 10, 14]
	assert fractionation.fraction_count == 4
	assert fractionation == vio.DetectorEventFractionation(4)
	assert repr(fractionation) == \
		'DetectorEventFractionation(fraction_count=4)'

def test_a_fractionation_refuses_what_has_no_fractions():
	with pytest.raises(ValueError):
		vio.DetectorEventFractionation(0)
	with pytest.raises(IndexError):
		vio.DetectorEventFractionation(2).get_fraction_begin(3, 10)
	assert vio.DetectorEventFractionation.max_fraction_count == 2**32 - 1

def test_a_descriptor_holds_what_it_was_given():
	descriptor = vio.DetectorEventTimelineDescriptor(
		(16, 32), (4, 4), 10, 0.5)
	assert descriptor.rank == 2
	assert descriptor.extents == (16, 32)
	assert descriptor.subdivisions == (4, 4)
	assert descriptor.time_extent == 10
	assert descriptor.time_quantum == 0.5
	assert descriptor == vio.DetectorEventTimelineDescriptor(
		(16, 32), (4, 4), 10, 0.5)
	assert 'subdivisions=(4, 4)' in repr(descriptor)

def test_a_descriptor_refuses_subdivisions_that_do_not_divide():
	with pytest.raises(ValueError):
		vio.DetectorEventTimelineDescriptor((16, 30), (4, 4), 10, 0.0)

def test_a_binning_resolves_into_position_quanta():
	descriptor = vio.DetectorEventTimelineDescriptor(
		(16, 32), (4, 4), 1, 0.0)
	assert vio.DetectorEventBinning(1).resolve(descriptor) == (4, 4)
	assert vio.DetectorEventBinning(2).resolve(descriptor) == (8, 8)
	with pytest.raises(ValueError):
		vio.DetectorEventBinning(3).resolve(descriptor)
	with pytest.raises(ValueError):
		vio.DetectorEventBinning(0)

def test_a_counting_renderer_reports_its_images():
	renderer = vio.CountingDetectorEventRenderer(vio.DetectorEventBinning(2))
	descriptor = vio.DetectorEventTimelineDescriptor(
		(16, 32), (4, 4), 1, 0.0)
	assert renderer.binning == vio.DetectorEventBinning(2)
	assert renderer.data_type == vio.NumericalType.uint32
	assert renderer.get_image_extents(descriptor) == (2, 4)

def test_a_timeline_takes_groups_of_positions():
	timeline = vio.DetectorEventTimeline(2)
	timeline.add_group(3, np.array([[0, 1], [2, 3]], dtype=np.uint32))
	timeline.add_group(5, np.array([[4, 5]], dtype=np.uint32))
	assert timeline.group_count == 2
	assert timeline.event_count == 3
	assert np.array_equal(np.asarray(timeline.timestamps), [3, 5])
	assert np.array_equal(
		np.asarray(timeline.get_positions(1)), [[4, 5]])
	assert np.array_equal(
		np.asarray(timeline.get_positions()), [[0, 1], [2, 3], [4, 5]])

def test_a_timeline_refuses_what_is_not_a_group():
	timeline = vio.DetectorEventTimeline(2)
	timeline.add_group(3, np.zeros((1, 2), dtype=np.uint32))
	with pytest.raises(ValueError):
		timeline.add_group(3, np.zeros((1, 2), dtype=np.uint32))
	with pytest.raises(ValueError):
		timeline.add_group(4, np.zeros((1, 3), dtype=np.uint32))
	with pytest.raises(IndexError):
		timeline.get_positions(1)

def test_a_view_of_a_timeline_is_read_only():
	timeline = vio.DetectorEventTimeline(1)
	timeline.add_group(0, np.array([[7]], dtype=np.uint32))
	view = np.asarray(timeline.get_positions())
	assert not view.flags.writeable

def test_a_timeline_is_not_changed_while_a_view_of_it_is_alive():
	timeline = vio.DetectorEventTimeline(1)
	timeline.add_group(0, np.array([[7]], dtype=np.uint32))
	view = np.asarray(timeline.get_positions())
	assert timeline.view_count == 1
	with pytest.raises(BufferError):
		timeline.clear()
	with pytest.raises(BufferError):
		timeline.add_group(1, np.array([[8]], dtype=np.uint32))
	assert view[0, 0] == 7

	del view
	gc.collect()
	assert timeline.view_count == 0
	timeline.clear()
	assert timeline.group_count == 0

def test_a_view_keeps_its_timeline_alive():
	timeline = vio.DetectorEventTimeline(1)
	timeline.add_group(0, np.array([[7], [9]], dtype=np.uint32))
	view = np.asarray(timeline.get_positions())
	del timeline
	gc.collect()
	assert view[:, 0].tolist() == [7, 9]

def test_events_are_read_a_stretch_of_time_at_a_time(eer):
	timeline = vio.read_events(ASSETS / RLE6[0])
	assert np.array_equal(np.asarray(timeline.timestamps), [0, 2])
	assert timeline.event_count == 52

	stretch = vio.read_events(ASSETS / RLE6[0], 2, 3)
	assert np.array_equal(np.asarray(stretch.timestamps), [2])

def test_events_land_where_the_formula_places_them(eer):
	for asset in (RLE7, RLE6):
		timeline = vio.read_events(ASSETS / asset[0])
		placed = np.zeros_like(__subpixel_frames(asset))
		for group, frame in enumerate(np.asarray(timeline.timestamps)):
			positions = np.asarray(timeline.get_positions(group))
			np.add.at(placed[frame], (positions[:, 0], positions[:, 1]), 1)
			del positions
		assert np.array_equal(placed, __subpixel_frames(asset))

def test_a_timeline_being_viewed_is_not_read_into(eer):
	events = vio.detector_event_timeline_reader_provider()
	reader = events.acquire(ASSETS / RLE7[0])
	timeline = vio.DetectorEventTimeline(2)
	reader.read(0, 1, timeline)
	view = np.asarray(timeline.get_positions())
	with pytest.raises(BufferError):
		reader.read(1, 2, timeline)
	del view

def test_an_eer_file_reads_as_a_movie(eer):
	expected = __binned(RLE7, __subpixel_frames(RLE7), 1)
	movie = np.asarray(vio.read(
		ASSETS / RLE7[0], vio.detector_event_image_reader_provider(4)))
	assert movie.shape == (4, 6, 8)
	assert movie.dtype == np.uint32
	assert np.array_equal(movie, expected)

def test_fractions_gather_frames_as_evenly_as_they_can(eer):
	frames = __subpixel_frames(RLE7)
	movie = np.asarray(vio.read(
		ASSETS / RLE7[0], vio.detector_event_image_reader_provider(3)))
	# Three fractions of four frames begin at frames 0, 1 and 2.
	expected = np.stack([
		__binned(RLE7, frames[0:1].sum(0, keepdims=True), 1)[0],
		__binned(RLE7, frames[1:2].sum(0, keepdims=True), 1)[0],
		__binned(RLE7, frames[2:4].sum(0, keepdims=True), 1)[0],
	])
	assert np.array_equal(movie, expected)

def test_a_movie_is_binned_and_converted_on_the_way(eer):
	frames = __subpixel_frames(RLE6)
	image = np.asarray(vio.read(
		ASSETS / RLE6[0],
		vio.detector_event_image_reader_provider(1, binning=2),
		data_type='float32'
	))
	assert image.shape == (4, 8)
	assert image.dtype == np.float32
	assert np.array_equal(
		image, __binned(RLE6, frames.sum(0, keepdims=True), 2)[0])

def test_a_binning_that_does_not_divide_the_frames_is_refused(eer):
	with pytest.raises(ValueError):
		vio.read(
			ASSETS / RLE7[0],
			vio.detector_event_image_reader_provider(1, binning=4)
		)

def test_a_movie_provider_caches_its_readers(eer):
	readers = vio.detector_event_image_reader_provider(2, cache=1)
	assert isinstance(readers, vio.CachingImageReaderProvider)
	assert vio.query_descriptor(ASSETS / RLE7[0], readers).extents == \
		(2, 6, 8)

def test_events_are_rendered_into_any_array(eer):
	events = vio.detector_event_timeline_reader_provider()
	reader = events.acquire(ASSETS / RLE7[0])
	timeline = vio.DetectorEventTimeline(2)
	reader.read(0, 4, timeline)
	renderer = vio.CountingDetectorEventRenderer(vio.DetectorEventBinning(2))
	image = np.full((3, 4), -1.0)
	renderer.render(reader.descriptor, timeline, image)
	frames = __subpixel_frames(RLE7)
	assert np.array_equal(
		image, __binned(RLE7, frames.sum(0, keepdims=True), 2)[0])

def __subpixel_frames(asset):
	"""
	The events of each frame on the grid of subpixels, restated from the
	formula of the README of the assets.
	"""
	_, frame_count, width, height, hbits, vbits, empty = asset
	frames = np.zeros(
		(frame_count, height << vbits, width << hbits), dtype=np.uint32)
	pixels = np.arange(width * height)
	for frame in range(frame_count):
		if frame in empty:
			continue
		hit = pixels[(7 * pixels + 3 * frame) % 5 == 0]
		rows = ((hit // width) << vbits) + (3 * hit + frame) % (1 << vbits)
		columns = ((hit % width) << hbits) + (hit + frame) % (1 << hbits)
		np.add.at(frames[frame], (rows, columns), 1)
	return frames

def __binned(asset, frames, factor):
	"""
	Frames of subpixels gathered into blocks of a factor of pixels of the
	detector.
	"""
	_, _, width, height, hbits, vbits, _ = asset
	block_rows = factor << vbits
	block_columns = factor << hbits
	return frames.reshape(
		frames.shape[0],
		height // factor, block_rows,
		width // factor, block_columns
	).sum(axis=(2, 4), dtype=np.uint32)
