// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

// The defaults of the tuning knobs of the library. Every one of them is
// reasonable on an ordinary machine and may be overridden from the build,
// which is what makes them worth defining rather than writing down at the
// point of use. As the library runs, the variable of the environment named
// as the macro without DEFAULT_ overrides each, as tuning.hpp reads them:
// VITRIO_PREFETCH_GAP_CAP overrides VITRIO_DEFAULT_PREFETCH_GAP_CAP.

// Widest gap between two stretches of a file that is still worth asking for
// in one piece rather than leaving to the fault that reaches it.
//
// The readahead a kernel does of its own accord is about this wide, so
// bridging a gap wider than it is speculation. What a batch actually bridges
// scales with what one of its regions spans and is only capped here, so that
// a gap wider than the data on either side of it is never bridged: a flat cap
// would have two small regions far apart ask for many times what they hold.
#ifndef VITRIO_DEFAULT_PREFETCH_GAP_CAP
	#define VITRIO_DEFAULT_PREFETCH_GAP_CAP (2UL<<16)
#endif

// Bytes one step of a read asks for before the regions of the step before it
// are walked.
//
// A batch that fits in this is asked for in one go, which is what a batch of
// small scattered regions wants: all of it is in flight before any of it is
// read, so every latency overlaps. Past it a batch is walked a step at a
// time, so that a read whose regions are large enough that asking for all of
// them at once would be asking for more than can be held asks for them as it
// reaches them instead.
//
// This is per read, so several reads at once hold several of these.
#ifndef VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET
	#define VITRIO_DEFAULT_PREFETCH_BYTE_BUDGET (4UL<<24)
#endif

// Bytes one step of a read or a write stages in memory of its own, where its
// values have to be converted on their way between the file and the array.
//
// A step is transferred while the one before it is moved, so a read or a
// write holds two steps of this at most, unless a single stretch is wider
// than a step. A scratch kept in a file takes values in through pieces of a
// step.
#ifndef VITRIO_DEFAULT_STAGING_BYTE_BUDGET
	#define VITRIO_DEFAULT_STAGING_BYTE_BUDGET (1UL<<23)
#endif

// Bytes the loads of a scratch kept in a file stage at once, all of them
// together.
//
// A load reads what it takes in into memory of its own before it writes it
// to the file, and a shuffled batch starts a load for nearly every run of a
// scratch, so what they stage is bounded: pieces past this wait for earlier
// ones, in the order they came, and one that alone stages more is started
// alone. Hard disks need the depth: a scratch on a solid-state drive, filled
// from a RAID of hard disks under ZFS, took its first epoch in at 0.80 of the
// rate of a mapped scratch with 16 MiB at once, and matched it from 64 MiB
// on, as ZFS merged its reads into ones twice as large. A solid-state drive
// gains nothing from more.
#ifndef VITRIO_DEFAULT_SCRATCH_LOAD_BYTE_BUDGET
	#define VITRIO_DEFAULT_SCRATCH_LOAD_BYTE_BUDGET (1UL<<26)
#endif

// Transfers of files the kernel carries out at once for one asynchronous
// queue or port: reads, writes and syncs.
//
// The threads of a file_transfer_pool have this many transfers in flight
// each, and so as many as a disk array needs in all: the reads of scattered
// regions keep the disks busy, and their schedulers order the reads along
// their way. More gains nothing, and where a transfer takes a thread of the
// kernel, as on ZFS, each one in flight costs one. Past it, queueing a
// transfer waits for an earlier one.
#ifndef VITRIO_DEFAULT_FILE_TRANSFER_QUEUE_CAPACITY
	#define VITRIO_DEFAULT_FILE_TRANSFER_QUEUE_CAPACITY 16
#endif

// Threads that carry out the transfers of files of one file_transfer_pool, each
// with a port of its own.
//
// A thread drives many transfers at once, and only queues their reads and
// hands their conversions to the executor of computations. Where the kernel
// copies what a read brings from its cache on the thread that drives the
// ring, as ext4 does, that copying is the thread's work too, and a solid-state
// drive delivers more than one thread copies.
#ifndef VITRIO_DEFAULT_FILE_TRANSFER_WORKERS
	#define VITRIO_DEFAULT_FILE_TRANSFER_WORKERS 4
#endif

// Bytes a read of a ZARR array holds of its chunks at once: those it has read
// and not yet decoded, and those it decodes.
//
// A chunk is read whole and decoded whole, and one of 256 voxels a side of
// floats, as tomograms are chunked, is 64 MiB. The chunks a read holds are
// decoded at once on the executor of computations, so this is what lets
// them keep its threads busy. A read always holds one chunk, however large.
#ifndef VITRIO_DEFAULT_CHUNK_BYTE_BUDGET
	#define VITRIO_DEFAULT_CHUNK_BYTE_BUDGET (1UL<<29)
#endif

// Chunks of a ZARR array a read has open at once, each a file of its own.
//
// Small chunks fit the budget of bytes many at once, and each takes a file
// descriptor, of which a process has a few thousand: several reads at once
// hold several of this.
#ifndef VITRIO_DEFAULT_CHUNKS_IN_FLIGHT
	#define VITRIO_DEFAULT_CHUNKS_IN_FLIGHT 64
#endif

// The width of a chunk of an image of two axes that vitrio writes to ZARR,
// and of a chunk of a volume, which is a cube.
//
// An image of a stack takes chunks of its own, so that one is read without
// another. A chunk of 1024 by 1024 holds a micrograph in a few, and one of
// 128 voxels a side a patch of a tomogram in one or a few, without reading
// much more than it.
#ifndef VITRIO_DEFAULT_ZARR_IMAGE_CHUNK_EDGE
	#define VITRIO_DEFAULT_ZARR_IMAGE_CHUNK_EDGE 1024
#endif

#ifndef VITRIO_DEFAULT_ZARR_VOLUME_CHUNK_EDGE
	#define VITRIO_DEFAULT_ZARR_VOLUME_CHUNK_EDGE 128
#endif

// Bytes of chunks a writer of a ZARR array holds in memory before it stores
// those written least recently.
//
// A chunk is stored once it is whole. A volume written a plane at a time
// leaves every chunk of the planes written so far partial, which this
// bounds: a chunk stored partial is read back when it is written again.
#ifndef VITRIO_DEFAULT_CHUNK_WRITE_BYTE_BUDGET
	#define VITRIO_DEFAULT_CHUNK_WRITE_BYTE_BUDGET (1UL<<30)
#endif
